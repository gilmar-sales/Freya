#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Transition predicate for AnimGraph.
     */
    struct AnimCondition
    {
        enum class Kind : std::uint8_t
        {
            FloatGreater,
            FloatLessEqual,
            BoolTrue,
            BoolFalse,
            Trigger,
        };

        Kind        kind = Kind::Trigger;
        std::string param;
        float       threshold = 0.f;

        static AnimCondition FloatGreater(std::string name, float value)
        {
            return { Kind::FloatGreater, std::move(name), value };
        }

        static AnimCondition FloatLessEqual(std::string name, float value)
        {
            return { Kind::FloatLessEqual, std::move(name), value };
        }

        static AnimCondition BoolTrue(std::string name)
        {
            return { Kind::BoolTrue, std::move(name), 0.f };
        }

        static AnimCondition BoolFalse(std::string name)
        {
            return { Kind::BoolFalse, std::move(name), 0.f };
        }

        static AnimCondition OnTrigger(std::string name)
        {
            return { Kind::Trigger, std::move(name), 0.f };
        }
    };

    /**
     * @brief Simple gameplay animation state machine with crossfade.
     *
     * States reference AnimationClip pointers owned by the app/asset.
     * Evaluate(dt) advances time, resolves transitions, blends poses.
     */
    class AnimGraph
    {
      public:
        void SetFloat(std::string_view name, float value);
        void SetBool(std::string_view name, bool value);
        void SetTrigger(std::string_view name);

        [[nodiscard]] float GetFloat(std::string_view name,
                                     float            fallback = 0.f) const;
        [[nodiscard]] bool  GetBool(std::string_view name,
                                    bool             fallback = false) const;

        /**
         * @brief Tick graph and return blended local pose.
         */
        LocalPose Evaluate(float dt);

        [[nodiscard]] std::string_view CurrentStateName() const;
        [[nodiscard]] bool             IsBlending() const { return mBlending; }

        [[nodiscard]] const Skeleton* GetSkeleton() const { return mSkeleton; }

      private:
        friend class AnimGraphBuilder;

        struct State
        {
            std::string          name;
            const AnimationClip* clip          = nullptr;
            bool                 loop          = true;
            float                playbackSpeed = 1.f;
        };

        struct Transition
        {
            std::uint32_t from = 0;
            std::uint32_t to   = 0;
            AnimCondition condition;
            float         blendDuration = 0.2f;
        };

        struct FloatParam
        {
            float value = 0.f;
        };
        struct BoolParam
        {
            bool value = false;
        };
        struct TriggerParam
        {
            bool raised = false;
        };

        bool evaluateCondition(const AnimCondition& c) const;
        void clearTriggers();
        void tryStartTransition();

        const Skeleton*         mSkeleton = nullptr;
        std::vector<State>      mStates;
        std::vector<Transition> mTransitions;
        std::uint32_t           mEntryState = 0;

        std::unordered_map<std::string, FloatParam>   mFloats;
        std::unordered_map<std::string, BoolParam>    mBools;
        std::unordered_map<std::string, TriggerParam> mTriggers;

        std::uint32_t mCurrentState = 0;
        float         mCurrentTime  = 0.f;

        bool          mBlending      = false;
        std::uint32_t mNextState     = 0;
        float         mNextTime      = 0.f;
        float         mBlendDuration = 0.2f;
        float         mBlendElapsed  = 0.f;
    };

    class AnimGraphBuilder
    {
      public:
        AnimGraphBuilder& SetSkeleton(const Skeleton* skeleton);

        AnimGraphBuilder& ParamFloat(std::string name,
                                     float       defaultValue = 0.f);
        AnimGraphBuilder& ParamBool(std::string name,
                                    bool        defaultValue = false);
        AnimGraphBuilder& ParamTrigger(std::string name);

        AnimGraphBuilder& State(std::string name, const AnimationClip& clip,
                                bool loop = true, float playbackSpeed = 1.f);
        AnimGraphBuilder& Entry(std::string_view stateName);

        AnimGraphBuilder& Transition(std::string_view from,
                                     std::string_view to,
                                     AnimCondition    condition,
                                     float            blendDuration = 0.2f);

        AnimGraph Build();

      private:
        [[nodiscard]] std::int32_t findState(std::string_view name) const;

        const Skeleton* mSkeleton = nullptr;
        AnimGraph       mGraph;
        std::string     mEntryName;
    };

} // namespace FREYA_NAMESPACE
