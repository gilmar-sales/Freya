set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    Catch2 GIT_REPOSITORY
    "https://github.com/catchorg/Catch2.git" GIT_TAG
    "v3.8.1" GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(FreyaTests test_event_manager.cpp test_freya_options.cpp test_gpu_animation.cpp test_gpu_scene.cpp test_lights.cpp test_limits.cpp test_lighting_technique_registry.cpp test_material_mask.cpp test_sparse_set.cpp test_uniform_buffer.cpp)

target_compile_features(FreyaTests PRIVATE cxx_std_26)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FreyaTests PRIVATE -freflection)
endif()

target_include_directories(
    FreyaTests PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
    FreyaTests PRIVATE Catch2::Catch2WithMain glm
    skirnir::skirnir)

target_precompile_headers(FreyaTests PRIVATE <Freya/Config.hpp>)

add_test(NAME Freya.Unit COMMAND FreyaTests)
