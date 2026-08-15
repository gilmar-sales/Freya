add_executable(CellBulbasaur Main.cpp)

target_compile_features(CellBulbasaur PUBLIC cxx_std_26)

target_link_libraries(CellBulbasaur Freya::Freya)

if (MSVC)
    target_compile_options(CellBulbasaur PUBLIC "/ZI" "/MP")
    target_link_options(CellBulbasaur PUBLIC "/INCREMENTAL")
endif ()

add_shader_outputs(Shaders
    "${CMAKE_BINARY_DIR}/Examples/CellBulbasaur/Resources/Shaders"
)
add_dependencies(CellBulbasaur ${Shaders_OUTPUT_TARGETS})

file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples/CellBulbasaur)

set(_freya_default_ibl
    "${CMAKE_SOURCE_DIR}/Resources/studio_small_09_4k.hdr")
if(EXISTS "${_freya_default_ibl}")
    file(MAKE_DIRECTORY
        "${CMAKE_BINARY_DIR}/Examples/CellBulbasaur/Resources/Environments")
    file(COPY "${_freya_default_ibl}"
         DESTINATION
         "${CMAKE_BINARY_DIR}/Examples/CellBulbasaur/Resources/Environments")
endif()
