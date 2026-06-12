add_executable(IndustrialPipeLamp Main.cpp)

target_compile_features(IndustrialPipeLamp PUBLIC cxx_std_23)

target_link_libraries(IndustrialPipeLamp Freya::Freya)

if (MSVC)
    target_compile_options(IndustrialPipeLamp PUBLIC "/ZI" "/MP")
    target_link_options(IndustrialPipeLamp PUBLIC "/INCREMENTAL")
endif ()

add_shader_outputs(Shaders
    "${CMAKE_BINARY_DIR}/Examples/IndustrialPipeLamp/Resources/Shaders"
)
add_dependencies(IndustrialPipeLamp ${Shaders_OUTPUT_TARGETS})

file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples/IndustrialPipeLamp)
