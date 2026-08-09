#pragma once

#include "Freya/Asset/AnimGraph.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Flat, GUI-toolkit-agnostic view of an #AnimGraph.
     *
     * Capture → bind widgets in ImGui/Qt/etc. → Apply to push edits back.
     * Structure fields (states / transitions / clip names) are informational;
     * #AnimGraph::ApplyDebugSnapshot only writes params, layer toggles/weights,
     * and triggers.
     *
     * Example (ImGui, outside Freya):
     * @code
     * fra::AnimGraphDebugSnapshot s;
     * graph.CaptureDebugSnapshot(s);
     * for (auto& p : s.floats)
     *     ImGui::SliderFloat(p.name.c_str(), &p.value, 0.f, 2.f);
     * for (auto& b : s.bools)
     *     ImGui::Checkbox(b.name.c_str(), &b.value);
     * for (auto& t : s.triggers)
     *     if (ImGui::Button(t.name.c_str())) t.pulse = true;
     * for (auto& L : s.layers) {
     *     ImGui::Checkbox(L.name.c_str(), &L.enabled);
     *     ImGui::SliderFloat((L.name + ".w").c_str(), &L.weight, 0.f, 1.f);
     * }
     * graph.ApplyDebugSnapshot(s);
     * @endcode
     */
    struct AnimGraphDebugSnapshot
    {
        struct FloatParam
        {
            std::string name;
            float       value        = 0.f;
            float       defaultValue = 0.f;
            float       minValue     = 0.f;
            float       maxValue     = 1.f;
            bool        hasRange     = false; ///< if true, clamp / use [min,max]
        };
        struct BoolParam
        {
            std::string name;
            bool        value = false;
        };
        struct TriggerParam
        {
            std::string name;
            bool raised = false; ///< current pending latch (read)
            bool pulse  = false; ///< set true in UI to fire on Apply
        };
        struct Layer
        {
            std::string   name;
            std::string   clipName;
            AnimLayerMode mode           = AnimLayerMode::OverrideMasked;
            bool          enabled        = true;
            float         weight         = 1.f;
            float         effectiveWeight = 0.f;
            float         time           = 0.f;
            std::string   weightParam;
        };
        struct State
        {
            std::string name;
            std::string kind; ///< "Clip" | "Blend1D" | "Blend2D"
            std::string blendParam;
            std::string blendParamY;
            std::uint32_t sampleCount = 0;
        };
        struct Transition
        {
            std::string from;
            std::string to;
            std::string condition; ///< "FloatGreater" | ... | "Trigger"
            std::string param;
            float       threshold     = 0.f;
            float       blendDuration = 0.2f;
        };
        struct Loco
        {
            bool        valid = false;
            std::string clipA;
            std::string clipB;
            std::string clipC;
            float       timeA = 0.f;
            float       timeB = 0.f;
            float       timeC = 0.f;
            float       wA    = 1.f;
            float       wB    = 0.f;
            float       wC    = 0.f;
        };

        std::string currentState;
        std::string nextState;
        bool        blending      = false;
        float       currentTime   = 0.f;
        float       nextTime      = 0.f;
        float       blendElapsed  = 0.f;
        float       blendDuration = 0.f;
        float       blendAlpha    = 0.f; ///< 0..1 while blending

        std::vector<FloatParam>   floats;
        std::vector<BoolParam>    bools;
        std::vector<TriggerParam> triggers;
        std::vector<Layer>        layers;
        std::vector<State>        states;
        std::vector<Transition>   transitions;
        Loco                      loco;

        /// Apply() writes these categories when true.
        bool applyFloats   = true;
        bool applyBools    = true;
        bool applyTriggers = true;
        bool applyLayers   = true;
    };

} // namespace FREYA_NAMESPACE
