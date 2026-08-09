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

    LocalPose AnimGraph::evaluateState(
        State& state, float& clipTime, const float dt,
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
                // Rebuild prev times from previous phase for accurate windows.
                WriteBlend1DTimesFromPhase(
                    state.blendSamples, prevTimes, prevPhase);
            }
            else
                AdvanceBlend1DTimes(state.blendSamples, state.blendTimes, dt);

            collectDominantBlendEvents(state.blendSamples, prevTimes,
                                       state.blendTimes, param, outEvents);

            return EvaluateBlend1D(
                *mSkeleton, state.blendSamples, state.blendTimes, param);
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

        if (state.baked && !state.baked->Empty())
            return SampleBaked(*mSkeleton, *state.baked, clipTime, state.loop);
        return state.clip
                   ? SampleClip(*mSkeleton, *state.clip, clipTime, state.loop)
                   : RestLocalPose(*mSkeleton);
    }

    LocalPose AnimGraph::Evaluate(const float                       dt,
                                  std::vector<FiredAnimationEvent>* outEvents)
    {
        if (!mSkeleton || mStates.empty())
            return {};

        tryStartTransition();

        LocalPose fromPose = evaluateState(mStates[mCurrentState], mCurrentTime,
                                           dt, mBlending ? nullptr : outEvents);

        if (!mBlending)
        {
            clearTriggers();
            return fromPose;
        }

        // Crossfade: emit markers from the incoming state only.
        LocalPose toPose =
            evaluateState(mStates[mNextState], mNextTime, dt, outEvents);

        mBlendElapsed += dt;
        const float alpha =
            std::clamp(mBlendElapsed / mBlendDuration, 0.f, 1.f);
        LocalPose blended = BlendLocalPoses(fromPose, toPose, alpha);

        if (alpha >= 1.f)
        {
            mCurrentState = mNextState;
            mCurrentTime  = mNextTime;
            mBlending     = false;
        }

        clearTriggers();
        return blended;
    }

    std::string_view AnimGraph::CurrentStateName() const
    {
        if (mStates.empty() || mCurrentState >= mStates.size())
            return {};
        return mStates[mCurrentState].name;
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
            }

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
