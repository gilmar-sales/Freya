#include "Freya/Asset/AnimGraph.hpp"
#include "Freya/Asset/BakedAnimation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace FREYA_NAMESPACE
{
    void AnimGraph::SetFloat(const std::string_view name, const float value)
    {
        const std::string key { name };
        if (auto it = mFloats.find(key); it != mFloats.end())
            it->second.value = value;
    }

    void AnimGraph::SetBool(const std::string_view name, const bool value)
    {
        const std::string key { name };
        if (auto it = mBools.find(key); it != mBools.end())
            it->second.value = value;
    }

    void AnimGraph::SetTrigger(const std::string_view name)
    {
        const std::string key { name };
        if (auto it = mTriggers.find(key); it != mTriggers.end())
            it->second.raised = true;
    }

    float AnimGraph::GetFloat(const std::string_view name,
                              const float            fallback) const
    {
        const std::string key { name };
        if (const auto it = mFloats.find(key); it != mFloats.end())
            return it->second.value;
        return fallback;
    }

    bool AnimGraph::GetBool(const std::string_view name,
                            const bool             fallback) const
    {
        const std::string key { name };
        if (const auto it = mBools.find(key); it != mBools.end())
            return it->second.value;
        return fallback;
    }

    std::int32_t AnimGraph::findLayer(const std::string_view name) const
    {
        for (std::uint32_t i = 0; i < mLayers.size(); ++i)
        {
            if (mLayers[i].name == name)
                return static_cast<std::int32_t>(i);
        }
        return -1;
    }

    void AnimGraph::SetLayerEnabled(const std::string_view name,
                                    const bool             enabled)
    {
        const auto idx = findLayer(name);
        if (idx >= 0)
            mLayers[static_cast<std::uint32_t>(idx)].enabled = enabled;
    }

    void AnimGraph::SetLayerWeight(const std::string_view name,
                                   const float            weight)
    {
        const auto idx = findLayer(name);
        if (idx >= 0)
            mLayers[static_cast<std::uint32_t>(idx)].weight = weight;
    }

    bool AnimGraph::IsLayerEnabled(const std::string_view name) const
    {
        const auto idx = findLayer(name);
        return idx >= 0 && mLayers[static_cast<std::uint32_t>(idx)].enabled;
    }

    float AnimGraph::effectiveLayerWeight(const Layer& layer) const
    {
        if (!layer.enabled)
            return 0.f;
        float w = layer.weight;
        if (!layer.weightParam.empty())
            w *= GetFloat(layer.weightParam, 1.f);
        return std::clamp(w, 0.f, 1.f);
    }

    bool AnimGraph::evaluateCondition(const AnimCondition& c) const
    {
        switch (c.kind)
        {
            case AnimCondition::Kind::FloatGreater:
                return GetFloat(c.param) > c.threshold;
            case AnimCondition::Kind::FloatLessEqual:
                return GetFloat(c.param) <= c.threshold;
            case AnimCondition::Kind::BoolTrue:
                return GetBool(c.param);
            case AnimCondition::Kind::BoolFalse:
                return !GetBool(c.param);
            case AnimCondition::Kind::Trigger: {
                const auto it = mTriggers.find(c.param);
                return it != mTriggers.end() && it->second.raised;
            }
        }
        return false;
    }

    void AnimGraph::clearTriggers()
    {
        for (auto& [_, t] : mTriggers)
            t.raised = false;
    }

    void AnimGraph::tryStartTransition()
    {
        if (mBlending || mStates.empty())
            return;

        for (const auto& tr : mTransitions)
        {
            if (tr.from != mCurrentState)
                continue;
            if (!evaluateCondition(tr.condition))
                continue;

            mNextState     = tr.to;
            mNextTime      = 0.f;
            mBlendDuration = std::max(tr.blendDuration, 1e-4f);
            mBlendElapsed  = 0.f;
            mBlending      = true;
            break;
        }
    }

    namespace
    {
        void collectDominantBlendEvents(
            const std::vector<Blend1DSample>& samples,
            const std::vector<float>&         prevTimes,
            const std::vector<float>& currTimes, const float param,
            std::vector<FiredAnimationEvent>* outEvents)
        {
            if (!outEvents || samples.empty())
                return;

            std::vector<float> values(samples.size());
            for (std::uint32_t i = 0; i < samples.size(); ++i)
                values[i] = samples[i].value;
            const auto span = ResolveBlend1D(values, param);

            const auto emit = [&](const std::uint32_t idx) {
                if (idx >= samples.size() || !samples[idx].clip)
                    return;
                const float t0 = idx < prevTimes.size() ? prevTimes[idx] : 0.f;
                const float t1 = idx < currTimes.size() ? currTimes[idx] : 0.f;
                CollectFiredClipEvents(
                    *samples[idx].clip, t0, t1, samples[idx].loop, *outEvents);
            };

            if (span.i0 == span.i1 || span.t < 0.5f)
                emit(span.i0);
            else if (span.t > 0.5f)
                emit(span.i1);
            else
            {
                emit(span.i0);
                emit(span.i1);
            }
        }
    } // namespace

    void AnimGraph::advanceState(State& state, float& clipTime, const float dt,
                                 std::vector<FiredAnimationEvent>* outEvents)
    {
        if (state.kind == StateKind::Blend1D)
        {
            if (state.blendTimes.size() != state.blendSamples.size())
                state.blendTimes.assign(state.blendSamples.size(), 0.f);

            const float        param     = GetFloat(state.blendParam);
            std::vector<float> prevTimes = state.blendTimes;
            const float        prevPhase = state.blendPhase;

            if (state.syncPhase)
            {
                state.blendPhase = AdvanceBlend1DPhase(
                    state.blendSamples, param, state.blendPhase, dt);
                WriteBlend1DTimesFromPhase(
                    state.blendSamples, state.blendTimes, state.blendPhase);
                WriteBlend1DTimesFromPhase(
                    state.blendSamples, prevTimes, prevPhase);
            }
            else
                AdvanceBlend1DTimes(state.blendSamples, state.blendTimes, dt);

            collectDominantBlendEvents(state.blendSamples, prevTimes,
                                       state.blendTimes, param, outEvents);
            return;
        }

        if (state.kind == StateKind::Blend2D)
        {
            if (state.blendTimes.size() != state.blend2DSamples.size())
                state.blendTimes.assign(state.blend2DSamples.size(), 0.f);

            const glm::vec2 param { GetFloat(state.blendParam),
                                    GetFloat(state.blendParamY) };
            if (state.syncPhase)
            {
                state.blendPhase = AdvanceBlend2DPhase(
                    state.blend2DSamples, param, state.blendPhase, dt);
                WriteBlend2DTimesFromPhase(
                    state.blend2DSamples, state.blendTimes, state.blendPhase);
            }
            else
                AdvanceBlend2DTimes(state.blend2DSamples, state.blendTimes, dt);
            return;
        }

        const float tPrev = clipTime;
        const float rate  = std::max(state.playbackSpeed, 0.f);
        clipTime += dt * rate;
        if (state.clip && state.clip->duration > 0.f && state.loop)
        {
            clipTime = std::fmod(clipTime, state.clip->duration);
            if (clipTime < 0.f)
                clipTime += state.clip->duration;
        }

        if (outEvents && state.clip)
            CollectFiredClipEvents(
                *state.clip, tPrev, clipTime, state.loop, *outEvents);
    }

    void AnimGraph::advanceLayers(const float dt)
    {
        for (auto& layer : mLayers)
        {
            if (!layer.clip && !(layer.baked && !layer.baked->Empty()))
                continue;
            const float dur  = layer.baked && !layer.baked->Empty()
                                   ? layer.baked->duration
                                   : (layer.clip ? layer.clip->duration : 0.f);
            const float rate = std::max(layer.playbackSpeed, 0.f);
            layer.time += dt * rate;
            if (dur > 0.f && layer.loop)
            {
                layer.time = std::fmod(layer.time, dur);
                if (layer.time < 0.f)
                    layer.time += dur;
            }
        }
    }

    LocalPose AnimGraph::sampleState(const State& state,
                                     const float  clipTime) const
    {
        if (!mSkeleton)
            return {};

        if (state.kind == StateKind::Blend1D)
        {
            std::vector<float> times = state.blendTimes;
            return EvaluateBlend1D(*mSkeleton, state.blendSamples, times,
                                   GetFloat(state.blendParam));
        }
        if (state.kind == StateKind::Blend2D)
        {
            std::vector<float> times = state.blendTimes;
            return EvaluateBlend2D(
                *mSkeleton, state.blend2DSamples, times,
                { GetFloat(state.blendParam), GetFloat(state.blendParamY) });
        }

        if (state.baked && !state.baked->Empty())
            return SampleBaked(*mSkeleton, *state.baked, clipTime, state.loop);
        return state.clip
                   ? SampleClip(*mSkeleton, *state.clip, clipTime, state.loop)
                   : RestLocalPose(*mSkeleton);
    }

    LocalPose AnimGraph::sampleLayer(const Layer& layer) const
    {
        if (!mSkeleton)
            return {};
        if (layer.baked && !layer.baked->Empty())
            return SampleBaked(*mSkeleton, *layer.baked, layer.time,
                               layer.loop);
        return layer.clip
                   ? SampleClip(*mSkeleton, *layer.clip, layer.time, layer.loop)
                   : RestLocalPose(*mSkeleton);
    }

    LocalPose AnimGraph::applyLayers(LocalPose base) const
    {
        if (!mSkeleton || mLayers.empty())
            return base;

        const LocalPose rest = RestLocalPose(*mSkeleton);
        for (const auto& layer : mLayers)
        {
            const float w = effectiveLayerWeight(layer);
            if (w <= 1e-6f)
                continue;
            const LocalPose overlay = sampleLayer(layer);
            if (layer.mode == AnimLayerMode::Additive)
            {
                if (layer.mask)
                    base = BlendAdditive(base, overlay, rest, *layer.mask, w);
                else
                    base = BlendAdditive(base, overlay, rest, w);
            }
            else if (layer.mask)
                base = BlendMasked(base, overlay, *layer.mask, w);
        }
        return base;
    }

    LocalPose AnimGraph::evaluateState(
        State& state, float& clipTime, const float dt,
        std::vector<FiredAnimationEvent>* outEvents)
    {
        advanceState(state, clipTime, dt, outEvents);
        return sampleState(state, clipTime);
    }

    void AnimGraph::Advance(const float                       dt,
                            std::vector<FiredAnimationEvent>* outEvents)
    {
        if (!mSkeleton || mStates.empty())
            return;

        tryStartTransition();

        advanceState(mStates[mCurrentState], mCurrentTime, dt,
                     mBlending ? nullptr : outEvents);

        if (!mBlending)
        {
            advanceLayers(dt);
            clearTriggers();
            return;
        }

        advanceState(mStates[mNextState], mNextTime, dt, outEvents);

        mBlendElapsed += dt;
        const float alpha =
            std::clamp(mBlendElapsed / mBlendDuration, 0.f, 1.f);
        if (alpha >= 1.f)
        {
            mCurrentState = mNextState;
            mCurrentTime  = mNextTime;
            mBlending     = false;
        }

        advanceLayers(dt);
        clearTriggers();
    }

    LocalPose AnimGraph::SampleCurrent() const
    {
        if (!mSkeleton || mStates.empty())
            return {};

        LocalPose fromPose = sampleState(mStates[mCurrentState], mCurrentTime);
        if (!mBlending)
            return applyLayers(std::move(fromPose));

        const LocalPose toPose = sampleState(mStates[mNextState], mNextTime);
        const float     alpha =
            std::clamp(mBlendElapsed / mBlendDuration, 0.f, 1.f);
        return applyLayers(BlendLocalPoses(fromPose, toPose, alpha));
    }

    LocalPose AnimGraph::Evaluate(const float                       dt,
                                  std::vector<FiredAnimationEvent>* outEvents)
    {
        Advance(dt, outEvents);
        return SampleCurrent();
    }

    std::string_view AnimGraph::CurrentStateName() const
    {
        if (mStates.empty() || mCurrentState >= mStates.size())
            return {};
        return mStates[mCurrentState].name;
    }

    bool AnimGraph::TryGetLocoGpuSample(AnimLocoGpuSample& out) const
    {
        out = {};
        if (!mSkeleton || mStates.empty() || mCurrentState >= mStates.size())
            return false;
        const auto& state = mStates[mCurrentState];

        if (state.kind == StateKind::Blend1D && !state.blendSamples.empty())
        {
            std::vector<float> values(state.blendSamples.size());
            for (std::uint32_t i = 0; i < state.blendSamples.size(); ++i)
                values[i] = state.blendSamples[i].value;
            const float param = GetFloat(state.blendParam);
            const auto  span  = ResolveBlend1D(values, param);
            const auto& s0    = state.blendSamples[span.i0];
            const auto& s1    = state.blendSamples[span.i1];
            out.clipA         = s0.clip;
            out.clipB         = s1.clip;
            out.clipC         = s0.clip;
            out.timeA         = span.i0 < state.blendTimes.size()
                                    ? state.blendTimes[span.i0]
                                    : 0.f;
            out.timeB         = span.i1 < state.blendTimes.size()
                                    ? state.blendTimes[span.i1]
                                    : 0.f;
            out.timeC         = out.timeA;
            out.wA            = 1.f - span.t;
            out.wB            = span.t;
            out.wC            = 0.f;
            return out.clipA != nullptr;
        }

        if (state.kind == StateKind::Blend2D && !state.blend2DSamples.empty())
        {
            std::vector<glm::vec2> positions(state.blend2DSamples.size());
            for (std::uint32_t i = 0; i < state.blend2DSamples.size(); ++i)
                positions[i] = state.blend2DSamples[i].pos;
            const glm::vec2 param { GetFloat(state.blendParam),
                                    GetFloat(state.blendParamY) };
            const auto      tri = ResolveBlend2D(positions, param);
            const auto&     s0  = state.blend2DSamples[tri.i0];
            const auto&     s1  = state.blend2DSamples[tri.i1];
            const auto&     s2  = state.blend2DSamples[tri.i2];
            out.clipA           = s0.clip;
            out.clipB           = s1.clip;
            out.clipC           = s2.clip;
            out.timeA           = tri.i0 < state.blendTimes.size()
                                      ? state.blendTimes[tri.i0]
                                      : 0.f;
            out.timeB           = tri.i1 < state.blendTimes.size()
                                      ? state.blendTimes[tri.i1]
                                      : 0.f;
            out.timeC           = tri.i2 < state.blendTimes.size()
                                      ? state.blendTimes[tri.i2]
                                      : 0.f;
            out.wA              = tri.w0;
            out.wB              = tri.w1;
            out.wC              = tri.w2;
            return out.clipA != nullptr;
        }
        return false;
    }

    bool AnimGraph::TryGetBlend1DGpuSample(
        const AnimationClip*& clipA, float& timeA, const AnimationClip*& clipB,
        float& timeB, float& blendT) const
    {
        AnimLocoGpuSample loco {};
        if (!TryGetLocoGpuSample(loco) || loco.wC > 1e-6f)
        {
            clipA = clipB = nullptr;
            timeA = timeB = blendT = 0.f;
            return false;
        }
        clipA  = loco.clipA;
        clipB  = loco.clipB;
        timeA  = loco.timeA;
        timeB  = loco.timeB;
        blendT = loco.wB;
        return clipA != nullptr;
    }

    bool AnimGraph::TryGetLayerGpuSlots(AnimLayerGpuSlots& out) const
    {
        out = {};
        for (const auto& layer : mLayers)
        {
            const float w = effectiveLayerWeight(layer);
            if (w <= 1e-6f || !layer.clip)
                continue;
            AnimLayerGpuSample* dest = nullptr;
            if (layer.mode == AnimLayerMode::Additive)
            {
                if (out.additive.active)
                    continue;
                dest = &out.additive;
            }
            else
            {
                if (out.masked.active)
                    continue;
                dest = &out.masked;
            }
            dest->active = true;
            dest->mode   = layer.mode;
            dest->clip   = layer.clip;
            dest->time   = layer.time;
            dest->weight = w;
            if (out.masked.active && out.additive.active)
                break;
        }
        return out.masked.active || out.additive.active;
    }

    bool AnimGraph::TryGetPrimaryLayerGpuSample(AnimLayerGpuSample& out) const
    {
        AnimLayerGpuSlots slots {};
        if (!TryGetLayerGpuSlots(slots))
        {
            out = {};
            return false;
        }
        out = slots.masked.active ? slots.masked : slots.additive;
        return out.active;
    }

    AnimGraphBuilder& AnimGraphBuilder::SetSkeleton(const Skeleton* skeleton)
    {
        mSkeleton = skeleton;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::ParamFloat(std::string name,
                                                   const float defaultValue)
    {
        mGraph.mFloats[std::move(name)] =
            AnimGraph::FloatParam { defaultValue };
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::ParamBool(std::string name,
                                                  const bool  defaultValue)
    {
        mGraph.mBools[std::move(name)] = AnimGraph::BoolParam { defaultValue };
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::ParamTrigger(std::string name)
    {
        mGraph.mTriggers[std::move(name)] = AnimGraph::TriggerParam {};
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::State(std::string          name,
                                              const AnimationClip& clip,
                                              const bool           loop,
                                              const float playbackSpeed)
    {
        AnimGraph::State s;
        s.name          = std::move(name);
        s.kind          = AnimGraph::StateKind::Clip;
        s.clip          = &clip;
        s.loop          = loop;
        s.playbackSpeed = playbackSpeed;
        mGraph.mStates.push_back(std::move(s));
        mLastBlendState = -1;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::Blend1DState(
        std::string name, std::string param, const bool syncPhase)
    {
        AnimGraph::State s;
        s.name       = std::move(name);
        s.kind       = AnimGraph::StateKind::Blend1D;
        s.blendParam = std::move(param);
        s.syncPhase  = syncPhase;
        mGraph.mStates.push_back(std::move(s));
        mLastBlendState = static_cast<std::int32_t>(mGraph.mStates.size() - 1);
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::AddBlendSample(
        const float value, const AnimationClip& clip, const bool loop,
        const float playbackSpeed, const BakedClip* baked)
    {
        if (mLastBlendState < 0)
            throw std::runtime_error(
                "AnimGraphBuilder: AddBlendSample without Blend1DState");

        auto& st = mGraph.mStates[static_cast<std::uint32_t>(mLastBlendState)];
        if (!st.blendSamples.empty() && value < st.blendSamples.back().value)
            throw std::runtime_error(
                "AnimGraphBuilder: Blend1D samples must be ascending");

        Blend1DSample sample;
        sample.value         = value;
        sample.clip          = &clip;
        sample.baked         = baked;
        sample.loop          = loop;
        sample.playbackSpeed = playbackSpeed;
        st.blendSamples.push_back(sample);
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::Blend2DState(std::string name,
                                                     std::string paramX,
                                                     std::string paramY,
                                                     const bool  syncPhase)
    {
        AnimGraph::State s;
        s.name        = std::move(name);
        s.kind        = AnimGraph::StateKind::Blend2D;
        s.blendParam  = std::move(paramX);
        s.blendParamY = std::move(paramY);
        s.syncPhase   = syncPhase;
        mGraph.mStates.push_back(std::move(s));
        mLastBlendState = static_cast<std::int32_t>(mGraph.mStates.size() - 1);
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::AddBlend2DSample(
        const float x, const float y, const AnimationClip& clip,
        const bool loop, const float playbackSpeed, const BakedClip* baked)
    {
        if (mLastBlendState < 0)
            throw std::runtime_error(
                "AnimGraphBuilder: AddBlend2DSample without Blend2DState");

        auto& st = mGraph.mStates[static_cast<std::uint32_t>(mLastBlendState)];
        if (st.kind != AnimGraph::StateKind::Blend2D)
            throw std::runtime_error(
                "AnimGraphBuilder: AddBlend2DSample on non-Blend2D state");

        Blend2DSample sample;
        sample.pos           = { x, y };
        sample.clip          = &clip;
        sample.baked         = baked;
        sample.loop          = loop;
        sample.playbackSpeed = playbackSpeed;
        st.blend2DSamples.push_back(sample);
        return *this;
    }

    AnimGraph::Layer& AnimGraphBuilder::lastLayer()
    {
        if (mLastLayer < 0)
            throw std::runtime_error(
                "AnimGraphBuilder: layer op without Layer()");
        return mGraph.mLayers[static_cast<std::uint32_t>(mLastLayer)];
    }

    AnimGraphBuilder& AnimGraphBuilder::Layer(std::string          name,
                                              const AnimationClip& clip,
                                              const bool           loop,
                                              const float playbackSpeed)
    {
        AnimGraph::Layer layer;
        layer.name          = std::move(name);
        layer.clip          = &clip;
        layer.loop          = loop;
        layer.playbackSpeed = playbackSpeed;
        layer.enabled       = false; // opt-in via SetLayerEnabled
        mGraph.mLayers.push_back(std::move(layer));
        mLastLayer      = static_cast<std::int32_t>(mGraph.mLayers.size() - 1);
        mLastBlendState = -1;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::LayerMasked(const BoneMask* mask,
                                                    const float     weight)
    {
        auto& layer  = lastLayer();
        layer.mode   = AnimLayerMode::OverrideMasked;
        layer.mask   = mask;
        layer.weight = weight;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::LayerAdditive(const float     weight,
                                                      const BoneMask* mask)
    {
        auto& layer  = lastLayer();
        layer.mode   = AnimLayerMode::Additive;
        layer.mask   = mask;
        layer.weight = weight;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::LayerWeightParam(std::string floatParam)
    {
        lastLayer().weightParam = std::move(floatParam);
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::LayerBake(const BakedClip* baked)
    {
        lastLayer().baked = baked;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::EnableBaking(const float bakeHz)
    {
        mBakeHz = bakeHz;
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::Entry(const std::string_view stateName)
    {
        mEntryName = std::string { stateName };
        return *this;
    }

    AnimGraphBuilder& AnimGraphBuilder::Transition(
        const std::string_view from, const std::string_view to,
        AnimCondition condition, const float blendDuration)
    {
        const auto fi = findState(from);
        const auto ti = findState(to);
        if (fi < 0 || ti < 0)
            throw std::runtime_error("AnimGraphBuilder: unknown state in "
                                     "Transition");

        AnimGraph::Transition tr;
        tr.from          = static_cast<std::uint32_t>(fi);
        tr.to            = static_cast<std::uint32_t>(ti);
        tr.condition     = std::move(condition);
        tr.blendDuration = blendDuration;
        mGraph.mTransitions.push_back(std::move(tr));
        return *this;
    }

    std::int32_t AnimGraphBuilder::findState(const std::string_view name) const
    {
        for (std::uint32_t i = 0; i < mGraph.mStates.size(); ++i)
        {
            if (mGraph.mStates[i].name == name)
                return static_cast<std::int32_t>(i);
        }
        return -1;
    }

    AnimGraph AnimGraphBuilder::Build()
    {
        if (!mSkeleton)
            throw std::runtime_error("AnimGraphBuilder: skeleton required");
        if (mGraph.mStates.empty())
            throw std::runtime_error("AnimGraphBuilder: no states");

        for (auto& st : mGraph.mStates)
        {
            if (st.kind == AnimGraph::StateKind::Blend1D)
            {
                if (st.blendSamples.size() < 2)
                    throw std::runtime_error(
                        "AnimGraphBuilder: Blend1D needs >= 2 samples");
                st.blendTimes.assign(st.blendSamples.size(), 0.f);
            }
            if (st.kind == AnimGraph::StateKind::Blend2D)
            {
                if (st.blend2DSamples.size() < 3)
                    throw std::runtime_error(
                        "AnimGraphBuilder: Blend2D needs >= 3 samples");
                st.blendTimes.assign(st.blend2DSamples.size(), 0.f);
            }
        }

        mGraph.mSkeleton = mSkeleton;

        if (mBakeHz > 0.f)
        {
            // Only bake clips that do not already have an external bake.
            std::vector<const AnimationClip*> ordered;
            ordered.reserve(8);
            auto consider = [&](const AnimationClip* clip,
                                const BakedClip*     existing) {
                if (!clip || existing)
                    return;
                if (std::find(ordered.begin(), ordered.end(), clip) ==
                    ordered.end())
                    ordered.push_back(clip);
            };
            for (const auto& st : mGraph.mStates)
            {
                consider(st.clip, st.baked);
                for (const auto& s : st.blendSamples)
                    consider(s.clip, s.baked);
                for (const auto& s : st.blend2DSamples)
                    consider(s.clip, s.baked);
            }
            for (const auto& layer : mGraph.mLayers)
                consider(layer.clip, layer.baked);

            mGraph.mBakedClips.clear();
            mGraph.mBakedClips.reserve(ordered.size());
            std::unordered_map<const AnimationClip*, std::uint32_t> index;
            index.reserve(ordered.size());
            for (const AnimationClip* clip : ordered)
            {
                const auto idx =
                    static_cast<std::uint32_t>(mGraph.mBakedClips.size());
                mGraph.mBakedClips.push_back(
                    BakeClip(*mSkeleton, *clip, mBakeHz));
                index.emplace(clip, idx);
            }

            auto resolve = [&](const AnimationClip* clip) -> const BakedClip* {
                if (!clip)
                    return nullptr;
                const auto it = index.find(clip);
                return it == index.end() ? nullptr
                                         : &mGraph.mBakedClips[it->second];
            };
            for (auto& st : mGraph.mStates)
            {
                if (st.kind == AnimGraph::StateKind::Clip && !st.baked)
                    st.baked = resolve(st.clip);
                for (auto& s : st.blendSamples)
                {
                    if (!s.baked)
                        s.baked = resolve(s.clip);
                }
                for (auto& s : st.blend2DSamples)
                {
                    if (!s.baked)
                        s.baked = resolve(s.clip);
                }
            }
            for (auto& layer : mGraph.mLayers)
            {
                if (!layer.baked)
                    layer.baked = resolve(layer.clip);
            }
        }

        if (!mEntryName.empty())
        {
            const auto idx = findState(mEntryName);
            if (idx < 0)
                throw std::runtime_error("AnimGraphBuilder: entry state not "
                                         "found");
            mGraph.mEntryState   = static_cast<std::uint32_t>(idx);
            mGraph.mCurrentState = mGraph.mEntryState;
        }
        else
        {
            mGraph.mEntryState   = 0;
            mGraph.mCurrentState = 0;
        }
        return std::move(mGraph);
    }

} // namespace FREYA_NAMESPACE
