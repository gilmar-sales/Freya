#pragma once

#include "Freya/Core/IFrameStage.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Device;
    class FullscreenEffectBuilder;

    enum class EffectInput
    {
        SceneColor,
        Depth,
        Albedo,
        Normal,
        Pbr,
        Velocity,
    };

    struct CellPushConstants
    {
        float     bands           = 4.0f;
        float     edgeDepthScale  = 80.0f;
        float     edgeNormalScale = 2.0f;
        float     strength        = 1.0f;
        glm::vec4 edgeColor { 0.02f, 0.02f, 0.04f, 1.0f };
        float     reverseZ   = 0.0f;
        float     shadowLift = 0.22f;
        float     edgeWidth  = 1.0f;
    };

    struct FullscreenMaterialMask
    {
        glm::uvec4    bits[2] {};
        std::uint32_t count = 0;
        std::uint32_t pad[3] {};
    };

    static_assert(sizeof(FullscreenMaterialMask) == 48,
                  "FullscreenMaterialMask must match GLSL std140");

    class FullscreenEffect : public skr::enable_arc_from_this<FullscreenEffect>
    {
      public:
        class Impl;

        explicit FullscreenEffect(std::unique_ptr<Impl> impl);
        ~FullscreenEffect();

        FullscreenEffect(const FullscreenEffect&)            = delete;
        FullscreenEffect& operator=(const FullscreenEffect&) = delete;

        [[nodiscard]] const char* Name() const;
        void                      SetEnabled(bool enabled);
        [[nodiscard]] bool        Enabled() const;

        void BindMaterial(std::uint32_t materialId);
        void UnbindMaterial(std::uint32_t materialId);
        void ClearMaterials();

        void SetPushConstants(const void* data, std::uint32_t size);

        template <typename T>
        void SetPushConstants(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>,
                          "Push constants must be trivially copyable");
            SetPushConstants(&value, static_cast<std::uint32_t>(sizeof(T)));
        }

        FrameStagePtr MakeStage();

        void Rebuild(RenderFrameContext& ctx, skr::ServiceProvider& sp);
        void Execute(RenderFrameContext& ctx);

      private:
        friend class FullscreenEffectBuilder;
        friend class FullscreenEffectStage;

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
