#include <benchmark/benchmark.h>

#include <Freya/Asset/AnimGraph.hpp>
#include <Freya/Asset/AnimationClip.hpp>
#include <Freya/Asset/BakedAnimation.hpp>
#include <Freya/Asset/Pose.hpp>
#include <Freya/Asset/Rig.hpp>
#include <Freya/Asset/Skeleton.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace
{
    constexpr float kDuration = 2.f;

    // Chain skeleton branching at every 4th joint to approximate spine + limbs.
    fra::Skeleton MakeSkeleton(std::uint32_t jointCount)
    {
        fra::Skeleton sk;
        sk.names.reserve(jointCount);
        sk.parents.reserve(jointCount);
        sk.inverseBind.resize(jointCount, glm::mat4(1.f));
        sk.restLocal.resize(jointCount, glm::mat4(1.f));
        sk.nonBoneParent.resize(jointCount, glm::mat4(1.f));

        sk.names.push_back("root");
        sk.parents.push_back(-1);

        for (std::uint32_t i = 1; i < jointCount; ++i)
        {
            sk.names.push_back("joint_" + std::to_string(i));
            const std::int32_t parent =
                (i % 4 == 0) ? static_cast<std::int32_t>(i / 4 - 1)
                             : static_cast<std::int32_t>(i - 1);
            sk.parents.push_back(parent < 0 ? 0 : parent);

            glm::mat4 local(1.f);
            local[3][1]     = 0.1f * static_cast<float>(i);
            sk.restLocal[i] = local;
        }
        return sk;
    }

    fra::AnimationClip MakeClip(const fra::Skeleton& sk,
                                std::uint32_t        keysPerChannel)
    {
        fra::AnimationClip clip;
        clip.name           = "BenchClip";
        clip.duration       = kDuration;
        clip.ticksPerSecond = 25.f;

        const float dt =
            keysPerChannel > 1
                ? kDuration / static_cast<float>(keysPerChannel - 1)
                : 0.f;

        clip.channels.reserve(sk.JointCount());
        for (std::uint32_t j = 0; j < sk.JointCount(); ++j)
        {
            fra::AnimationChannel ch;
            ch.jointIndex = j;
            ch.translations.reserve(keysPerChannel);
            ch.rotations.reserve(keysPerChannel);
            ch.scales.reserve(keysPerChannel);
            for (std::uint32_t k = 0; k < keysPerChannel; ++k)
            {
                const float t   = static_cast<float>(k) * dt;
                const float ang = t * std::numbers::pi_v<float>;
                ch.translations.push_back(
                    { t, { 0.f, std::sin(ang) * 0.05f, 0.f } });
                ch.rotations.push_back(
                    { t,
                      glm::angleAxis(ang * 0.1f, glm::vec3(0.f, 1.f, 0.f)) });
                ch.scales.push_back({ t, { 1.f, 1.f, 1.f } });
            }
            clip.channels.push_back(std::move(ch));
        }
        return clip;
    }

} // namespace

