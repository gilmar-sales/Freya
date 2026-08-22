#pragma once

#include "Freya/Core/IFrameStage.hpp"

namespace FREYA_NAMESPACE
{
    class PickFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Pick"; }
        void                      Execute(StageContext& ctx) override;
    };

    class ShadowFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Shadow"; }
        void                      Execute(StageContext& ctx) override;
    };

    class DeferredGeometryFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "DeferredGeometry";
        }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class SsaoLightingFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "SsaoLighting";
        }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class TaaFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Taa"; }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class TranslucentFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "Translucent";
        }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class BillboardVfxFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "BillboardVfx";
        }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class BillboardUiFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "BillboardUi";
        }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class BloomFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Bloom"; }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class CompositeFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Composite"; }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    class DebugDrawFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "DebugDraw"; }
        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override;
        void Execute(StageContext& ctx) override;
    };

    /**
     * @brief GPU animation compute (fills bone SSBO). Runs before Pick.
     *
     * No-op while GpuAnimPass is disabled or instanceCount == 0 (CPU upload
     * path unchanged).
     */
    class GpuAnimFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "GpuAnim"; }
        void                      Execute(StageContext& ctx) override;
    };

} // namespace FREYA_NAMESPACE
