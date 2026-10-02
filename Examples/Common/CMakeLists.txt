add_library(FreyaExamplesCommon STATIC
    src/FlyCam.cpp
    src/GroundMesh.cpp
    src/DebugOverlay.cpp
    src/CullFrameDumpIo.cpp
    src/CullAabbDebugDraw.cpp
)

add_library(FreyaExamples::Common ALIAS FreyaExamplesCommon)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

target_compile_options(FreyaExamplesCommon PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-UVK_NO_PROTOTYPES>
)

target_include_directories(FreyaExamplesCommon
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(FreyaExamplesCommon PUBLIC cxx_std_26)

target_link_libraries(FreyaExamplesCommon
    PUBLIC Freya::Freya nlohmann_json::nlohmann_json
    PRIVATE Vulkan::Vulkan SDL3::SDL3
)
