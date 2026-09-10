set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    Catch2 GIT_REPOSITORY
    "https://github.com/catchorg/Catch2.git" GIT_TAG
    "v3.8.1" GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(FreyaTests test_event_manager.cpp test_freya_options.cpp test_gpu_animation.cpp test_gpu_scene.cpp test_lights.cpp test_limits.cpp test_lighting_technique_registry.cpp test_material_mask.cpp test_mesh_lod.cpp test_sparse_set.cpp test_uniform_buffer.cpp test_window_config_context.cpp
        ${CMAKE_SOURCE_DIR}/src/Freya/Asset/MeshLod.cpp)

target_compile_features(FreyaTests PRIVATE cxx_std_26)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FreyaTests PRIVATE -freflection)
endif()

target_include_directories(
    FreyaTests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
    FreyaTests PRIVATE Catch2::Catch2WithMain glm
    skirnir::skirnir meshoptimizer)

target_precompile_headers(FreyaTests PRIVATE <Freya/Config.hpp>)

add_test(NAME Freya.Unit COMMAND FreyaTests)

option(FREYA_BUILD_GPU_TESTS "Build FreyaGpuTests (requires Vulkan ICD)" OFF)

if(FREYA_BUILD_GPU_TESTS)
    if(NOT TARGET nlohmann_json::nlohmann_json)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif()

    add_executable(FreyaGpuTests
        gpu/test_gpu_cull.cpp
        ${CMAKE_SOURCE_DIR}/Examples/Common/src/CullFrameDumpIo.cpp)

    target_compile_features(FreyaGpuTests PRIVATE cxx_std_26)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(FreyaGpuTests PRIVATE -freflection)
    endif()

    target_include_directories(FreyaGpuTests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/Examples/Common/include)

    target_compile_definitions(FreyaGpuTests PRIVATE
        FREYA_SHADER_ROOT="${CMAKE_BINARY_DIR}/Resources/Shaders"
        FREYA_GPU_FIXTURE_ROOT="${CMAKE_SOURCE_DIR}/tests/fixtures/gpu_cull")

    target_link_libraries(FreyaGpuTests PRIVATE
        Catch2::Catch2WithMain
        Freya::Freya
        nlohmann_json::nlohmann_json
        SDL3::SDL3
        Vulkan::Vulkan)

    target_precompile_headers(FreyaGpuTests PRIVATE
        <vulkan/vulkan.hpp>
        <Freya/Config.hpp>)

    add_dependencies(FreyaGpuTests Shaders)

    add_test(NAME Freya.Gpu COMMAND FreyaGpuTests)
    set_tests_properties(Freya.Gpu PROPERTIES LABELS "gpu")
endif()
