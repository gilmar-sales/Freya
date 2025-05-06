cmake_minimum_required(VERSION 3.29)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Freya CXX)

find_package(Vulkan REQUIRED)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable assimp tests build")
set(SDL_TEST OFF CACHE BOOL "Disable SDL2 tests build")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Disable SDL3 tests build")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Shared Library")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable assimp warnings as errors")

include(FetchContent)

FetchContent_Declare(
SDL
GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
GIT_TAG        "release-3.2.12"
)
FetchContent_MakeAvailable(SDL)

FetchContent_Declare(
glm
GIT_REPOSITORY "https://github.com/g-truc/glm.git"
GIT_TAG        "1.0.1"
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
assimp
GIT_REPOSITORY "https://github.com/assimp/assimp.git"
GIT_TAG        "v5.4.3"
)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(
skirnir
GIT_REPOSITORY "https://github.com/gilmar-sales/Skirnir.git"
GIT_TAG        "v0.9.0"
)
FetchContent_MakeAvailable(skirnir)

option(FREYA_BUILD_EXAMPLES "Build freyr examples" OFF)

file(GLOB_RECURSE Freya_SRCS src/**.cpp)

add_library(Freya ${Freya_SRCS})

add_library(Freya::Freya ALIAS Freya)

target_compile_features(Freya PUBLIC cxx_std_23)
target_include_directories(Freya PUBLIC include src ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Freya SDL3::SDL3 glm assimp ${Vulkan_LIBRARIES} skirnir::skirnir)

target_precompile_headers(Freya PUBLIC
        <memory>
        <limits>
        <cstdint>
        <string>
        <vector>
        <set>
        <iostream>
        <fstream>
        <vulkan/vulkan.hpp>
        <vulkan/vulkan_enums.hpp>
        <SDL3/SDL.h>
        <SDL3/SDL_events.h>
        <SDL3/SDL_vulkan.h>
        <Skirnir/Skirnir.hpp>
        <Freya/Pch.hpp>
)

if (FREYA_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif (FREYA_BUILD_EXAMPLES)


file(COPY Shaders DESTINATION ${CMAKE_BINARY_DIR}/Resources)