if(NOT TARGET CLI11::CLI11)
  FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11
    GIT_TAG v2.5.0
    GIT_SHALLOW ON
    SYSTEM)
  FetchContent_MakeAvailable(cli11)
endif()

if(NOT TARGET KaGen::KaGen)
  set(KAGEN_USE_SPARSEHASH OFF)
  FetchContent_Declare(
    kagen
    GIT_REPOSITORY https://github.com/KarlsruheGraphGeneration/KaGen
    GIT_TAG feature/true-edge-balance-redistribution
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
  FetchContent_MakeAvailable(kagen)
endif()

add_executable(dbg2parhip dbg2parhip.cpp)
target_link_libraries(dbg2parhip PRIVATE parconnect::debruijn CLI11::CLI11 KaGen::KaGen)
