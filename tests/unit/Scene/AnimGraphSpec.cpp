#include "../Support/AnimationFixtures.hpp"

#include <Freya/Asset/AnimGraph.hpp>
#include <Freya/Asset/AnimGraphDebug.hpp>

#include <stdexcept>

using namespace fra::test;

class AnimGraphSpec : public testing::Test
{
  protected:
    fra::Skeleton      skeleton = MakeSkeleton();
    fra::AnimationClip idle     = MakeConstantClip(0.f);
    fra::AnimationClip run      = MakeConstantClip(8.f);

    fra::AnimGraphBuilder Builder()
    {
        fra::AnimGraphBuilder builder;
        builder.SetSkeleton(&skeleton)
            .ParamFloat("speed", 0.f)
            .ParamBool("active", false)
            .ParamTrigger("go")
            .State("idle", idle)
            .State("run", run);
        return builder;
    }
};

TEST_F(AnimGraphSpec, BuilderRejectsMissingSkeletonStatesAndUnknownEntry)
{
    EXPECT_THROW(fra::AnimGraphBuilder {}.State("idle", idle).Build(),
                 std::runtime_error);
    EXPECT_THROW(fra::AnimGraphBuilder {}.SetSkeleton(&skeleton).Build(),
                 std::runtime_error);
    EXPECT_THROW(Builder().Entry("missing").Build(), std::runtime_error);
    EXPECT_THROW(Builder().Transition("idle", "missing",
                                      fra::AnimCondition::OnTrigger("go")),
                 std::runtime_error);
}

TEST_F(AnimGraphSpec, BuilderRejectsInsufficientBlendSamples)
{
    EXPECT_THROW(fra::AnimGraphBuilder {}
                     .SetSkeleton(&skeleton)
                     .Blend1DState("loco", "speed")
                     .AddBlendSample(0.f, idle)
                     .Build(),
                 std::runtime_error);
    EXPECT_THROW(fra::AnimGraphBuilder {}
                     .SetSkeleton(&skeleton)
                     .Blend2DState("loco", "x", "y")
                     .AddBlend2DSample(0.f, 0.f, idle)
                     .AddBlend2DSample(1.f, 0.f, run)
                     .Build(),
                 std::runtime_error);
}

TEST_F(AnimGraphSpec, EntryAndParametersPreserveDefaultsAndIgnoreUnknownNames)
{
    auto graph = Builder().Entry("run").Build();
    EXPECT_EQ(graph.GetSkeleton(), &skeleton);
    EXPECT_EQ(graph.CurrentStateName(), "run");
    EXPECT_FALSE(graph.IsBlending());
    EXPECT_FLOAT_EQ(graph.GetFloat("speed"), 0.f);
    EXPECT_FALSE(graph.GetBool("active"));
    graph.SetFloat("speed", 2.f);
    graph.SetBool("active", true);
    graph.SetFloat("missing", 9.f);
    graph.SetBool("missing", false);
    EXPECT_FLOAT_EQ(graph.GetFloat("speed"), 2.f);
    EXPECT_TRUE(graph.GetBool("active"));
    EXPECT_FLOAT_EQ(graph.GetFloat("missing", 7.f), 7.f);
    EXPECT_TRUE(graph.GetBool("missing", true));
}

TEST_F(AnimGraphSpec, FloatTransitionCrossfadesThenCommitsDestination)
{
    auto graph =
        Builder()
            .Transition("idle", "run",
                        fra::AnimCondition::FloatGreater("speed", 1.f), 1.f)
            .Build();
    graph.SetFloat("speed", 1.f);
    graph.Advance(0.1f);
    EXPECT_FALSE(graph.IsBlending());
    graph.SetFloat("speed", 2.f);
    const auto half = graph.Evaluate(0.5f);
    EXPECT_TRUE(graph.IsBlending());
    EXPECT_EQ(graph.CurrentStateName(), "idle");
    ASSERT_EQ(half.Size(), 4u);
    EXPECT_FLOAT_EQ(half.joints[0].translation.x, 4.f);
    graph.Advance(0.5f);
    EXPECT_FALSE(graph.IsBlending());
    EXPECT_EQ(graph.CurrentStateName(), "run");
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 8.f);
}

