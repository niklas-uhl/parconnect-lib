#ifndef MXX2_DISTRIBUTION_HPP
#define MXX2_DISTRIBUTION_HPP
#include <mxx/distribution.hpp>

namespace mxx {

// non-stable version (much faster, since data exchange is only for unequal
// parts)
template <typename _Iterator>
_Iterator block_decompose_partitions_right(_Iterator begin, _Iterator mid,
                                           _Iterator end,
                                           const mxx::comm &comm) {
  typedef typename std::iterator_traits<_Iterator>::value_type T;
  // return same sequence if there's a single process
  if (comm.size() == 1)
    return mid;
  // get sizes
  size_t right_local_size = std::distance(mid, end);
  size_t right_size = mxx::allreduce(right_local_size, comm);
  blk_dist right_part(
      right_size, comm.size(), comm.rank());
  impl::signed_size_t surplus = (impl::signed_size_t)right_local_size -
                                (impl::signed_size_t)right_part.local_size();
  bool fits = end - begin >= right_part.local_size();
  bool all_fits = mxx::all_of(fits, comm);
  if (!all_fits)
    return mid;
  std::vector<impl::signed_size_t> surpluses = mxx::allgather(surplus, comm);

  MXX_ASSERT(std::accumulate(surpluses.begin(), surpluses.end(), 0) == 0);

  // get send counts
  std::vector<size_t> send_counts =
      impl::surplus_send_pairing(surpluses, comm.size(), comm.rank(), true);

  std::size_t send_size =
      std::accumulate(send_counts.begin(), send_counts.end(), 0);
  std::vector<T> buffer;
  if (send_size > 0)
    buffer.resize(send_size);

  std::vector<size_t> recv_counts = mxx::all2all(send_counts, comm);
  // assert send and receive size are the same!
  for (int i = 0; i < comm.size(); ++i) {
    MXX_ASSERT(send_counts[i] == recv_counts[i]);
  }

  // send from the surplus, receive into buffer
  if (surplus > 0) {
    mxx::all2allv(&(*(mid)), send_counts, &buffer[0], recv_counts, comm);
    std::copy(buffer.begin(), buffer.end(), mid);
  } else if (surplus < 0) {
    mxx::all2allv(&(*(mid + surplus)), send_counts, &buffer[0], recv_counts,
                  comm);
    std::copy(buffer.begin(), buffer.end(), mid + surplus);
  } else {
    MXX_ASSERT(send_size == 0);
    mxx::all2allv((T *)nullptr, send_counts, (T *)nullptr, recv_counts, comm);
  }

  // copy back
  return mid + surplus;
}
} // namespace mxx
#endif // MXX2_DISTRIBUTION_HPP
