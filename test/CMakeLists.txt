FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0)
FetchContent_Declare(
  KaTestrophe
  GIT_REPOSITORY https://github.com/kamping-site/KaTestrophe.cmake.git
  GIT_TAG main)

FetchContent_MakeAvailable(googletest KaTestrophe)

katestrophe_add_test_executable(test_ccl_coloring FILES test_ccl_coloring.cpp)
target_link_libraries(test_ccl_coloring PRIVATE parconnect)
katestrophe_add_mpi_test(test_ccl_coloring CORES 2 4 8)
