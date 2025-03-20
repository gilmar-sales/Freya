cmake_minimum_required(VERSION 3.25)

project(
        Freya
        LANGUAGES C CXX
)

option(FREYA_BUILD_EXAMPLES "Build freyr examples" OFF)

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
GIT_TAG        "release-3.2.8"
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
GIT_TAG        "main"
)
FetchContent_MakeAvailable(skirnir)

file(GLOB_RECURSE Asset_SRCS Source/Asset/**.cpp Include/Asset/**.hpp)
file(GLOB_RECURSE Builders_SRCS Source/Builders/**.cpp Include/Builders/**.hpp)
file(GLOB_RECURSE Core_SRCS Source/Core/**.cpp Include/Core/**.hpp)
file(GLOB_RECURSE Containers_SRCS Include/Containers/**.hpp)
file(GLOB_RECURSE Factories_SRCS Source/Factories/**.cpp Include/Factories/**.hpp)
file(GLOB_RECURSE Shaders_SRCS Shaders/**.vert Shaders/**.frag)

SOURCE_GROUP(Asset FILES ${Asset_SRCS})
SOURCE_GROUP(Builders FILES ${Builders_SRCS})
SOURCE_GROUP(Core FILES ${Core_SRCS})
SOURCE_GROUP(Containers FILES ${Containers_SRCS})
SOURCE_GROUP(Factories FILES ${Factories_SRCS})
SOURCE_GROUP(Shaders FILES ${Shaders_SRCS})

if (BUILD_SHARED_LIBS)
    add_library(Freya
            ${Asset_SRCS}
            ${Builders_SRCS}
            ${Core_SRCS}
            ${Factories_SRCS}
            ${Shaders_SRCS}
    )


    if (MSVC)
        target_compile_options(Freya PUBLIC "/ZI" "/MP")
        target_link_options(Freya PUBLIC "/INCREMENTAL")
    endif ()

    target_compile_features(Freya PUBLIC cxx_std_23)
    target_include_directories(Freya PUBLIC Include ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(Freya SDL3::SDL3 glm assimp ${Vulkan_LIBRARIES} skirnir::skirnir)

    target_precompile_headers(Freya PUBLIC
            <memory>
            <limits>
            <cstdint>
            <string>
            <vector>
            <set>
            <print>
            <fstream>
            <vulkan/vulkan.hpp>
            <SDL3/SDL.h>
            <SDL3/SDL_vulkan.h>
            Include/Freya/Freya.hpp
    )

    if (MINGW)
        target_link_libraries(Freya stdc++exp)
    endif (MINGW)
endif (BUILD_SHARED_LIBS)

add_library(Freya-static STATIC
        ${Asset_SRCS}
        ${Builders_SRCS}
        ${Core_SRCS}
        ${Factories_SRCS}
        ${Shaders_SRCS}
)

if (MSVC)
    target_compile_options(Freya-static PUBLIC "/ZI" "/MP")
    target_link_options(Freya-static PUBLIC "/INCREMENTAL")
endif ()

target_compile_features(Freya-static PUBLIC cxx_std_23)
target_include_directories(Freya-static PUBLIC Include ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Freya-static SDL3::SDL3 glm assimp ${Vulkan_LIBRARIES} skirnir::skirnir)

if (MINGW)
    target_link_libraries(Freya-static stdc++exp)
endif (MINGW)

target_precompile_headers(Freya-static PUBLIC
        <memory>
        <limits>
        <cstdint>
        <string>
        <vector>
        <set>
        <iostream>
        <fstream>
        <vulkan/vulkan.hpp>
        <SDL3/SDL.h>
        <SDL3/SDL_vulkan.h>
        Include/Freya/Freya.hpp
)


if (FREYA_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif (FREYA_BUILD_EXAMPLES)


file(COPY Shaders DESTINATION ${CMAKE_BINARY_DIR}/Resources)