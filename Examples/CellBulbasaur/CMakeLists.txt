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

set(_freya_studio_ibl
    "${CMAKE_SOURCE_DIR}/Resources/studio_small_09_4k.hdr")
set(_freya_outdoor_ibl
    "${CMAKE_SOURCE_DIR}/Resources/horn-koppe_spring_4k.hdr")
file(MAKE_DIRECTORY
    "${CMAKE_BINARY_DIR}/Examples/CellBulbasaur/Resources/Environments")
foreach(_ibl IN ITEMS ${_freya_studio_ibl} ${_freya_outdoor_ibl})
    if(EXISTS "${_ibl}")
        file(COPY "${_ibl}" DESTINATION
             "${CMAKE_BINARY_DIR}/Examples/CellBulbasaur/Resources/Environments")
    endif()
endforeach()
