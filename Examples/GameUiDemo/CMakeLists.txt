add_freya_example(GameUiDemo SOURCES Main.cpp IBL none)

# Paper-doll preview reuses the SkinnedFox Fox.glb asset.
set(_fox_src
    "${CMAKE_SOURCE_DIR}/Examples/SkinnedFox/Resources/Models/Fox.glb")
set(_fox_dst_dir
    "${CMAKE_BINARY_DIR}/Examples/GameUiDemo/Resources/Models")
if(EXISTS "${_fox_src}")
    file(MAKE_DIRECTORY "${_fox_dst_dir}")
    file(COPY "${_fox_src}" DESTINATION "${_fox_dst_dir}")
endif()
