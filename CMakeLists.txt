cmake_minimum_required(VERSION 3.23)

project(parconnect-lib)
option(PARCONNECT_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})

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
  SYSTEM)

FetchContent_MakeAvailable(mxx spdlog plfit)

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
            utils/commonfuncs.hpp
            mxx_extra/sort.hpp
            mxx_extra/distribution.hpp)
	  
target_compile_features(parconnect INTERFACE cxx_std_14)

target_link_libraries(parconnect INTERFACE mxx spdlog::spdlog plfit)

if(PARCONNECT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