// ---------------------------------------------------------------------------
// BM_BakeClip — pre-bake cost (setup / streaming scenario)
// ---------------------------------------------------------------------------
static void BM_BakeClip(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));
    const auto bakeHz     = static_cast<float>(state.range(1));

    const auto sk   = MakeSkeleton(jointCount);
    const auto clip = MakeClip(sk, 30);

    for (auto _ : state)
    {
        auto baked = fra::BakeClip(sk, clip, bakeHz);
        benchmark::DoNotOptimize(baked);
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_BakeClip)
    ->ArgNames({ "joints", "hz" })
    ->Args({ 24, 30 })
    ->Args({ 64, 30 })
    ->Args({ 128, 30 })
    ->Args({ 64, 60 })
    ->Args({ 128, 60 })
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_SampleClip — keyframe binary-search evaluation per frame
// ---------------------------------------------------------------------------
static void BM_SampleClip(benchmark::State& state)
{
    const auto jointCount     = static_cast<std::uint32_t>(state.range(0));
    const auto keysPerChannel = static_cast<std::uint32_t>(state.range(1));

    const auto sk   = MakeSkeleton(jointCount);
    const auto clip = MakeClip(sk, keysPerChannel);

    fra::LocalPose out;
    out.Resize(jointCount);
    float time = 0.f;

    for (auto _ : state)
    {
        fra::SampleClipInto(sk, clip, time, out);
        benchmark::DoNotOptimize(out);
        time += 0.016f;
        if (time > kDuration)
            time -= kDuration;
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_SampleClip)
    ->ArgNames({ "joints", "keys" })
    ->Args({ 24, 10 })
    ->Args({ 64, 10 })
    ->Args({ 128, 10 })
    ->Args({ 64, 30 })
    ->Args({ 128, 30 })
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_SampleBaked — dense frame-table lerp (cache-coherent path)
// ---------------------------------------------------------------------------
static void BM_SampleBaked(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));
    const auto bakeHz     = static_cast<float>(state.range(1));

    const auto sk    = MakeSkeleton(jointCount);
    const auto clip  = MakeClip(sk, 30);
    const auto baked = fra::BakeClip(sk, clip, bakeHz);

    float time = 0.f;

    for (auto _ : state)
    {
        auto result = fra::SampleBaked(sk, baked, time);
        benchmark::DoNotOptimize(result);
        time += 0.016f;
        if (time > kDuration)
            time -= kDuration;
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_SampleBaked)
    ->ArgNames({ "joints", "hz" })
    ->Args({ 24, 30 })
    ->Args({ 64, 30 })
    ->Args({ 128, 30 })
    ->Args({ 64, 60 })
    ->Args({ 128, 60 })
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_BlendLocalPoses — nlerp/slerp pair blend
// ---------------------------------------------------------------------------
static void BM_BlendLocalPoses(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));

    const auto sk    = MakeSkeleton(jointCount);
    const auto clip  = MakeClip(sk, 10);
    const auto baked = fra::BakeClip(sk, clip);
    const auto poseA = fra::SampleBaked(sk, baked, 0.2f);
    const auto poseB = fra::SampleBaked(sk, baked, 1.4f);

    float t = 0.f;
    for (auto _ : state)
    {
        auto result = fra::BlendLocalPoses(poseA, poseB, t);
        benchmark::DoNotOptimize(result);
        t += 0.01f;
        if (t > 1.f)
            t = 0.f;
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_BlendLocalPoses)
    ->ArgName("joints")
    ->Arg(24)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_BlendMasked — per-joint masked overlay layer
// ---------------------------------------------------------------------------
static void BM_BlendMasked(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));

    const auto sk          = MakeSkeleton(jointCount);
    const auto clip        = MakeClip(sk, 10);
    const auto baked       = fra::BakeClip(sk, clip);
    const auto poseBase    = fra::SampleBaked(sk, baked, 0.2f);
    const auto poseOverlay = fra::SampleBaked(sk, baked, 1.0f);
    const auto mask        = fra::BoneMask::Filled(jointCount, 0.5f);

    for (auto _ : state)
    {
        auto result = fra::BlendMasked(poseBase, poseOverlay, mask);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_BlendMasked)
    ->ArgName("joints")
    ->Arg(24)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_BlendAdditive — additive layer (delta from reference pose)
// ---------------------------------------------------------------------------
static void BM_BlendAdditive(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));

    const auto sk        = MakeSkeleton(jointCount);
    const auto clip      = MakeClip(sk, 10);
    const auto baked     = fra::BakeClip(sk, clip);
    const auto base      = fra::SampleBaked(sk, baked, 0.2f);
    const auto additive  = fra::SampleBaked(sk, baked, 1.0f);
    const auto reference = fra::RestLocalPose(sk);

    for (auto _ : state)
    {
        auto result = fra::BlendAdditive(base, additive, reference, 0.5f);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_BlendAdditive)
    ->ArgName("joints")
    ->Arg(24)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_LocalToGlobal — forward kinematics hierarchy pass
