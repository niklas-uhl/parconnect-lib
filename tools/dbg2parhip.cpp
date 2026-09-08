/**
 * @file    dbg2parhip.cpp
 * @brief   Builds the de Bruijn graph of DNA read files and writes it out as ParHiP.
 *
 * This is ParConnect's `utils_exportBinaryFormat` reduced to its de Bruijn input mode,
 * with the ad-hoc per-rank binary edge list output replaced by a direct KaGen ParHiP
 * write. The point is that the result can be fed to any KaGen consumer as
 * `file;filename=<out>;input_format=parhip`, so the same graph reaches every algorithm
 * through the same code path.
 */

#include <mpi.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "graphGen/common/reduceIds.hpp"
#include "graphGen/deBruijn/deBruijnGraphGen.hpp"

#include <mxx/collective.hpp>
#include <mxx/comm.hpp>
#include <mxx/reduction.hpp>

#include <CLI/CLI.hpp>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <kagen.h>
#include <kagen/io.h>

namespace {

using VId  = std::int64_t;
using Edge = std::pair<VId, VId>;

//! First vertex of rank `r` under the balanced partition of [0, n) into `p` blocks.
VId block_begin(const std::size_t n, const int r, const int p) {
  return static_cast<VId>((static_cast<std::size_t>(r) * n) / static_cast<std::size_t>(p));
}

//! Inverse of block_begin: the rank owning vertex `v`.
int block_owner(const std::size_t n, const VId v, const int p) {
  return static_cast<int>(((static_cast<std::size_t>(v) + 1) * static_cast<std::size_t>(p) - 1) / n);
}

std::size_t global_sum(const std::size_t local, const mxx::comm& comm) {
  return mxx::allreduce(local, std::plus<std::size_t>(), comm);
}

//! Lower case extension of `path`, without the dot.
std::string extension(const std::string& path) {
  const auto  dot = path.rfind('.');
  std::string ext = dot == std::string::npos ? std::string{} : path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

} // namespace

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  int exit_code = 0;
  {
    mxx::comm comm;

    CLI::App app{
        "Build the k=31 de Bruijn graph of DNA read files and write it as a ParHiP graph."};

    // BLISS dispatches its sequence parser on the file extension, and the generator fixes
    // that parser to FASTQ, so a .fasta input makes build() throw on every rank. Reject it
    // here, where the message is readable.
    const auto fastq_file = [](const std::string& f) -> std::string {
      return extension(f) == "fastq" ? std::string{} : "not a .fastq file: " + f;
    };

    // Both files are named options: with a variadic input list, a positional output is
    // ambiguous, and forgetting it would silently overwrite the last input.
    std::vector<std::string> inputs;
    std::string              output;
    app.add_option("-i,--input", inputs,
                   "Input sequence files. Must have a .fastq extension. Multiple files are "
                   "built into a single graph, exactly as their concatenation would be.")
        ->required()
        ->check(CLI::ExistingFile)
        ->check(fastq_file, "FASTQ");
    app.add_option("-o,--output", output, "Output path for the ParHiP graph.")->required();

    bool quiet = false;
    app.add_flag("-q,--quiet", quiet, "Only report errors.");

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
      exit_code = app.exit(e);
      MPI_Finalize();
      return exit_code;
    }

    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::set_level(quiet ? spdlog::level::warn : spdlog::level::info);
    const auto log = [&](const std::string& msg) {
      if (comm.rank() == 0) {
        spdlog::info(msg);
      }
    };

    spdlog::stopwatch total;

    // ---------------------------------------------------------------- generate
    // Vertex ids are 2-bit packed canonical 31-mers, hence sparse in [0, 2^62). Every
    // k-mer emits its own incident edges, so a clean read set yields a symmetric edge
    // list, but see the symmetrisation below for why that cannot be relied upon.
    spdlog::stopwatch phase;
    std::vector<Edge> edges;
    {
      conn::graphGen::deBruijnGraph generator;
      generator.populateEdgeList(edges, inputs, comm);
    }
    const std::size_t directed_edges = global_sum(edges.size(), comm);
    log(fmt::format("Built de Bruijn graph of {}: {} directed edges in {:.3}s",
                    fmt::join(inputs, ", "), directed_edges, phase));

    if (directed_edges < static_cast<std::size_t>(comm.size())) {
      // reduceVertexIds dereferences edgeList.back() on every rank, so it cannot cope
      // with a rank that owns no edge at all.
      if (comm.rank() == 0) {
        spdlog::error("Graph has {} edges but there are {} ranks; use fewer ranks.",
                      directed_edges, comm.size());
      }
      MPI_Abort(comm, 1);
    }