TEST_F(AnimGraphSpec, LessEqualConditionIncludesThreshold)
{
    auto graph =
        Builder()
            .Entry("run")
            .Transition("run", "idle",
                        fra::AnimCondition::FloatLessEqual("speed", 1.f), 0.f)
            .Build();
    graph.SetFloat("speed", 1.f);
    graph.Advance(0.1f);
    EXPECT_EQ(graph.CurrentStateName(), "idle");
}

TEST_F(AnimGraphSpec, BoolConditionsTransitionBothDirections)
{
    auto graph =
        Builder()
            .Transition("idle", "run", fra::AnimCondition::BoolTrue("active"),
                        0.f)
            .Transition("run", "idle", fra::AnimCondition::BoolFalse("active"),
                        0.f)
            .Build();
    graph.SetBool("active", true);
    graph.Advance(0.1f);
    EXPECT_EQ(graph.CurrentStateName(), "run");
    graph.SetBool("active", false);
    graph.Advance(0.1f);
    EXPECT_EQ(graph.CurrentStateName(), "idle");
}

TEST_F(AnimGraphSpec, TriggerIsConsumedAfterOneAdvance)
{
    auto graph =
        Builder()
            .Transition("idle", "run", fra::AnimCondition::OnTrigger("go"), 0.f)
            .Transition("run", "idle", fra::AnimCondition::OnTrigger("go"), 0.f)
            .Build();
    graph.SetTrigger("go");
    graph.Advance(0.1f);
    EXPECT_EQ(graph.CurrentStateName(), "run");
    graph.Advance(0.1f);
    EXPECT_EQ(graph.CurrentStateName(), "run");
    graph.SetTrigger("go");
    graph.Advance(0.1f);
    EXPECT_EQ(graph.CurrentStateName(), "idle");
}

TEST_F(AnimGraphSpec, AdvanceAndEvaluateProduceEquivalentPoses)
{
    const auto clip    = MakeLinearClip();
    auto       builder = [&] {
        return fra::AnimGraphBuilder {}
            .SetSkeleton(&skeleton)
            .State("move", clip, true, 2.f)
            .Build();
    };
    auto       a         = builder();
    auto       b         = builder();
    const auto evaluated = a.Evaluate(0.25f);
    b.Advance(0.25f);
    const auto sampled = b.SampleCurrent();
    ASSERT_EQ(sampled.Size(), 4u);
    EXPECT_FLOAT_EQ(sampled.joints[0].translation.x, 2.f);
    ExpectMatrixNear(sampled.joints[0].ToMatrix(),
                     evaluated.joints[0].ToMatrix());
}

TEST_F(AnimGraphSpec, EventsAppendAndKeepSourceClip)
{
    idle.events                                 = { { "step", 0.5f } };
    auto                                  graph = Builder().Build();
    std::vector<fra::FiredAnimationEvent> events;
    graph.Advance(0.5f, &events);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].name, "step");
    EXPECT_EQ(events[0].clip, &idle);
    graph.Advance(0.25f, &events);
    EXPECT_EQ(events.size(), 1u);
}

TEST_F(AnimGraphSpec, MaskedLayerCanBeWeightedAndDisabled)
{
    const auto mask = fra::BoneMask::Filled(4, 0.5f);
    auto       graph =
        Builder().Layer("overlay", run).LayerMasked(&mask, 0.5f).Build();
    EXPECT_EQ(graph.LayerCount(), 1u);
    EXPECT_FALSE(graph.IsLayerEnabled("overlay"));
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 0.f);
    graph.SetLayerEnabled("overlay", true);
    EXPECT_TRUE(graph.IsLayerEnabled("overlay"));
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 2.f);
    graph.SetLayerWeight("overlay", 1.f);
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 4.f);
    graph.SetLayerEnabled("overlay", false);
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 0.f);
    EXPECT_FALSE(graph.IsLayerEnabled("missing"));
}

