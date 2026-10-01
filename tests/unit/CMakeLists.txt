file(GLOB_RECURSE FREYA_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*Spec.cpp")

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
