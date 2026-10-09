#include "../Support/AnimationFixtures.hpp"

#include <array>

using namespace fra::test;

TEST(BlendSpace, OneDimensionalSpanHandlesEmptySingletonAndBounds)
{
    const auto empty = fra::ResolveBlend1D({}, 10.f);
    EXPECT_EQ(empty.i0, 0u);
    EXPECT_EQ(empty.i1, 0u);
    EXPECT_FLOAT_EQ(empty.t, 0.f);
    const std::array one { 2.f };
    EXPECT_EQ(fra::ResolveBlend1D(one, 10.f).i1, 0u);
    const std::array values { 0.f, 2.f, 4.f };
    EXPECT_EQ(fra::ResolveBlend1D(values, -1.f).i0, 0u);
    EXPECT_EQ(fra::ResolveBlend1D(values, 10.f).i0, 2u);
    const auto span = fra::ResolveBlend1D(values, 3.f);
    EXPECT_EQ(span.i0, 1u);
    EXPECT_EQ(span.i1, 2u);
    EXPECT_FLOAT_EQ(span.t, 0.5f);
}

TEST(BlendSpace, OneDimensionalEvaluationBlendsClipsAndDefaultsMissingTimes)
{
    const auto       a = MakeConstantClip(2.f);
    const auto       b = MakeConstantClip(10.f);
    const std::array samples {
        fra::Blend1DSample { .value = 0.f, .clip = &a },
        fra::Blend1DSample { .value = 1.f, .clip = &b }
    };
    const auto pose = fra::EvaluateBlend1D(MakeSkeleton(), samples, {}, 0.25f);
    ASSERT_EQ(pose.Size(), 4u);
    EXPECT_FLOAT_EQ(pose.joints[0].translation.x, 4.f);
    const auto rest = fra::EvaluateBlend1D(MakeSkeleton(), {}, {}, 1.f);
    ASSERT_EQ(rest.Size(), 4u);
    EXPECT_FLOAT_EQ(rest.joints[0].translation.x, 0.f);
}

TEST(BlendSpace, TimesRespectLoopSpeedNullClipsAndShortOutput)
{
    const auto       clip = MakeLinearClip();
    const std::array samples {
        fra::Blend1DSample { .clip = &clip, .playbackSpeed = 2.f },
        fra::Blend1DSample { .clip = &clip, .loop = false },
        fra::Blend1DSample { .clip = &clip, .playbackSpeed = -1.f },
        fra::Blend1DSample {}
    };
    std::array times { 1.5f, 1.5f, 0.5f, 7.f };
    fra::AdvanceBlend1DTimes(samples, times, 0.5f);
    EXPECT_EQ(times, (std::array { 0.5f, 2.f, 0.5f, 7.f }));
    std::array shortTimes { 9.f };
    fra::AdvanceBlend1DTimes(samples, shortTimes, 1.f);
    EXPECT_FLOAT_EQ(shortTimes[0], 9.f);
}

TEST(BlendSpace, PhaseSynchronizesDifferentDurationsAndWraps)
{
    auto a     = MakeLinearClip();
    auto b     = a;
    b.duration = 4.f;
    const std::array samples {
        fra::Blend1DSample { .value = 0.f, .clip = &a },
        fra::Blend1DSample { .value = 1.f, .clip = &b }
    };
    EXPECT_NEAR(fra::AdvanceBlend1DPhase(samples, 0.5f, 0.9f, 0.6f), 0.1f,
                1e-5f);
    EXPECT_FLOAT_EQ(fra::AdvanceBlend1DPhase(samples, 0.f, 0.4f, 0.f), 0.4f);
    std::array times { 0.f, 0.f };
    fra::WriteBlend1DTimesFromPhase(samples, times, 0.25f);
    EXPECT_EQ(times, (std::array { 0.5f, 1.f }));
    fra::WriteBlend1DTimesFromPhase(samples, times, 2.f);
    EXPECT_EQ(times, (std::array { 2.f, 4.f }));
}

