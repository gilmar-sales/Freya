# cmake/FetchFidelityFX.cmake
#
# Fetches AMD FidelityFX SDK 1.1.4 (FSR Upscaler 3.1.4) via the Linux-friendly
# DethRaid/FidelityFX-SDK-Linux fork and builds only the Vulkan backend +
# FSR3 upscaler as static libraries. Official PrebuiltSignedDLL is Windows/MSVC
# only; Freya CI uses GCC/MinGW on both platforms, so we link the host API
# statically (same shader permutations as the FidelityFX API DLL).
#
# Override the source tree with:
#   -DFETCHCONTENT_SOURCE_DIR_FIDELITYFX_SDK=/path/to/clone

include(FetchContent)
include(ExternalProject)

set(FREYA_FIDELITYFX_GIT_TAG "main" CACHE STRING
    "DethRaid/FidelityFX-SDK-Linux git tag (FidelityFX SDK 1.1.4 + Linux)")

message(STATUS "FidelityFX: fetching SDK (${FREYA_FIDELITYFX_GIT_TAG})")

FetchContent_Declare(
    fidelityfx_sdk
    GIT_REPOSITORY "https://github.com/DethRaid/FidelityFX-SDK-Linux.git"
    GIT_TAG        "${FREYA_FIDELITYFX_GIT_TAG}"
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(fidelityfx_sdk)
if(NOT fidelityfx_sdk_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(fidelityfx_sdk)
endif()

set(FFX_SDK_ROOT "${fidelityfx_sdk_SOURCE_DIR}" CACHE INTERNAL
    "FidelityFX SDK root")
set(FFX_INCLUDE_DIR "${FFX_SDK_ROOT}/sdk/include" CACHE INTERNAL
    "FidelityFX host/gpu include dir")

if(NOT EXISTS "${FFX_INCLUDE_DIR}/FidelityFX/host/ffx_fsr3upscaler.h")
    message(FATAL_ERROR
        "FidelityFX SDK: headers missing at ${FFX_INCLUDE_DIR}. "
        "Fetch may have failed.")
endif()

set(_ffx_ep_bin "${FFX_SDK_ROOT}/sdk/bin/ffx_sdk")
set(_ffx_lib_fsr "${_ffx_ep_bin}/libffx_fsr3upscaler_x64.a")
set(_ffx_lib_vk "${_ffx_ep_bin}/libffx_backend_vk_x64.a")
if(MSVC)
    set(_ffx_lib_fsr "${_ffx_ep_bin}/ffx_fsr3upscaler_x64.lib")
    set(_ffx_lib_vk "${_ffx_ep_bin}/ffx_backend_vk_x64.lib")
endif()

ExternalProject_Add(
    ffx_sdk_build
    SOURCE_DIR "${FFX_SDK_ROOT}/sdk"
    BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/ffx_sdk_build"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DFFX_API_BACKEND=VK_X64
        -DFFX_FSR3UPSCALER=ON
        -DFFX_ALL=OFF
        -DFFX_AUTO_COMPILE_SHADERS=ON
        -DFFX_BUILD_AS_DLL=OFF
    BUILD_BYPRODUCTS "${_ffx_lib_fsr}" "${_ffx_lib_vk}"
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
    BUILD_ALWAYS FALSE
)

add_library(ffx_fsr3upscaler STATIC IMPORTED GLOBAL)
add_library(ffx_backend_vk STATIC IMPORTED GLOBAL)
set_target_properties(ffx_fsr3upscaler PROPERTIES
    IMPORTED_LOCATION "${_ffx_lib_fsr}")
set_target_properties(ffx_backend_vk PROPERTIES
    IMPORTED_LOCATION "${_ffx_lib_vk}")
add_dependencies(ffx_fsr3upscaler ffx_sdk_build)
add_dependencies(ffx_backend_vk ffx_sdk_build)

add_library(FidelityFX_FSR3 INTERFACE)
add_library(FidelityFX::FSR3 ALIAS FidelityFX_FSR3)
target_include_directories(FidelityFX_FSR3 INTERFACE "${FFX_INCLUDE_DIR}")
target_link_libraries(FidelityFX_FSR3 INTERFACE
    ffx_fsr3upscaler
    ffx_backend_vk
    ${Vulkan_LIBRARIES})

message(STATUS "FidelityFX: include = ${FFX_INCLUDE_DIR}")
message(STATUS "FidelityFX: fsr3     = ${_ffx_lib_fsr}")
message(STATUS "FidelityFX: vk       = ${_ffx_lib_vk}")
