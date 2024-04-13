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
file(GLOB_RECURSE Shaders_SRCS Shaders/**.vert Shaders/**.frag)

SOURCE_GROUP(Asset FILES ${Asset_SRCS})
SOURCE_GROUP(Builders FILES ${Builders_SRCS})
SOURCE_GROUP(Core FILES ${Core_SRCS})
SOURCE_GROUP(Containers FILES ${Containers_SRCS})
SOURCE_GROUP(Shaders FILES ${Shaders_SRCS})

add_library(Freya 
${Asset_SRCS}
${Builders_SRCS}
${Core_SRCS}
${Shaders_SRCS}
)

add_library(Freya-static STATIC
${Asset_SRCS}
${Builders_SRCS}
${Core_SRCS}
${Shaders_SRCS}
)

if(MSVC)
  target_compile_options(Freya PUBLIC "/ZI" "/MP")
  target_link_options(Freya PUBLIC "/INCREMENTAL")

  target_compile_options(Freya-static  PUBLIC "/ZI" "/MP")
  target_link_options(Freya-static  PUBLIC "/INCREMENTAL")
endif()

target_compile_features(Freya PUBLIC cxx_std_23)
target_compile_features(Freya-static  PUBLIC cxx_std_23)

target_include_directories(Freya PUBLIC Include ${Vulkan_INCLUDE_DIRS})
target_include_directories(Freya-static  PUBLIC Include ${Vulkan_INCLUDE_DIRS})

target_link_libraries(Freya SDL3-shared glm assimp ${Vulkan_LIBRARIES})
target_link_libraries(Freya-static  SDL3-static glm assimp ${Vulkan_LIBRARIES})

target_precompile_headers(Freya  PUBLIC
    <memory>
    <limits>
    <cstdint>
    <string>
    <vector>
    <set>
    <iostream>
    <fstream>
    <SDL3/SDL.h>
    <SDL3/SDL_vulkan.h>
    <vulkan/vulkan.hpp>
    Include/Macro.hpp
)

target_precompile_headers(Freya-static  PUBLIC
    <memory>
    <limits>
    <cstdint>
    <string>
    <vector>
    <set>
    <iostream>
    <fstream>
    <SDL3/SDL.h>
    <SDL3/SDL_vulkan.h>
    <vulkan/vulkan.hpp>
    Include/Macro.hpp
)

add_executable(Main Examples/Main.cpp)

target_compile_features(Main PUBLIC cxx_std_23)

target_link_libraries(Main Freya) 

if(MSVC)
  target_compile_options(Main PUBLIC "/ZI" "/MP")
  target_link_options(Main PUBLIC "/INCREMENTAL")
endif()


file(COPY Shaders DESTINATION ${CMAKE_BINARY_DIR})

add_subdirectory(Thirdparty/SDL)
add_subdirectory(Thirdparty/glm)
add_subdirectory(Thirdparty/assimp)
