file(GLOB_RECURSE FREYA_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*Spec.cpp")

# Scene.cpp also contains Upload(), which pulls the renderer implementation
# from the static library. Keep it separate from the UI replacement symbols.
list(REMOVE_ITEM FREYA_UNIT_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Scene/SceneSpec.cpp")

add_executable(FreyaTests
        ${FREYA_UNIT_TEST_SOURCES}
        stubs/FontAtlasStub.cpp
        stubs/UiModelPreviewStub.cpp)

target_compile_features(FreyaTests PRIVATE cxx_std_26)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FreyaTests PRIVATE -freflection)
endif ()

target_include_directories(FreyaTests PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(FreyaTests PRIVATE GTest::gtest_main Freya::Freya)
target_precompile_headers(FreyaTests PRIVATE <Freya/Config.hpp>)

gtest_discover_tests(FreyaTests PROPERTIES LABELS "unit")

add_executable(FreyaSceneTests Scene/SceneSpec.cpp)
target_compile_features(FreyaSceneTests PRIVATE cxx_std_26)
target_link_libraries(FreyaSceneTests PRIVATE GTest::gtest_main Freya::Freya)
target_precompile_headers(FreyaSceneTests PRIVATE <Freya/Config.hpp>)
gtest_discover_tests(FreyaSceneTests PROPERTIES LABELS "unit")
