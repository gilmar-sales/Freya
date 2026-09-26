set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
        googletest GIT_REPOSITORY
        "https://github.com/google/googletest.git" GIT_TAG
        "v1.17.0" GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(FreyaTests test_billboard_draw.cpp test_event_manager.cpp test_freya_options.cpp test_gpu_animation.cpp test_gpu_scene.cpp test_lights.cpp test_limits.cpp test_lighting_technique_registry.cpp test_material_mask.cpp test_mesh_lod.cpp test_particle_emitter.cpp test_sparse_set.cpp test_spinlock.cpp test_uniform_buffer.cpp test_window_config_context.cpp test_ui_draw.cpp test_ui_layout.cpp
        font_atlas_stub.cpp
        ui_model_preview_stub.cpp)

target_compile_features(FreyaTests PRIVATE cxx_std_26)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FreyaTests PRIVATE -freflection)
endif ()

target_include_directories(
        FreyaTests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
        FreyaTests PRIVATE GTest::gtest_main Freya::Freya)

target_precompile_headers(FreyaTests PRIVATE <Freya/Config.hpp>)

gtest_discover_tests(FreyaTests)

option(FREYA_BUILD_GPU_TESTS "Build FreyaGpuTests (requires Vulkan ICD)" OFF)

if (FREYA_BUILD_GPU_TESTS)
    if (NOT TARGET nlohmann_json::nlohmann_json)
        FetchContent_Declare(
                nlohmann_json
                GIT_REPOSITORY https://github.com/nlohmann/json.git
                GIT_TAG v3.11.3
                GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(nlohmann_json)
    endif ()

    add_executable(FreyaGpuTests
            gpu/test_gpu_cull.cpp
            ${CMAKE_SOURCE_DIR}/Examples/Common/src/CullFrameDumpIo.cpp)

    target_compile_features(FreyaGpuTests PRIVATE cxx_std_26)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(FreyaGpuTests PRIVATE -freflection)
    endif ()

    target_include_directories(FreyaGpuTests PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/Examples/Common/include)

    target_compile_definitions(FreyaGpuTests PRIVATE
            FREYA_SHADER_ROOT="${CMAKE_BINARY_DIR}/Resources/Shaders"
            FREYA_GPU_FIXTURE_ROOT="${CMAKE_SOURCE_DIR}/tests/fixtures/gpu_cull")

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

    gtest_discover_tests(FreyaGpuTests PROPERTIES LABELS "gpu")
endif ()