    // ---------------------------------------------------------------- symmetrise
    // BLISS stores a k-mer's incident edges as a DNA16 bit mask of the neighbouring
    // base, and DNA16 encodes 'N' as 0b1111. A k-mer that sits next to an 'N' in a read
    // therefore claims a neighbour for all four bases, while the only neighbour that is
    // ever inserted as a vertex is the one that reads the 'N' as an 'A' (the 2-bit DNA
    // alphabet maps every non-ACGT character to 'A'). Those three phantom edges have no
    // counterpart at the other endpoint, so the raw edge list of a read set containing
    // 'N' is *not* symmetric.
    //
    // That has to be repaired here, before relabelling: reduceVertexIds() numbers the
    // source and the target layer independently and its two numberings only agree when
    // both layers contain the same set of vertices. A single one-sided edge shifts the
    // two numberings apart and scrambles the whole graph, not just that one edge.
    //
    // Duplicates are not a problem, they are removed by the repartitioning below.
    phase.reset();
    {
      const std::size_t forward = edges.size();
      edges.reserve(2 * forward);
      for (std::size_t i = 0; i < forward; ++i) {
        edges.emplace_back(edges[i].second, edges[i].first);
      }
    }
    log(fmt::format("Symmetrised to {} directed edges in {:.3}s", global_sum(edges.size(), comm),
                    phase));

    // ------------------------------------------------------------- relabel ids
    // Compacts the packed-kmer ids to [0, n). The edge list is symmetric, so the
    // independent numberings this assigns to the source and target layers agree.
    // Leaves the edge list globally sorted by source, block-distributed by edge count.
    phase.reset();
    std::size_t n = 0;
    conn::graphGen::reduceVertexIds(edges, n, comm);
    log(fmt::format("Relabelled to {} contiguous vertex ids in {:.3}s", n, phase));

    // ------------------------------------------------------- repartition by vertex
    // ParHiP is written rank by rank into a single file, so each rank has to hold a
    // contiguous vertex block *and* every edge of the vertices in it. The relabelling
    // above balances edges, which can split a vertex across two ranks.
    phase.reset();
    const VId my_begin = block_begin(n, comm.rank(), comm.size());
    const VId my_end   = block_begin(n, comm.rank() + 1, comm.size());

    std::sort(edges.begin(), edges.end());
    std::vector<std::size_t> send_counts(comm.size(), 0);
    for (const auto& e : edges) {
      ++send_counts[block_owner(n, e.first, comm.size())];
    }
    edges = mxx::all2allv(edges, send_counts, comm);

    std::sort(edges.begin(), edges.end());
    const std::size_t received = edges.size();
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    edges.erase(std::remove_if(edges.begin(), edges.end(),
                               [](const Edge& e) { return e.first == e.second; }),
                edges.end());
    const std::size_t dropped = global_sum(received - edges.size(), comm);
    log(fmt::format("Repartitioned by vertex in {:.3}s, dropped {} duplicate/self edges",
                    phase, dropped));

    // ------------------------------------------------------------------- write
    phase.reset();
    kagen::Graph graph;
    graph.representation = kagen::GraphRepresentation::EDGE_LIST;
    graph.vertex_range   = {static_cast<kagen::SInt>(my_begin), static_cast<kagen::SInt>(my_end)};
    graph.edges.reserve(edges.size());
    for (const auto& e : edges) {
      graph.edges.emplace_back(static_cast<kagen::SInt>(e.first),
                               static_cast<kagen::SInt>(e.second));
    }
    edges = {};

    kagen::OutputGraphConfig config;
    config.filename    = output;
    config.formats     = {kagen::FileFormat::PARHIP};
    config.extension   = false;
    config.distributed = false;

    const kagen::GraphInfo info(graph, comm);
    const auto&            factory = kagen::GetGraphFormatFactory(kagen::FileFormat::PARHIP);
    auto                   writer  = factory->CreateWriter(config, graph, info, comm.rank(), comm.size());
    kagen::WriteGraph(*writer, config, /*output=*/!quiet && comm.rank() == 0, comm);
    log(fmt::format("Wrote {} ({} vertices, {} undirected edges) in {:.3}s",
                    output, info.global_n, info.global_m / 2, phase));
    log(fmt::format("Done in {:.3}s", total));
  }

  MPI_Finalize();
  return exit_code;
}
