#include "Freya/Asset/AnimGraph.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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

    LocalPose AnimGraph::Evaluate(const float dt)
    {
        if (!mSkeleton || mStates.empty())
            return {};

        tryStartTransition();

        auto&       curState = mStates[mCurrentState];
        const float curRate  = std::max(curState.playbackSpeed, 0.f);
        mCurrentTime += dt * curRate;
        if (curState.clip && curState.clip->duration > 0.f && curState.loop)
        {
            mCurrentTime = std::fmod(mCurrentTime, curState.clip->duration);
            if (mCurrentTime < 0.f)
                mCurrentTime += curState.clip->duration;
        }

        LocalPose fromPose =
            curState.clip ? SampleClip(*mSkeleton, *curState.clip, mCurrentTime,
                                       curState.loop)
                          : RestLocalPose(*mSkeleton);

        if (!mBlending)
        {
            clearTriggers();
            return fromPose;
        }

        auto&       nextState = mStates[mNextState];
        const float nextRate  = std::max(nextState.playbackSpeed, 0.f);
        mNextTime += dt * nextRate;
        if (nextState.clip && nextState.clip->duration > 0.f && nextState.loop)
        {
            mNextTime = std::fmod(mNextTime, nextState.clip->duration);
            if (mNextTime < 0.f)
                mNextTime += nextState.clip->duration;
        }

        LocalPose toPose =
            nextState.clip ? SampleClip(*mSkeleton, *nextState.clip, mNextTime,
                                        nextState.loop)
                           : RestLocalPose(*mSkeleton);

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
        s.clip          = &clip;
        s.loop          = loop;
        s.playbackSpeed = playbackSpeed;
        mGraph.mStates.push_back(std::move(s));
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

        mGraph.mSkeleton = mSkeleton;
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
