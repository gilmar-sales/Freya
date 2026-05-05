add_executable(Sofa Main.cpp)

target_compile_features(Sofa PUBLIC cxx_std_23)

target_link_libraries(Sofa Freya::Freya)

if (MSVC)
    target_compile_options(Sofa PUBLIC "/ZI" "/MP")
    target_link_options(Sofa PUBLIC "/INCREMENTAL")
endif ()

add_dependencies(Sofa Shaders)
add_custom_command(TARGET Sofa POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/Resources/Shaders"
        "${CMAKE_BINARY_DIR}/Examples/Sofa/Resources/Shaders"
    COMMENT "Copying compiled shaders to Sofa/Resources/Shaders"
    VERBATIM
)

file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples/Sofa)