// ---------------------------------------------------------------------------
static void BM_LocalToGlobal(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));

    const auto sk    = MakeSkeleton(jointCount);
    const auto clip  = MakeClip(sk, 10);
    const auto baked = fra::BakeClip(sk, clip);
    const auto pose  = fra::SampleBaked(sk, baked, 0.5f);

    std::vector<glm::mat4> out(jointCount);

    for (auto _ : state)
    {
        fra::LocalToGlobalInto(sk, pose, out);
        benchmark::DoNotOptimize(out);
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_LocalToGlobal)
    ->ArgName("joints")
    ->Arg(24)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_PoseToSkinMatrices — global * inverseBind (GPU upload payload)
// ---------------------------------------------------------------------------
static void BM_PoseToSkinMatrices(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));

    const auto sk    = MakeSkeleton(jointCount);
    const auto clip  = MakeClip(sk, 10);
    const auto baked = fra::BakeClip(sk, clip);
    const auto pose  = fra::SampleBaked(sk, baked, 0.5f);

    std::vector<glm::mat4> out(jointCount);

    for (auto _ : state)
    {
        fra::PoseToSkinMatricesInto(sk, pose, out);
        benchmark::DoNotOptimize(out);
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_PoseToSkinMatrices)
    ->ArgName("joints")
    ->Arg(24)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_EvaluateBlend1D — speed blend tree (Idle / Walk / Run style)
