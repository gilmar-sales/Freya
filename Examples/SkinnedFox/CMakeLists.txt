add_executable(SkinnedFox Main.cpp)

target_compile_features(SkinnedFox PUBLIC cxx_std_26)

target_link_libraries(SkinnedFox Freya::Freya)

if (MSVC)
    target_compile_options(SkinnedFox PUBLIC "/ZI" "/MP")
    target_link_options(SkinnedFox PUBLIC "/INCREMENTAL")
endif ()

add_shader_outputs(Shaders
    "${CMAKE_BINARY_DIR}/Examples/SkinnedFox/Resources/Shaders"
)
add_dependencies(SkinnedFox ${Shaders_OUTPUT_TARGETS})

file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples/SkinnedFox)

set(_freya_default_ibl
    "${CMAKE_SOURCE_DIR}/Resources/studio_small_09_4k.hdr")
if(EXISTS "${_freya_default_ibl}")
    file(MAKE_DIRECTORY
        "${CMAKE_BINARY_DIR}/Examples/SkinnedFox/Resources/Environments")
    file(COPY "${_freya_default_ibl}"
         DESTINATION
         "${CMAKE_BINARY_DIR}/Examples/SkinnedFox/Resources/Environments")
endif()
