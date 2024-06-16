cmake_minimum_required(VERSION 3.20)

project(
    Freya 
    LANGUAGES C CXX
)

find_package(Vulkan REQUIRED)

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


if(MSVC)
  target_compile_options(Freya PUBLIC "/ZI" "/MP")
  target_link_options(Freya PUBLIC "/INCREMENTAL")
endif()

target_compile_features(Freya PUBLIC cxx_std_23)
target_include_directories(Freya PUBLIC Include ${Vulkan_INCLUDE_DIRS})
target_link_libraries(Freya SDL3::SDL3 glm assimp ${Vulkan_LIBRARIES})

target_precompile_headers(Freya  PUBLIC
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
    Include/Macro.hpp
)

endif(BUILD_SHARED_LIBS)

add_library(Freya-static STATIC
${Asset_SRCS}
${Builders_SRCS}
${Core_SRCS}
${Factories_SRCS}
${Shaders_SRCS}
)

if(MSVC)
  target_compile_options(Freya-static  PUBLIC "/ZI" "/MP")
  target_link_options(Freya-static  PUBLIC "/INCREMENTAL")
endif()

target_compile_features(Freya-static  PUBLIC cxx_std_23)

target_include_directories(Freya-static  PUBLIC Include ${Vulkan_INCLUDE_DIRS})

target_link_libraries(Freya-static  SDL3::SDL3 glm assimp ${Vulkan_LIBRARIES})

target_precompile_headers(Freya-static  PUBLIC
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
    Include/Macro.hpp
)

add_executable(Main Examples/Main.cpp)

target_compile_features(Main PUBLIC cxx_std_23)

target_link_libraries(Main Freya-static) 

if(MSVC)
  target_compile_options(Main PUBLIC "/ZI" "/MP")
  target_link_options(Main PUBLIC "/INCREMENTAL")
endif()


file(COPY Shaders DESTINATION ${CMAKE_BINARY_DIR})

set(BUILD_ASSIMP_TESTS OFF CACHE BOOL "Disable assimp tests build")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Shared Library")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable assimp warnings as errors")

add_subdirectory(ThirdParty/SDL)
add_subdirectory(ThirdParty/glm)
add_subdirectory(ThirdParty/assimp)
