cmake_minimum_required(VERSION 3.23)

project(parconnect-lib)
option(PARCONNECT_BUILD_TESTS "Build tests" ${IS_TOP_LEVEL_PROJECT})

find_package(MPI REQUIRED)

include(FetchContent)

FetchContent_Declare(
  mxx
  GIT_REPOSITORY https://github.com/patflick/mxx
  GIT_TAG master
  SYSTEM)

FetchContent_MakeAvailable(mxx)

add_library(easyloggingpp ext/easylogging++.cc)
target_sources(easyloggingpp PUBLIC FILE_SET HEADERS BASE_DIRS ext FILES
                                    ext/easylogging++.h)
target_compile_features(easyloggingpp PUBLIC cxx_std_11)

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
            utils/commonfuncs.hpp
            mxx_extra/sort.hpp
            mxx_extra/distribution.hpp)
	  
target_compile_features(parconnect INTERFACE cxx_std_14)

target_link_libraries(parconnect INTERFACE mxx easyloggingpp)

if(PARCONNECT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
