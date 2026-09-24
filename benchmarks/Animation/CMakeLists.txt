add_executable(
    AnimationBench
    src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/Freya/Asset/Pose.cpp
    ${CMAKE_SOURCE_DIR}/src/Freya/Asset/AnimationClip.cpp
    ${CMAKE_SOURCE_DIR}/src/Freya/Asset/BakedAnimation.cpp
    ${CMAKE_SOURCE_DIR}/src/Freya/Asset/Rig.cpp
    ${CMAKE_SOURCE_DIR}/src/Freya/Asset/AnimGraph.cpp
)

target_compile_features(AnimationBench PRIVATE cxx_std_26)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(AnimationBench PRIVATE -freflection)
endif()

target_include_directories(
    AnimationBench PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(AnimationBench PRIVATE benchmark::benchmark glm
                                             skirnir::skirnir)

target_precompile_headers(AnimationBench PRIVATE <Freya/Config.hpp>)
