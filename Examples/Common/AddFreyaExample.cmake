# add_freya_example(<Name>
#   SOURCES Main.cpp [...]
#   [IBL studio|outdoor|both|none]   # default: studio
# )
function(add_freya_example EXAMPLE_NAME)
    cmake_parse_arguments(ARG "" "IBL" "SOURCES" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "add_freya_example(${EXAMPLE_NAME}): SOURCES required")
    endif()

    if(NOT ARG_IBL)
        set(ARG_IBL studio)
    endif()

    add_executable(${EXAMPLE_NAME} ${ARG_SOURCES})

    target_compile_features(${EXAMPLE_NAME} PUBLIC cxx_std_26)

    target_link_libraries(${EXAMPLE_NAME}
        PRIVATE Freya::Freya FreyaExamplesCommon)

    if(MSVC)
        target_compile_options(${EXAMPLE_NAME} PUBLIC "/ZI" "/MP")
        target_link_options(${EXAMPLE_NAME} PUBLIC "/INCREMENTAL")
    endif()

    set(_example_bin_dir "${CMAKE_BINARY_DIR}/Examples/${EXAMPLE_NAME}")

    add_shader_outputs(Shaders
        "${_example_bin_dir}/Resources/Shaders"
    )
    add_dependencies(${EXAMPLE_NAME} ${Shaders_OUTPUT_TARGETS})

    file(COPY Resources DESTINATION "${_example_bin_dir}")

    set(_ibl_files "")
    if(ARG_IBL STREQUAL "studio" OR ARG_IBL STREQUAL "both")
        list(APPEND _ibl_files
            "${CMAKE_SOURCE_DIR}/Resources/studio_small_09_4k.hdr")
    endif()
    if(ARG_IBL STREQUAL "outdoor" OR ARG_IBL STREQUAL "both")
        list(APPEND _ibl_files
            "${CMAKE_SOURCE_DIR}/Resources/horn-koppe_spring_4k.hdr")
    endif()
    if(ARG_IBL STREQUAL "none")
        set(_ibl_files "")
    endif()

    if(_ibl_files)
        file(MAKE_DIRECTORY "${_example_bin_dir}/Resources/Environments")
        foreach(_ibl IN LISTS _ibl_files)
            if(EXISTS "${_ibl}")
                file(COPY "${_ibl}"
                     DESTINATION "${_example_bin_dir}/Resources/Environments")
            endif()
        endforeach()
    endif()
endfunction()
