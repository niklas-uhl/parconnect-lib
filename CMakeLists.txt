cmake_minimum_required(VERSION 3.23)

project(parconnect-lib)
option(PARCONNECT_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

# The de Bruijn generator is the one part of the library that needs BLISS, and the tools
# additionally need KaGen. Both default to OFF for consumers so that merely linking the
# connectivity algorithm doesn't drag either in.
option(PARCONNECT_ENABLE_DEBRUIJN
       "Build the de Bruijn graph generator (pulls in BLISS)" ${PROJECT_IS_TOP_LEVEL})
option(PARCONNECT_BUILD_TOOLS "Build the command line tools (pulls in KaGen and CLI11)"
       ${PROJECT_IS_TOP_LEVEL})

find_package(MPI REQUIRED)

include(FetchContent)

set(MXX_BUILD_TESTS OFF CACHE BOOL "Build tests")
FetchContent_Declare(
  mxx
  GIT_REPOSITORY https://github.com/niklas-uhl/mxx
  GIT_TAG master
  SYSTEM)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog
  GIT_TAG v1.15.3
  SYSTEM)

FetchContent_Declare(
  plfit
  GIT_REPOSITORY https://github.com/niklas-uhl/plfit
  GIT_TAG main
  EXCLUDE_FROM_ALL
  SYSTEM)

FetchContent_Declare(
  CombBLAS
  GIT_REPOSITORY https://github.com/niklas-uhl/combblas
  GIT_TAG master
  EXCLUDE_FROM_ALL
  SYSTEM)

FetchContent_MakeAvailable(mxx spdlog plfit CombBLAS)

add_library(parconnect INTERFACE)
target_link_libraries(parconnect INTERFACE MPI::MPI_CXX)
target_sources(
  parconnect
  INTERFACE FILE_SET
            HEADERS
            BASE_DIRS
            lib
            FILES
            coloring/labelProp.hpp
            coloring/labelProp_utils.hpp
            coloring/timer.hpp
	    dynamic/degreeDistInfo.hpp
	    bfs/bfsRunner.hpp
	    bfs/timer.hpp
	    graphGen/common/reduceIds.hpp
	    graphGen/common/utils.hpp
	    hash/invertible_hash.hpp
            utils/commonfuncs.hpp
            mxx_extra/sort.hpp
            mxx_extra/distribution.hpp)
	  
target_compile_features(parconnect INTERFACE cxx_std_14)

target_link_libraries(parconnect INTERFACE mxx::mxx spdlog::spdlog plfit CombBLAS)

if(PARCONNECT_ENABLE_DEBRUIJN)
  FetchContent_Declare(
    bliss
    GIT_REPOSITORY https://github.com/niklas-uhl/kmerind
    GIT_TAG main
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
  FetchContent_MakeAvailable(bliss)

  add_library(parconnect-debruijn INTERFACE)
  add_library(parconnect::debruijn ALIAS parconnect-debruijn)
  target_sources(
    parconnect-debruijn
    INTERFACE FILE_SET
              HEADERS
              BASE_DIRS
              lib
              FILES
              graphGen/deBruijn/deBruijnGraphGen.hpp)
  target_link_libraries(parconnect-debruijn INTERFACE parconnect bliss::bliss)
endif()

if(PARCONNECT_BUILD_TOOLS)
  if(NOT PARCONNECT_ENABLE_DEBRUIJN)
    message(FATAL_ERROR "PARCONNECT_BUILD_TOOLS requires PARCONNECT_ENABLE_DEBRUIJN")
  endif()
  add_subdirectory(tools)
endif()

if(PARCONNECT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
