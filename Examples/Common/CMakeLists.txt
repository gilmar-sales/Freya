add_library(FreyaExamplesCommon STATIC
    src/FlyCam.cpp
    src/GroundMesh.cpp
    src/DebugOverlay.cpp
    src/CullFrameDumpIo.cpp
)

add_library(FreyaExamples::Common ALIAS FreyaExamplesCommon)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(imgui_freya STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
)
target_include_directories(imgui_freya PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)
target_compile_features(imgui_freya PUBLIC cxx_std_17)
target_link_libraries(imgui_freya PUBLIC SDL3::SDL3 Vulkan::Vulkan)
target_compile_options(imgui_freya PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-UVK_NO_PROTOTYPES>
)
target_compile_options(FreyaExamplesCommon PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-UVK_NO_PROTOTYPES>
)

target_include_directories(FreyaExamplesCommon
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(FreyaExamplesCommon PUBLIC cxx_std_26)

target_link_libraries(FreyaExamplesCommon
    PUBLIC Freya::Freya imgui_freya nlohmann_json::nlohmann_json
    PRIVATE Vulkan::Vulkan SDL3::SDL3
)
