add_executable(Sofa Main.cpp)

target_compile_features(Sofa PUBLIC cxx_std_23)

target_link_libraries(Sofa Freya::Freya)

if (MSVC)
    target_compile_options(Sofa PUBLIC "/ZI" "/MP")
    target_link_options(Sofa PUBLIC "/INCREMENTAL")
endif ()


file(COPY ../../shaders DESTINATION ${CMAKE_BINARY_DIR}/Examples/Sofa/Resources)
file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples/Sofa)
