# Fetch Intel XeSS SDK (headers + prebuilt Windows runtime).
#
# The published SDK tag has no CMakeLists.txt and no Linux .so — only
# bin/libxess.dll + lib/libxess.lib. This module creates an IMPORTED
# `xess` target on WIN32 and sets FREYA_HAS_XESS accordingly.

include(FetchContent)

set(XESS_BUILD_EXAMPLES OFF CACHE BOOL "Disable XeSS samples" FORCE)
set(XESS_BUILD_TESTS OFF CACHE BOOL "Disable XeSS tests" FORCE)

option(FREYA_ENABLE_XESS
        "Enable Intel XeSS-SR (Windows SDK runtime required)" ON)

FetchContent_Declare(
        intel_xess
        GIT_REPOSITORY https://github.com/intel/xess.git
        GIT_TAG        v3.0.2
        GIT_SHALLOW    TRUE
)

# Populate without add_subdirectory (repo ships no CMakeLists.txt).
# MakeAvailable only calls add_subdirectory when a CMakeLists.txt exists.
FetchContent_MakeAvailable(intel_xess)

# When the project was previously populated under a custom name path,
# SOURCE_DIR is still set by FetchContent.
if(NOT intel_xess_SOURCE_DIR)
        set(intel_xess_SOURCE_DIR
                "${CMAKE_BINARY_DIR}/_deps/intel_xess-src")
endif()

set(FREYA_XESS_ROOT "${intel_xess_SOURCE_DIR}" CACHE PATH
        "Intel XeSS SDK root (inc/, bin/, lib/)" FORCE)

set(FREYA_HAS_XESS OFF)

if(FREYA_ENABLE_XESS)
        if(WIN32)
                set(_xess_dll "${FREYA_XESS_ROOT}/bin/libxess.dll")
                set(_xess_lib "${FREYA_XESS_ROOT}/lib/libxess.lib")
                if(EXISTS "${_xess_dll}" AND EXISTS "${_xess_lib}")
                        add_library(xess SHARED IMPORTED GLOBAL)
                        set_target_properties(xess PROPERTIES
                                IMPORTED_LOCATION "${_xess_dll}"
                                IMPORTED_IMPLIB   "${_xess_lib}"
                                INTERFACE_INCLUDE_DIRECTORIES
                                        "${FREYA_XESS_ROOT}/inc"
                        )
                        set(FREYA_HAS_XESS ON)
                        message(STATUS
                                "Freya: Intel XeSS SDK v3.0.2 enabled "
                                "(${FREYA_XESS_ROOT})")
                else()
                        message(WARNING
                                "Freya: FREYA_ENABLE_XESS=ON but libxess "
                                "runtime was not found under "
                                "${FREYA_XESS_ROOT}; disabling XeSS.")
                endif()
        else()
                message(STATUS
                        "Freya: Intel XeSS SDK has no Linux runtime "
                        "(libxess.so); XeSS support disabled. Headers are "
                        "at ${FREYA_XESS_ROOT}/inc for reference.")
        endif()
endif()

# Copy libxess.dll next to an executable target (Windows only).
function(freya_copy_xess_runtime target)
        if(NOT FREYA_HAS_XESS)
                return()
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:xess>
                        $<TARGET_FILE_DIR:${target}>
                COMMENT
                        "Copying XeSS runtime (libxess.dll) for ${target}")
endfunction()
