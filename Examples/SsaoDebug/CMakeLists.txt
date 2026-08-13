add_executable(SsaoDebug Main.cpp)

target_compile_features(SsaoDebug PUBLIC cxx_std_26)

target_link_libraries(SsaoDebug Freya::Freya)

if (MSVC)
    target_compile_options(SsaoDebug PUBLIC "/ZI" "/MP")
    target_link_options(SsaoDebug PUBLIC "/INCREMENTAL")
endif ()

add_shader_outputs(Shaders
    "${CMAKE_BINARY_DIR}/Examples/SsaoDebug/Resources/Shaders"
)
add_dependencies(SsaoDebug ${Shaders_OUTPUT_TARGETS})

file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples/SsaoDebug)

# Engine default IBL environment (repo-root Resources/)
set(_freya_default_ibl
    "${CMAKE_SOURCE_DIR}/Resources/studio_small_09_4k.hdr")
if(EXISTS "${_freya_default_ibl}")
    file(MAKE_DIRECTORY
        "${CMAKE_BINARY_DIR}/Examples/SsaoDebug/Resources/Environments")
    file(COPY "${_freya_default_ibl}"
         DESTINATION
         "${CMAKE_BINARY_DIR}/Examples/SsaoDebug/Resources/Environments")
endif()