TEST_F(AnimGraphSpec, AdditiveLayerUsesRestReferenceAndWeightParameter)
{
    auto graph =
        Builder()
            .ParamFloat("overlayWeight", 0.25f)
            .Layer("add", run)
            .LayerAdditive(1.f)
            .LayerWeightParam("overlayWeight")
            .Build();
    graph.SetLayerEnabled("add", true);
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 2.f);
    graph.SetFloat("overlayWeight", 0.5f);
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 4.f);
}

TEST_F(AnimGraphSpec, GpuLayersChooseFirstEnabledLayerPerMode)
{
    const auto mask = fra::BoneMask::Filled(4, 1.f);
    auto       graph =
        Builder()
            .Layer("first", run)
            .LayerMasked(&mask, 0.5f)
            .Layer("second", idle)
            .LayerMasked(&mask)
            .Layer("add", run)
            .LayerAdditive(0.25f)
            .Build();
    graph.SetLayerEnabled("first", true);
    graph.SetLayerEnabled("second", true);
    graph.SetLayerEnabled("add", true);
    fra::AnimLayerGpuSlots slots;
    ASSERT_TRUE(graph.TryGetLayerGpuSlots(slots));
    EXPECT_TRUE(slots.masked.active);
    EXPECT_EQ(slots.masked.clip, &run);
    EXPECT_FLOAT_EQ(slots.masked.weight, 0.5f);
    EXPECT_EQ(slots.additive.clip, &run);
    EXPECT_FLOAT_EQ(slots.additive.weight, 0.25f);
    graph.SetLayerEnabled("first", false);
    ASSERT_TRUE(graph.TryGetLayerGpuSlots(slots));
    EXPECT_EQ(slots.masked.clip, &idle);
}

TEST_F(AnimGraphSpec, Blend1DGpuWeightsAgreeWithCpuPose)
{
    auto graph =
        fra::AnimGraphBuilder {}
            .SetSkeleton(&skeleton)
            .ParamFloat("speed", 0.25f)
            .Blend1DState("loco", "speed")
            .AddBlendSample(0.f, idle)
            .AddBlendSample(1.f, run)
            .Build();
    fra::AnimLocoGpuSample sample;
    ASSERT_TRUE(graph.TryGetLocoGpuSample(sample));
    EXPECT_EQ(sample.clipA, &idle);
    EXPECT_EQ(sample.clipB, &run);
    EXPECT_FLOAT_EQ(sample.wA, 0.75f);
    EXPECT_FLOAT_EQ(sample.wB, 0.25f);
    EXPECT_FLOAT_EQ(sample.wC, 0.f);
    EXPECT_FLOAT_EQ(graph.SampleCurrent().joints[0].translation.x, 2.f);
}

TEST_F(AnimGraphSpec, BakingPreservesConstantPoseAfterBuilderDestruction)
{
    auto       graph = Builder().Entry("run").EnableBaking(4.f).Build();
    const auto pose  = graph.Evaluate(0.3f);
    ASSERT_EQ(pose.Size(), 4u);
    EXPECT_FLOAT_EQ(pose.joints[0].translation.x, 8.f);
}

TEST_F(AnimGraphSpec, DebugSnapshotAppliesOnlyEnabledFieldsAndClampsRange)
{
    auto graph = Builder().ParamFloat("ranged", 0.5f, 0.f, 1.f).Build();
    fra::AnimGraphDebugSnapshot snapshot;
    graph.CaptureDebugSnapshot(snapshot);
    for (auto& param : snapshot.floats)
        if (param.name == "ranged")
            param.value = 5.f;
    snapshot.applyFloats = false;
    graph.ApplyDebugSnapshot(snapshot);
    EXPECT_FLOAT_EQ(graph.GetFloat("ranged"), 0.5f);
    snapshot.applyFloats = true;
    graph.ApplyDebugSnapshot(snapshot);
    EXPECT_FLOAT_EQ(graph.GetFloat("ranged"), 1.f);
}
