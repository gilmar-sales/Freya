add_library(FreyaExamplesCommon STATIC
    src/FlyCam.cpp
    src/GroundMesh.cpp
    src/DebugOverlay.cpp
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
# Do NOT define IMGUI_IMPL_VULKAN_NO_PROTOTYPES (even to 0): #ifdef treats
# any definition as enabled and then Init requires LoadFunctions().
# Undefine VK_NO_PROTOTYPES if a dependency injected it.
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
    PUBLIC Freya::Freya imgui_freya
    PRIVATE Vulkan::Vulkan SDL3::SDL3
)
