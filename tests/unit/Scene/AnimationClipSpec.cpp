#include "../Support/AnimationFixtures.hpp"

namespace
{
    fra::AnimationClip EventClip()
    {
        auto clip   = fra::test::MakeLinearClip();
        clip.events = { { "start", 0.f },
                        { "left", 0.5f },
                        { "right", 1.5f },
                        { "end", 2.f } };
        return clip;
    }
} // namespace

TEST(AnimationClip, EventsUseExclusiveStartInclusiveEndAndAppend)
{
    const auto                       clip = EventClip();
    std::vector<fra::AnimationEvent> out { { "existing", -1.f } };
    fra::CollectClipEvents(clip, 0.5f, 1.5f, false, out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].name, "existing");
    EXPECT_EQ(out[1].name, "right");
}

TEST(AnimationClip, LoopBoundaryIncludesEndAndTimeZeroInOrder)
{
    const auto                       clip = EventClip();
    std::vector<fra::AnimationEvent> out;
    fra::CollectClipEvents(clip, 1.75f, 0.5f, true, out);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].name, "end");
    EXPECT_EQ(out[1].name, "start");
    EXPECT_EQ(out[2].name, "left");
}

TEST(AnimationClip, UnchangedTimeAndEmptyEventsDoNotEmit)
{
    std::vector<fra::AnimationEvent> out;
    fra::CollectClipEvents(EventClip(), 0.5f, 0.5f, true, out);
    fra::CollectClipEvents({}, 0.f, 1.f, false, out);
    EXPECT_TRUE(out.empty());
}

TEST(AnimationClip, NegativeLoopTimesWrap)
{
    std::vector<fra::AnimationEvent> out;
    fra::CollectClipEvents(EventClip(), -1.75f, -1.25f, true, out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].name, "left");
}

TEST(AnimationClip, FiredEventsRetainSourceClipAndAppend)
{
    const auto                            clip = EventClip();
    std::vector<fra::FiredAnimationEvent> out { { "existing", -1.f, nullptr } };
    fra::CollectFiredClipEvents(clip, 0.f, 0.5f, false, out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[1].clip, &clip);
    EXPECT_EQ(out[1].name, "left");
    EXPECT_FLOAT_EQ(out[1].timeSec, 0.5f);
}

TEST(AnimationClip, DefaultFootstepsAreInsertedOnce)
{
    auto clip = fra::test::MakeLinearClip();
    fra::EnsureDefaultFootstepEvents(clip);
    fra::EnsureDefaultFootstepEvents(clip);
    ASSERT_EQ(clip.events.size(), 2u);
    EXPECT_EQ(clip.events[0].name, "Footstep.L");
    EXPECT_EQ(clip.events[1].name, "Footstep.R");
    EXPECT_FLOAT_EQ(clip.events[0].timeSec, 0.3f);
    EXPECT_FLOAT_EQ(clip.events[1].timeSec, 1.3f);
}

TEST(AnimationClip, DefaultFootstepsPreserveAuthoredEventsAndSkipZeroDuration)
{
    auto clip = EventClip();
    fra::EnsureDefaultFootstepEvents(clip);
    EXPECT_EQ(clip.events.size(), 4u);
    fra::AnimationClip empty;
    fra::EnsureDefaultFootstepEvents(empty);
    EXPECT_TRUE(empty.events.empty());
}

TEST(AnimationClip, EvaluateSkeletonPoseIncludesHierarchyAndLooping)
{
    const auto skin = fra::EvaluateSkeletonPose(
        fra::test::MakeSkeleton(), fra::test::MakeLinearClip(), 2.5f);
    ASSERT_EQ(skin.size(), 4u);
    for (const auto& matrix : skin)
        fra::test::ExpectVecNear(glm::vec3(matrix[3]), { 2.f, 0.f, 0.f });
}
