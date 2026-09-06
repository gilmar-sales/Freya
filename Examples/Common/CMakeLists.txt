add_library(FreyaExamplesCommon STATIC
    src/FlyCam.cpp
    src/GroundMesh.cpp
)

add_library(FreyaExamples::Common ALIAS FreyaExamplesCommon)

target_include_directories(FreyaExamplesCommon
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(FreyaExamplesCommon PUBLIC cxx_std_26)

target_link_libraries(FreyaExamplesCommon PUBLIC Freya::Freya)
