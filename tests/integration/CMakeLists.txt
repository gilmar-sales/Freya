option(FREYA_BUILD_GPU_TESTS
        "Build Freya GPU integration tests (requires Vulkan ICD)" OFF)

if (NOT FREYA_BUILD_GPU_TESTS)
    return()
endif ()

if (NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
endif ()

add_executable(FreyaGpuTests
        Rendering/GpuCullSpec.cpp
        ${PROJECT_SOURCE_DIR}/Examples/Common/src/CullFrameDumpIo.cpp)

target_compile_features(FreyaGpuTests PRIVATE cxx_std_26)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FreyaGpuTests PRIVATE -freflection)
endif ()

target_include_directories(FreyaGpuTests PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/Examples/Common/include)

target_compile_definitions(FreyaGpuTests PRIVATE
        FREYA_SHADER_ROOT="${CMAKE_BINARY_DIR}/Resources/Shaders"
        FREYA_GPU_FIXTURE_ROOT="${PROJECT_SOURCE_DIR}/tests/fixtures/gpu_cull")

target_link_libraries(FreyaGpuTests PRIVATE
        GTest::gtest_main
        Freya::Freya
        nlohmann_json::nlohmann_json
        SDL3::SDL3
        Vulkan::Vulkan)

target_precompile_headers(FreyaGpuTests PRIVATE
        <vulkan/vulkan.hpp>
        <Freya/Config.hpp>)

add_dependencies(FreyaGpuTests Shaders)
gtest_discover_tests(FreyaGpuTests PROPERTIES LABELS "integration;gpu")