// ---------------------------------------------------------------------------
static void BM_EvaluateBlend1D(benchmark::State& state)
{
    const auto jointCount  = static_cast<std::uint32_t>(state.range(0));
    const auto sampleCount = static_cast<std::uint32_t>(state.range(1));

    const auto sk = MakeSkeleton(jointCount);

    std::vector<fra::AnimationClip> clips(sampleCount);
    std::vector<fra::BakedClip>     bakes(sampleCount);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        clips[i]          = MakeClip(sk, 10);
        clips[i].duration = kDuration * (1.f + 0.5f * static_cast<float>(i));
        bakes[i]          = fra::BakeClip(sk, clips[i]);
    }

    std::vector<fra::Blend1DSample> samples(sampleCount);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        samples[i].value = static_cast<float>(i);
        samples[i].clip  = &clips[i];
        samples[i].baked = &bakes[i];
    }
    std::vector<float> times(sampleCount, 0.f);

    float param = 0.f;
    for (auto _ : state)
    {
        auto result = fra::EvaluateBlend1D(
            sk, std::span { samples }, std::span { times }, param);
        benchmark::DoNotOptimize(result);
        param += 0.05f;
        if (param > static_cast<float>(sampleCount - 1))
            param = 0.f;
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_EvaluateBlend1D)
    ->ArgNames({ "joints", "samples" })
    ->Args({ 64, 3 })
    ->Args({ 64, 5 })
    ->Args({ 128, 3 })
    ->Args({ 128, 5 })
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_EvaluateBlend2D — strafe × speed 2D blend space (4- and 9-point grids)
// ---------------------------------------------------------------------------
static void BM_EvaluateBlend2D(benchmark::State& state)
{
    const auto jointCount  = static_cast<std::uint32_t>(state.range(0));
    const auto sampleCount = static_cast<std::uint32_t>(state.range(1));

    const auto sk = MakeSkeleton(jointCount);

    std::vector<fra::AnimationClip> clips(sampleCount);
    std::vector<fra::BakedClip>     bakes(sampleCount);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        clips[i] = MakeClip(sk, 10);
        bakes[i] = fra::BakeClip(sk, clips[i]);
    }

    // Arrange on a uniform grid over [-1, 1]²
    const auto side = static_cast<std::uint32_t>(
        std::round(std::sqrt(static_cast<float>(sampleCount))));
    const float sideFrac = side > 1 ? static_cast<float>(side - 1) : 1.f;

    std::vector<fra::Blend2DSample> samples(sampleCount);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        const float x    = static_cast<float>(i % side) / sideFrac * 2.f - 1.f;
        const float y    = static_cast<float>(i / side) / sideFrac * 2.f - 1.f;
        samples[i].pos   = { x, y };
        samples[i].clip  = &clips[i];
        samples[i].baked = &bakes[i];
    }
    std::vector<float> times(sampleCount, 0.f);

    glm::vec2 param { 0.f };
    for (auto _ : state)
    {
        auto result = fra::EvaluateBlend2D(
            sk, std::span { samples }, std::span { times }, param);
        benchmark::DoNotOptimize(result);
        param.x += 0.05f;
        if (param.x > 1.f)
            param.x = -1.f;
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_EvaluateBlend2D)
    ->ArgNames({ "joints", "samples" })
    ->Args({ 64, 4 })
    ->Args({ 64, 9 })
    ->Args({ 128, 4 })
    ->Args({ 128, 9 })
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_AnimGraph_Evaluate — full state-machine tick (Blend1D locomotion state)
// ---------------------------------------------------------------------------
static void BM_AnimGraph_Evaluate(benchmark::State& state)
{
    const auto jointCount  = static_cast<std::uint32_t>(state.range(0));
    const auto sampleCount = static_cast<std::uint32_t>(state.range(1));

    const auto sk = MakeSkeleton(jointCount);

    std::vector<fra::AnimationClip> clips(sampleCount);
    std::vector<fra::BakedClip>     bakes(sampleCount);
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        clips[i]          = MakeClip(sk, 10);
        clips[i].duration = kDuration * (1.f + 0.5f * static_cast<float>(i));
        bakes[i]          = fra::BakeClip(sk, clips[i]);
    }

    fra::AnimGraphBuilder builder;
    builder.SetSkeleton(&sk).ParamFloat("Speed");
    builder.Blend1DState("Locomotion", "Speed");
    for (std::uint32_t i = 0; i < sampleCount; ++i)
    {
        builder.AddBlendSample(
            static_cast<float>(i), clips[i], true, 1.f, &bakes[i]);
    }
    builder.Entry("Locomotion");

    auto graph = builder.Build();

    constexpr float kDt = 0.016f;
    for (auto _ : state)
    {
        auto pose = graph.Evaluate(kDt);
        benchmark::DoNotOptimize(pose);
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_AnimGraph_Evaluate)
    ->ArgNames({ "joints", "samples" })
    ->Args({ 64, 3 })
    ->Args({ 64, 5 })
    ->Args({ 128, 3 })
    ->Args({ 128, 5 })
    ->Unit(benchmark::kMicrosecond);

// ---------------------------------------------------------------------------
// BM_FullAnimPipeline — SampleBaked + PoseToSkinMatrices end-to-end
// ---------------------------------------------------------------------------
static void BM_FullAnimPipeline(benchmark::State& state)
{
    const auto jointCount = static_cast<std::uint32_t>(state.range(0));

    const auto sk    = MakeSkeleton(jointCount);
    const auto clip  = MakeClip(sk, 30);
    const auto baked = fra::BakeClip(sk, clip, 60.f);

    std::vector<glm::mat4> skinOut(jointCount);
    float                  time = 0.f;

    for (auto _ : state)
    {
        const auto pose = fra::SampleBaked(sk, baked, time);
        fra::PoseToSkinMatricesInto(sk, pose, skinOut);
        benchmark::DoNotOptimize(skinOut);
        time += 0.016f;
        if (time > kDuration)
            time -= kDuration;
    }
    state.SetItemsProcessed(state.iterations() * jointCount);
}
BENCHMARK(BM_FullAnimPipeline)
    ->ArgName("joints")
    ->Arg(24)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
