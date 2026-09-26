#include <Freya/Asset/GpuAnimation.hpp>
#include <Freya/Asset/Pose.hpp>
#include <Freya/Asset/Skeleton.hpp>

#include <gtest/gtest.h>

#include <cstddef>

#include <glm/gtc/quaternion.hpp>

// Parallel GPU packing contract (no Vulkan device in these tests):
// - Prefer pre-resident clips/skeletons on main: Upload+Pin or Ensure+Pin.
// - Workers: FindClipSlot / FindSkeletonSlot and/or Ensure* (SpinLock;
//   free-slot fills only while instance staging is open — no LRU evict).
// - Evict / Reset / UploadClipSlot / UploadSkeleton / UploadBakes rejected
//   while instance staging is open.
// - Multi-rig mid-tier: GpuAnimInstance::skeletonSlot indexes atlas slabs.

TEST(GpuAnimation, ClipKeyIsStableFnv1aAndNeverZero)
{
    EXPECT_EQ(fra::GpuClipKey("Idle"), fra::GpuClipKey("Idle"));
    EXPECT_NE(fra::GpuClipKey("Idle"), fra::GpuClipKey("Walk"));
    EXPECT_NE(fra::GpuClipKey(""), 0ull);
}

TEST(GpuAnimation, SkeletonKeyMatchesClipKeyFnv)
{
    EXPECT_EQ(fra::GpuSkeletonKey("Fox"), fra::GpuClipKey("Fox"));
    EXPECT_NE(fra::GpuSkeletonKey("Fox"), fra::GpuSkeletonKey("Human"));
}

TEST(GpuAnimation, InstanceSkeletonSlotLayoutAndHeaderSize)
{
    static_assert(sizeof(fra::GpuSkeletonHeader) == 16);
    static_assert(
        offsetof(fra::GpuAnimInstance, skeletonSlot) ==
        offsetof(fra::GpuAnimInstance, clipAdd) + sizeof(std::uint32_t));
    fra::GpuAnimInstance inst {};
    EXPECT_EQ(inst.skeletonSlot, 0u);
}

TEST(GpuAnimation, IdentityQuaternionOmitsWInSmallestThreePack)
{
    const auto bits = fra::PackQuatSmallestThree(glm::quat(1.f, 0.f, 0.f, 0.f));
    EXPECT_EQ(bits >> 30, 3u);
}

TEST(GpuAnimation, FloatJointPackCopiesTrs)
{
    fra::JointTRS j;
    j.translation = { 1.f, 2.f, 3.f };
    j.rotation    = glm::quat(1.f, 0.f, 0.f, 0.f);
    j.scale       = { 2.f, 2.f, 2.f };

    const auto g = fra::ToGpuFloatJoint(j);
    EXPECT_TRUE(g.t == j.translation);
    EXPECT_TRUE(g.s == j.scale);
    EXPECT_FLOAT_EQ(g.q.w, 1.f);
}

TEST(GpuAnimation, PackSkeletonFillsMissingParentsAndIbm)
{
    fra::Skeleton sk;
    sk.names = { "root", "child" };

    const auto pack = fra::PackSkeleton(sk);
    EXPECT_EQ(pack.jointCount, 2u);
    EXPECT_EQ(pack.parents.size(), 2u);
    EXPECT_EQ(pack.inverseBind.size(), 2u);
    EXPECT_EQ(pack.parents[0], -1);
    EXPECT_TRUE(pack.inverseBind[1] == glm::mat4(1.f));
}

TEST(GpuAnimation, PackBoneMaskClampsAndPads)
{
    fra::BoneMask mask;
    mask.weights = { 1.5f, -0.2f };

    const auto packed = fra::PackBoneMask(mask, 4);
    EXPECT_EQ(packed.size(), 4u);
    EXPECT_FLOAT_EQ(packed[0], 1.f);
    EXPECT_FLOAT_EQ(packed[1], 0.f);
    EXPECT_FLOAT_EQ(packed[2], 0.f);
    EXPECT_FLOAT_EQ(packed[3], 0.f);
}