TEST(BlendSpace, TwoDimensionalTriangleProducesBarycentricWeights)
{
    const std::array positions { glm::vec2(0, 0), glm::vec2(1, 0),
                                 glm::vec2(0, 1) };
    const auto       tri = fra::ResolveBlend2D(positions, { 0.25f, 0.25f });
    EXPECT_EQ(tri.i0, 0u);
    EXPECT_EQ(tri.i1, 1u);
    EXPECT_EQ(tri.i2, 2u);
    EXPECT_NEAR(tri.w0, 0.5f, 1e-5f);
    EXPECT_NEAR(tri.w1, 0.25f, 1e-5f);
    EXPECT_NEAR(tri.w2, 0.25f, 1e-5f);
    EXPECT_NEAR(tri.w0 + tri.w1 + tri.w2, 1.f, 1e-5f);
}

TEST(BlendSpace, TwoDimensionalOutsideHullSnapsToNearestSample)
{
    const std::array positions { glm::vec2(0, 0), glm::vec2(1, 0),
                                 glm::vec2(0, 1) };
    const auto       tri = fra::ResolveBlend2D(positions, { 3.f, 0.f });
    EXPECT_EQ(tri.i0, 1u);
    EXPECT_EQ(tri.i1, 1u);
    EXPECT_EQ(tri.i2, 1u);
    EXPECT_FLOAT_EQ(tri.w0, 1.f);
    EXPECT_FLOAT_EQ(tri.w1 + tri.w2, 0.f);
}

TEST(BlendSpace, TwoSamplesProjectOntoLineAndCoincidentPointsStayFinite)
{
    const std::array line { glm::vec2(0, 0), glm::vec2(2, 0) };
    const auto       tri = fra::ResolveBlend2D(line, { 1.f, 5.f });
    EXPECT_FLOAT_EQ(tri.w0, 0.5f);
    EXPECT_FLOAT_EQ(tri.w1, 0.5f);
    const std::array duplicate { glm::vec2(0.f), glm::vec2(0.f) };
    const auto       same = fra::ResolveBlend2D(duplicate, { 1.f, 1.f });
    EXPECT_FLOAT_EQ(same.w0, 1.f);
    EXPECT_FLOAT_EQ(same.w1, 0.f);
}

TEST(BlendSpace, CollinearSamplesFallBackToNearestWithoutNan)
{
    const std::array positions { glm::vec2(0, 0), glm::vec2(1, 0),
                                 glm::vec2(2, 0) };
    const auto       tri = fra::ResolveBlend2D(positions, { 1.1f, 0.1f });
    EXPECT_EQ(tri.i0, 1u);
    EXPECT_FLOAT_EQ(tri.w0, 1.f);
    EXPECT_EQ(fra::ResolveBlend2D({}, {}).i0, 0u);
}

TEST(BlendSpace, TwoDimensionalEvaluationBlendsThreeClips)
{
    const auto       a = MakeConstantClip(0.f);
    const auto       b = MakeConstantClip(8.f);
    const auto       c = MakeConstantClip(16.f);
    const std::array samples {
        fra::Blend2DSample { .pos = { 0, 0 }, .clip = &a },
        fra::Blend2DSample { .pos = { 1, 0 }, .clip = &b },
        fra::Blend2DSample { .pos = { 0, 1 }, .clip = &c }
    };
    const auto pose =
        fra::EvaluateBlend2D(MakeSkeleton(), samples, {}, { 0.25f, 0.25f });
    ASSERT_EQ(pose.Size(), 4u);
    EXPECT_NEAR(pose.joints[0].translation.x, 6.f, 1e-5f);
    std::array times { 1.75f, 1.75f, 1.75f };
    fra::AdvanceBlend2DTimes(samples, times, 0.5f);
    EXPECT_EQ(times, (std::array { 0.25f, 0.25f, 0.25f }));
    fra::WriteBlend2DTimesFromPhase(samples, times, 0.5f);
    EXPECT_EQ(times, (std::array { 1.f, 1.f, 1.f }));
    EXPECT_NEAR(fra::AdvanceBlend2DPhase(samples, { 0.25f, 0.25f }, 0.75f, 1.f),
                0.25f, 1e-5f);
}
