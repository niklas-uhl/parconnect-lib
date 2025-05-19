cmake_minimum_required(VERSION 3.10)

add_library(parconnect)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        1.17.0
)
FetchContent_MakeAvailable(googletest)

add_executable(test_ccl_coloring 
    test_ccl_coloring_test.cpp
)
target_link_libraries(test_ccl_coloring
  PRIVATE
  parconnect
  googletest::gtest
)
