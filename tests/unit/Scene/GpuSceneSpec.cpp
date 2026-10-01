#include <Freya/Asset/GpuScene.hpp>
#include <Freya/Asset/InstanceTransform.hpp>
#include <Freya/Asset/SceneInstanceUpload.hpp>
#include <Freya/Asset/SceneTransform.hpp>

#include <gtest/gtest.h>

#include <limits>

TEST(GpuScene, RecordsKeepGlslStd430Sizes)
{
    static_assert(sizeof(fra::MeshLodInfo) == 16);
    static_assert(sizeof(fra::MeshInfo) == 64);
    static_assert(sizeof(fra::SceneInstance) == 96);
    static_assert(sizeof(fra::CullPushConstants) == 128);
    static_assert(sizeof(fra::MaterialGPU) == 96);
    static_assert(sizeof(fra::SceneTransform) == 40);
    static_assert(fra::kPickMissId == 0xFFFFFFFFu);
    static_assert(fra::kNoSkin == std::numeric_limits<std::uint32_t>::max());
}

TEST(GpuScene, MaterialAndInstanceFlagsAreDistinctBits)
{
    EXPECT_EQ(fra::kMaterialFlagPackedMR & fra::kMaterialFlagUnlit, 0u);
    EXPECT_EQ(
        fra::kSceneInstanceFlagCastShadows & fra::kSceneInstanceFlagTranslucent,
        0u);
    EXPECT_EQ(
        fra::kSceneInstanceFlagTranslucent & fra::kSceneInstanceFlagSkinned,
        0u);
}

TEST(GpuScene, SceneTransformPackedLayout)
{
    static_assert(offsetof(fra::SceneTransform, scale) == 12);
    static_assert(offsetof(fra::SceneTransform, rotation) == 24);
}
