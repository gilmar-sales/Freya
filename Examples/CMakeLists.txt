add_executable(Main Main.cpp)

target_compile_features(Main PUBLIC cxx_std_23)

target_link_libraries(Main Freya-static)

if (MSVC)
    target_compile_options(Main PUBLIC "/ZI" "/MP")
    target_link_options(Main PUBLIC "/INCREMENTAL")
endif ()


file(COPY ../Shaders DESTINATION ${CMAKE_BINARY_DIR}/Examples/Resources)
file(COPY Resources DESTINATION ${CMAKE_BINARY_DIR}/Examples)
