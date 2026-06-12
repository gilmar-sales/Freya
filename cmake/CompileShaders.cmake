# CompileShaders.cmake
# ---------------------------------------------------------------------------
# Reusable module for compiling GLSL shaders to SPIR-V at build time.
#
# Provides:
#   find_glslc()           — Locates glslc, stores result in GLSLC variable
#   add_shader_target()    — Creates a custom target that compiles all shaders
#                            under a source directory into an output (staging)
#                            directory.
#   add_shader_outputs()   — Adds one or more per-target output directories to
#                            a previously declared shader target. For every
#                            output, a per-target custom target is created
#                            that copies the staged .spv files into the given
#                            directory (preserving the <variant>/ structure).
#                            The list of created per-target target names is
#                            stored in parent scope as
#                            "${TARGET}_OUTPUT_TARGETS" so callers can wire
#                            them up with a single add_dependencies().
#
# Usage:
#   include(CompileShaders)
#   find_glslc()
#   add_shader_target(TARGET Shaders
#                     FROM  "${CMAKE_SOURCE_DIR}/Shaders"
#                     INTO  "${CMAKE_BINARY_DIR}/Resources/Shaders"
#   )
#   add_dependencies(MyLibrary Shaders)
#
#   # In an example (or any consumer) that needs the shaders next to its
#   # binary:
#   add_shader_outputs(Shaders
#       "${CMAKE_BINARY_DIR}/Examples/MyExample/Resources/Shaders"
#   )
#   add_dependencies(MyExample ${Shaders_OUTPUT_TARGETS})
#
# If glslc is not found a warning is emitted and add_shader_target creates
# a configure-time copy fallback (mirrored by add_shader_outputs for every
# requested output).
# ---------------------------------------------------------------------------

# Find path to glslc from the Vulkan SDK.
macro(find_glslc)
    if(NOT GLSLC)
        find_program(GLSLC glslc)
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

# Internal helper — derive a filesystem-safe suffix from an output path
# for use inside a CMake target name.
function(_shader_target_suffix_for_path path out_var)
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _suffix "${path}")
    set(${out_var} "${_suffix}" PARENT_SCOPE)
endfunction()

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
        # Expose the staged .spv list + staging root for add_shader_outputs().
        set_property(TARGET ${ARG_TARGET} PROPERTY
            _SHADER_STAGED_OUTPUTS "${_outputs}"
        )
        set_property(TARGET ${ARG_TARGET} PROPERTY
            _SHADER_STAGING_ROOT "${ARG_INTO}"
        )
        message(STATUS "add_shader_target: ${ARG_TARGET} — ${_outputs} targets")
    else()
        # Fallback — copy whatever is in FROM (hopefully pre-compiled .spv)
        add_custom_target(${ARG_TARGET} ALL)
        file(COPY ${ARG_FROM} DESTINATION ${ARG_INTO})
        set_property(TARGET ${ARG_TARGET} PROPERTY
            _SHADER_STAGING_ROOT "${ARG_INTO}"
        )
        message(STATUS "add_shader_target: ${ARG_TARGET} — fallback copy from ${ARG_FROM}")
    endif()

    # Reset the per-output target list in the caller's scope. New entries
    # are appended by add_shader_outputs().
    set(${ARG_TARGET}_OUTPUT_TARGETS "" PARENT_SCOPE)
endfunction()

# Add one or more per-target output directories to a shader target created
# by add_shader_target(). For every output path a custom target named
# "${TARGET}__copy__<idx>__<sanitized-path>" is created which copies the
# staged .spv files into the requested directory (preserving the
# <variant>/ sub-directory layout). The list of created target names is
# exposed in parent scope as "${TARGET}_OUTPUT_TARGETS" so callers can do
# `add_dependencies(MyExample ${Shaders_OUTPUT_TARGETS})`.
function(add_shader_outputs TARGET)
    if(NOT TARGET)
        message(FATAL_ERROR "add_shader_outputs: TARGET is required")
    endif()
    if(NOT ARGN)
        message(FATAL_ERROR "add_shader_outputs: at least one OUTPUT is required")
    endif()

    get_target_property(_staged_spvs "${TARGET}" _SHADER_STAGED_OUTPUTS)
    get_target_property(_staging_root "${TARGET}" _SHADER_STAGING_ROOT)

    set(_output_targets "")
    set(_idx 0)
    foreach(_output ${ARGN})
        set(_dest_outputs "")
        if(GLSLC AND _staged_spvs)
            foreach(_spv ${_staged_spvs})
                if(IS_ABSOLUTE "${_spv}" AND IS_ABSOLUTE "${_staging_root}")
                    file(RELATIVE_PATH _rel "${_staging_root}" "${_spv}")
                else()
                    get_filename_component(_rel "${_spv}" DIRECTORY)
                endif()
                set(_dest "${_output}/${_rel}")
                get_filename_component(_dest_dir "${_dest}" DIRECTORY)
                add_custom_command(
                    OUTPUT  ${_dest}
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${_dest_dir}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_spv}" "${_dest}"
                    DEPENDS ${_spv}
                    COMMENT "Copying ${_rel} → ${_output}"
                    VERBATIM
                )
                list(APPEND _dest_outputs ${_dest})
            endforeach()
        endif()

        _shader_target_suffix_for_path("${_output}" _suffix)
        set(_tgt "${TARGET}__copy__${_idx}__${_suffix}")
        if(GLSLC AND _dest_outputs)
            add_custom_target(${_tgt} ALL DEPENDS ${_dest_outputs})
        elseif(NOT GLSLC AND _staging_root)
            # Fallback — copy the entire staging dir (which itself was
            # populated at configure time by add_shader_target) into the
            # requested output location.
            add_custom_target(${_tgt} ALL)
            file(COPY "${_staging_root}" DESTINATION "${_output}")
        else()
            add_custom_target(${_tgt} ALL)
        endif()
        list(APPEND _output_targets "${_tgt}")
        math(EXPR _idx "${_idx} + 1")
    endforeach()

    # Append the new per-target copy targets to the parent-scope list,
    # preserving any previously declared outputs for this target.
    set(_existing "${${TARGET}_OUTPUT_TARGETS}")
    if(_existing)
        set(${TARGET}_OUTPUT_TARGETS
            ${_existing} ${_output_targets}
            PARENT_SCOPE
        )
    else()
        set(${TARGET}_OUTPUT_TARGETS
            ${_output_targets}
            PARENT_SCOPE
        )
    endif()

    message(STATUS
        "add_shader_outputs: ${TARGET} → ${_output_targets}"
    )
endfunction()
