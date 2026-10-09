#include "../Support/AnimationFixtures.hpp"

#include <Freya/Asset/BakedAnimation.hpp>

using namespace fra::test;

TEST(BakedAnimation, BakeUsesFrameMajorLayoutWithoutDuplicateLoopEndpoint)
{
    const auto baked = fra::BakeClip(MakeSkeleton(), MakeLinearClip(), 2.f);
    EXPECT_FALSE(baked.Empty());
    EXPECT_EQ(baked.jointCount, 4u);
    ASSERT_EQ(baked.frameCount, 4u);
    ASSERT_EQ(baked.joints.size(), 16u);
    EXPECT_FLOAT_EQ(baked.frameDt, 0.5f);
    for (unsigned frame = 0; frame < 4; ++frame)
    {
        EXPECT_FLOAT_EQ(baked.joints[frame * 4].translation.x, frame * 2.f);
        ExpectMatrixNear(baked.joints[frame * 4 + 1].ToMatrix(),
                         glm::mat4(1.f));
    }
}

TEST(BakedAnimation, EmptySkeletonProducesEmptyBake)
{
    const auto baked = fra::BakeClip({}, MakeLinearClip());
    EXPECT_TRUE(baked.Empty());
    EXPECT_EQ(baked.frameCount, 0u);
    EXPECT_TRUE(baked.joints.empty());
}

TEST(BakedAnimation, ZeroDurationProducesSingleRestFrame)
{
    auto skeleton         = MakeSkeleton();
    skeleton.restLocal[0] = glm::translate(glm::mat4(1.f), glm::vec3(3, 0, 0));
    const auto baked      = fra::BakeClip(skeleton, {});
    ASSERT_EQ(baked.frameCount, 1u);
    const auto pose = fra::SampleBaked(skeleton, baked, 100.f);
    ASSERT_EQ(pose.Size(), 4u);
    EXPECT_FLOAT_EQ(pose.joints[0].translation.x, 3.f);
}

TEST(BakedAnimation, BakeRateIsClampedAndShortClipsHaveTwoFrames)
{
    auto       clip  = MakeLinearClip();
    const auto baked = fra::BakeClip(MakeSkeleton(), clip, -3.f);
    EXPECT_FLOAT_EQ(baked.frameDt, 1.f);
    EXPECT_EQ(baked.frameCount, 2u);
    clip.duration = 0.01f;
    EXPECT_EQ(fra::BakeClip(MakeSkeleton(), clip, 1.f).frameCount, 2u);
}

TEST(BakedAnimation, SamplingInterpolatesAdjacentFrames)
{
    const auto skeleton = MakeSkeleton();
    const auto baked    = fra::BakeClip(skeleton, MakeLinearClip(), 2.f);
    const auto pose     = fra::SampleBaked(skeleton, baked, 0.25f);
    ASSERT_EQ(pose.Size(), 4u);
    EXPECT_FLOAT_EQ(pose.joints[0].translation.x, 1.f);
}

TEST(BakedAnimation, SamplingWrapsAcrossLastAndFirstFrames)
{
    const auto skeleton = MakeSkeleton();
    const auto baked    = fra::BakeClip(skeleton, MakeLinearClip(), 2.f);
    for (float time : { 1.75f, -0.25f, 3.75f })
    {
        SCOPED_TRACE(time);
        EXPECT_FLOAT_EQ(
            fra::SampleBaked(skeleton, baked, time).joints[0].translation.x,
            3.f);
    }
}

TEST(BakedAnimation, NonLoopSamplingClampsToFirstAndLastStoredFrames)
{
    const auto skeleton = MakeSkeleton();
    const auto baked    = fra::BakeClip(skeleton, MakeLinearClip(), 2.f);
    EXPECT_FLOAT_EQ(
        fra::SampleBaked(skeleton, baked, -1.f, false).joints[0].translation.x,
        0.f);
    EXPECT_FLOAT_EQ(
        fra::SampleBaked(skeleton, baked, 10.f, false).joints[0].translation.x,
        6.f);
}

TEST(BakedAnimation, EmptyOrIncompatibleBakeFallsBackToRestPose)
{
    auto skeleton         = MakeSkeleton();
    skeleton.restLocal[0] = glm::translate(glm::mat4(1.f), glm::vec3(7, 0, 0));
    EXPECT_FLOAT_EQ(fra::SampleBaked(skeleton, {}, 0.f).joints[0].translation.x,
                    7.f);
    auto baked       = fra::BakeClip(skeleton, MakeLinearClip());
    baked.jointCount = 1;
    EXPECT_FLOAT_EQ(
        fra::SampleBaked(skeleton, baked, 0.f).joints[0].translation.x,
        7.f);
}
