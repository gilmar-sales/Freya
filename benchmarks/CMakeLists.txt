set(BENCHMARK_ENABLE_TESTING OFF)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF)
set(BENCHMARK_ENABLE_INSTALL OFF)

include(FetchContent)
FetchContent_Declare(
    google-benchmark
    GIT_REPOSITORY "https://github.com/google/benchmark.git"
    GIT_TAG "v1.9.5"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(google-benchmark)

add_subdirectory(Animation)
