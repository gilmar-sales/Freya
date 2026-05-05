# CompileShaders.cmake
# ---------------------------------------------------------------------------
# Reusable module for compiling GLSL shaders to SPIR-V at build time.
#
# Provides:
#   find_glslc()         — Locates glslc, stores result in GLSLC variable
#   add_shader_target()  — Creates a custom target that compiles all shaders
#                          under a source directory into an output directory.
#
# Usage:
#   include(CompileShaders)
#   find_glslc()
#   add_shader_target(TARGET MyShaders
#                     FROM  "${CMAKE_SOURCE_DIR}/Shaders"
#                     INTO  "${CMAKE_BINARY_DIR}/Resources/Shaders"
#   )
#   add_dependencies(MyLibrary MyShaders)
#
# If glslc is not found a warning is emitted and add_shader_target creates
# a configure-time copy fallback instead.
# ---------------------------------------------------------------------------

# Find path to glslc from the Vulkan SDK.
macro(find_glslc)
    if(NOT GLSLC)
        find_program(GLSLC glslc
            PATHS "$ENV{VULKAN_SDK}/Bin"
        )
        if(GLSLC)
            message(STATUS "Found glslc: ${GLSLC}")
        else()
            message(WARNING
                "glslc not found — shaders will not be recompiled at build time. "
                "Set VULKAN_SDK or add glslc to PATH."
            )
        endif()
    endif()
endmacro()

# Create a target that compiles every .vert/.frag under FROM into .spv under INTO.
# The target name is given by TARGET.
function(add_shader_target)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "TARGET;FROM;INTO" "")

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "add_shader_target: TARGET is required")
    endif()
    if(NOT ARG_FROM)
        message(FATAL_ERROR "add_shader_target: FROM is required")
    endif()
    if(NOT ARG_INTO)
        message(FATAL_ERROR "add_shader_target: INTO is required")
    endif()

    if(GLSLC)
        file(GLOB_RECURSE _sources
            "${ARG_FROM}/*.vert" "${ARG_FROM}/*.frag"
        )

        if(NOT _sources)
            message(WARNING "add_shader_target: no .vert/.frag files found in ${ARG_FROM}")
            add_custom_target(${ARG_TARGET} ALL)
            return()
        endif()

        foreach(_src ${_sources})
            file(RELATIVE_PATH _rel ${ARG_FROM} ${_src})
            get_filename_component(_dir ${_rel} DIRECTORY)
            get_filename_component(_name ${_src} NAME)
            set(_spv "${ARG_INTO}/${_dir}/${_name}.spv")

            add_custom_command(
                OUTPUT  ${_spv}
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "${ARG_INTO}/${_dir}"
                COMMAND ${GLSLC} -o ${_spv} ${_src}
                DEPENDS ${_src}
                COMMENT "Compiling ${_rel} → ${_name}.spv"
                VERBATIM
            )
            list(APPEND _outputs ${_spv})
        endforeach()

        add_custom_target(${ARG_TARGET} ALL DEPENDS ${_outputs})
        message(STATUS "add_shader_target: ${ARG_TARGET} — ${_outputs} targets")
    else()
        # Fallback — copy whatever is in FROM (hopefully pre-compiled .spv)
        add_custom_target(${ARG_TARGET} ALL)
        file(COPY ${ARG_FROM} DESTINATION ${ARG_INTO})
        message(STATUS "add_shader_target: ${ARG_TARGET} — fallback copy from ${ARG_FROM}")
    endif()
endfunction()
