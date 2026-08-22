#include <Freya/Asset/GpuScene.hpp>
#include <Freya/Asset/InstanceTransform.hpp>

#include <catch2/catch_test_macros.hpp>

#include <limits>

TEST_CASE("GPU scene records keep GLSL std430 sizes", "[gpu-scene]")
{
    STATIC_REQUIRE(sizeof(fra::MeshLodInfo) == 16);
    STATIC_REQUIRE(sizeof(fra::MeshInfo) == 64);
    STATIC_REQUIRE(sizeof(fra::SceneInstance) == 96);
    STATIC_REQUIRE(sizeof(fra::CullPushConstants) == 120);
    STATIC_REQUIRE(sizeof(fra::MaterialGPU) == 96);
    STATIC_REQUIRE(fra::kPickMissId == 0xFFFFFFFFu);
    STATIC_REQUIRE(fra::kNoSkin == std::numeric_limits<std::uint32_t>::max());
}

TEST_CASE("material and instance flags are distinct bits", "[gpu-scene]")
{
    REQUIRE((fra::kMaterialFlagPackedMR & fra::kMaterialFlagUnlit) == 0u);
    REQUIRE((fra::kSceneInstanceFlagCastShadows &
             fra::kSceneInstanceFlagTranslucent) == 0u);
    REQUIRE((fra::kSceneInstanceFlagTranslucent &
             fra::kSceneInstanceFlagSkinned) == 0u);
}
