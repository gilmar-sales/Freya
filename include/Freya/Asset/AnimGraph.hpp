#pragma once

#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Skeleton.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FREYA_NAMESPACE
{
    struct AnimGraphDebugSnapshot;
    /**
     * @brief How an AnimGraph overlay layer is composited onto the base pose.
     */
    enum class AnimLayerMode : std::uint8_t
    {
        OverrideMasked = 0, ///< BlendMasked
        Additive       = 1, ///< BlendAdditive vs rest pose
    };

    /**
     * @brief GPU bridge for one graph overlay layer.
     */
    struct AnimLayerGpuSample
    {
        bool                 active = false;
        AnimLayerMode        mode   = AnimLayerMode::OverrideMasked;
        const AnimationClip* clip   = nullptr;
        float                time   = 0.f;
        float                weight = 0.f;
    };

    /**
     * @brief Fixed GPU overlay contract: at most one masked + one additive.
     *
     * Matches #GpuAnimInstance dual slots (crowd-safe). Extra graph layers of
     * the same mode are ignored on the GPU path (first wins).
     */
    struct AnimLayerGpuSlots
    {
        AnimLayerGpuSample masked;
        AnimLayerGpuSample additive;
    };

    /**
     * @brief GPU loco pack: up to three clips with barycentric weights.
     *
     * Blend1D sets `wC = 0`. Blend2D fills a triangle (`wA+wB+wC ≈ 1`).
     */
    struct AnimLocoGpuSample
    {
        const AnimationClip* clipA = nullptr;
        const AnimationClip* clipB = nullptr;
        const AnimationClip* clipC = nullptr;
        float                timeA = 0.f;
        float                timeB = 0.f;
        float                timeC = 0.f;
        float                wA    = 1.f;
        float                wB    = 0.f;
        float                wC    = 0.f;
    };

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
     * @brief Gameplay animation state machine with clip / Blend1D + crossfade
     *        and optional overlay layers (mask / additive) for GPU/CPU.
     *
     * States reference AnimationClip pointers owned by the app/asset.
     * Evaluate(dt) advances time, resolves transitions, blends poses, then
     * applies enabled layers in authoring order.
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

        void               SetLayerEnabled(std::string_view name, bool enabled);
        void               SetLayerWeight(std::string_view name, float weight);
        [[nodiscard]] bool IsLayerEnabled(std::string_view name) const;

        /**
         * @brief Tick graph (transitions + phase/time) without sampling poses.
         *
         * Advances base state and all layers. Use with GPU sample APIs, or
         * call SampleCurrent() for a CPU local pose afterward.
         *
         * With GPU skin (Crowd), still call Advance each frame so clip markers
         * (e.g. Footstep) fire from Blend1D/Blend2D phase — pose bake stays on
         * the GPU.
         */
        void Advance(float                             dt,
                     std::vector<FiredAnimationEvent>* outEvents = nullptr);

        /**
         * @brief Sample the current state + layers into a local pose.
         */
        [[nodiscard]] LocalPose SampleCurrent() const;

        /**
         * @brief Tick graph and return blended local pose.
         *
         * When `outEvents` is non-null, appends clip markers crossed this tick
         * (e.g. Footstep.L / Footstep.R).
         */
        LocalPose Evaluate(
            float dt, std::vector<FiredAnimationEvent>* outEvents = nullptr);

        [[nodiscard]] std::string_view CurrentStateName() const;
        [[nodiscard]] bool             IsBlending() const { return mBlending; }

        /**
         * @brief Pack current loco state for GPU (Blend1D or Blend2D).
         */
        bool TryGetLocoGpuSample(AnimLocoGpuSample& out) const;

        /**
         * @brief Blend1D-only helper (`blendT` toward clipB). Prefer
         * #TryGetLocoGpuSample for new code.
         */
        bool TryGetBlend1DGpuSample(const AnimationClip*& clipA, float& timeA,
                                    const AnimationClip*& clipB, float& timeB,
                                    float& blendT) const;

        /**
         * @brief Pack enabled layers into the fixed GPU dual-slot contract.
         *
         * First OverrideMasked and first Additive with weight &gt; 0 fill the
         * slots (CPU may still stack more layers in SampleCurrent).
         */
        bool TryGetLayerGpuSlots(AnimLayerGpuSlots& out) const;

        /**
         * @brief First enabled overlay (legacy helper; prefer
         * #TryGetLayerGpuSlots).
         */
        bool TryGetPrimaryLayerGpuSample(AnimLayerGpuSample& out) const;

        /**
         * @brief Fill a toolkit-agnostic snapshot for debug UI (ImGui, etc.).
         * @see AnimGraphDebugSnapshot
         */
        void CaptureDebugSnapshot(AnimGraphDebugSnapshot& out) const;

        /**
         * @brief Push editable snapshot fields back into the graph.
         *
         * Writes floats/bools/triggers/layer enable+weight according to the
         * snapshot's `apply*` flags. Does not mutate states or transitions.
         */
        void ApplyDebugSnapshot(const AnimGraphDebugSnapshot& in);

        [[nodiscard]] std::uint32_t LayerCount() const
        {
            return static_cast<std::uint32_t>(mLayers.size());
        }

        [[nodiscard]] const Skeleton* GetSkeleton() const { return mSkeleton; }

      private:
        friend class AnimGraphBuilder;

        enum class StateKind : std::uint8_t
        {
            Clip,
            Blend1D,
            Blend2D,
        };

        struct State
        {
            std::string          name;
            StateKind            kind          = StateKind::Clip;
            const AnimationClip* clip          = nullptr;
            bool                 loop          = true;
            float                playbackSpeed = 1.f;

            std::string                blendParam;
            std::string                blendParamY;
            std::vector<Blend1DSample> blendSamples;
            std::vector<Blend2DSample> blend2DSamples;
            std::vector<float>         blendTimes;
            bool                       syncPhase  = true;
            float                      blendPhase = 0.f;

            const BakedClip* baked = nullptr; ///< optional clip-state bake
        };

        struct Layer
        {
            std::string          name;
            const AnimationClip* clip          = nullptr;
            const BakedClip*     baked         = nullptr;
            bool                 loop          = true;
            float                playbackSpeed = 1.f;
            float                weight        = 1.f;
            std::string          weightParam;
            AnimLayerMode        mode    = AnimLayerMode::OverrideMasked;
            const BoneMask*      mask    = nullptr;
            bool                 enabled = true;
            float                time    = 0.f;
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
            float value        = 0.f;
            float defaultValue = 0.f;
            float minValue     = 0.f;
            float maxValue     = 1.f;
            bool  hasRange     = false;
        };
        struct BoolParam
        {
            bool value = false;
        };
        struct TriggerParam
        {
            bool raised = false;
        };

        bool      evaluateCondition(const AnimCondition& c) const;
        void      clearTriggers();
        void      tryStartTransition();
        void      advanceState(State& state, float& clipTime, float dt,
                               std::vector<FiredAnimationEvent>* outEvents);
        void      advanceLayers(float dt);
        LocalPose sampleState(const State& state, float clipTime) const;
        LocalPose sampleLayer(const Layer& layer) const;
        LocalPose applyLayers(LocalPose base) const;
        [[nodiscard]] float effectiveLayerWeight(const Layer& layer) const;
        [[nodiscard]] std::int32_t findLayer(std::string_view name) const;
        LocalPose evaluateState(State& state, float& clipTime, float dt,
                                std::vector<FiredAnimationEvent>* outEvents);

        const Skeleton*         mSkeleton = nullptr;
        std::vector<State>      mStates;
        std::vector<Layer>      mLayers;
        std::vector<Transition> mTransitions;
        std::uint32_t           mEntryState = 0;
        /// Owned bakes; Blend1DSample::baked / State::baked / Layer::baked.
        std::vector<BakedClip> mBakedClips;

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
        /**
         * @brief Float param with UI range hint (also clamps on Apply snapshot).
         */
        AnimGraphBuilder& ParamFloat(std::string name, float defaultValue,
                                     float minValue, float maxValue);
        AnimGraphBuilder& ParamBool(std::string name,
                                    bool        defaultValue = false);
        AnimGraphBuilder& ParamTrigger(std::string name);

        AnimGraphBuilder& State(std::string name, const AnimationClip& clip,
                                bool loop = true, float playbackSpeed = 1.f);

        /**
         * @brief Begin a Blend1D state driven by float param (e.g. "Speed").
         *
         * Call AddBlendSample afterwards (ascending `value` order).
         * When `syncPhase` is true (default), Walk/Run share a normalized
         * cycle so feet stay aligned while blending.
         */
        AnimGraphBuilder& Blend1DState(std::string name, std::string param,
                                       bool syncPhase = true);

        AnimGraphBuilder& AddBlendSample(
            float value, const AnimationClip& clip, bool loop = true,
            float playbackSpeed = 1.f, const BakedClip* baked = nullptr);

        /**
         * @brief Begin a Blend2D state (e.g. Strafe × Speed).
         *
         * Call AddBlend2DSample for each point in the blend space.
         */
        AnimGraphBuilder& Blend2DState(std::string name, std::string paramX,
                                       std::string paramY,
                                       bool        syncPhase = true);

        AnimGraphBuilder& AddBlend2DSample(
            float x, float y, const AnimationClip& clip, bool loop = true,
            float playbackSpeed = 1.f, const BakedClip* baked = nullptr);

        /**
         * @brief Add an overlay layer (clip) applied after the base state.
         *
         * Follow with LayerMasked / LayerAdditive. Layers advance in Advance
         * and composite in SampleCurrent. GPU packing uses at most one masked
         * and one additive slot (#TryGetLayerGpuSlots).
         */
        AnimGraphBuilder& Layer(std::string name, const AnimationClip& clip,
                                bool loop = true, float playbackSpeed = 1.f);

        AnimGraphBuilder& LayerMasked(const BoneMask* mask, float weight = 1.f);
        AnimGraphBuilder& LayerAdditive(float           weight = 1.f,
                                        const BoneMask* mask   = nullptr);
        AnimGraphBuilder& LayerWeightParam(std::string floatParam);
        AnimGraphBuilder& LayerBake(const BakedClip* baked);

        /**
         * @brief Bake every clip referenced by states/layers at `bakeHz`.
         *
         * Pass bakeHz &lt;= 0 to disable (keyframe SampleClip path).
         */
        AnimGraphBuilder& EnableBaking(float bakeHz = 30.f);

        AnimGraphBuilder& Entry(std::string_view stateName);

        AnimGraphBuilder& Transition(std::string_view from,
                                     std::string_view to,
                                     AnimCondition    condition,
                                     float            blendDuration = 0.2f);

        AnimGraph Build();

      private:
        [[nodiscard]] std::int32_t      findState(std::string_view name) const;
        [[nodiscard]] AnimGraph::Layer& lastLayer();

        const Skeleton* mSkeleton = nullptr;
        AnimGraph       mGraph;
        std::string     mEntryName;
        std::int32_t    mLastBlendState = -1;
        std::int32_t    mLastLayer      = -1;
        float           mBakeHz         = 0.f;
    };

} // namespace FREYA_NAMESPACE
