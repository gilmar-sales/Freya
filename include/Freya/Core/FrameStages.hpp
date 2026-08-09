#pragma once

#include "Freya/Core/IFrameStage.hpp"

namespace FREYA_NAMESPACE
{
    class PickFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Pick"; }
        void                      Execute(RenderFrameContext& ctx) override;
    };

    class ShadowFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Shadow"; }
        void                      Execute(RenderFrameContext& ctx) override;
    };

    class DeferredGeometryFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "DeferredGeometry";
        }
        void Rebuild(RenderFrameContext&   ctx,
                     skr::ServiceProvider& sp) override;
        void Execute(RenderFrameContext& ctx) override;
    };

    class SsaoLightingFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "SsaoLighting";
        }
        void Rebuild(RenderFrameContext&   ctx,
                     skr::ServiceProvider& sp) override;
        void Execute(RenderFrameContext& ctx) override;
    };

    class TaaFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Taa"; }
        void                      Rebuild(RenderFrameContext&   ctx,
                                          skr::ServiceProvider& sp) override;
        void                      Execute(RenderFrameContext& ctx) override;
    };

    class TranslucentFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override
        {
            return "Translucent";
        }
        void Rebuild(RenderFrameContext&   ctx,
                     skr::ServiceProvider& sp) override;
        void Execute(RenderFrameContext& ctx) override;
    };

    class BloomFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Bloom"; }
        void                      Rebuild(RenderFrameContext&   ctx,
                                          skr::ServiceProvider& sp) override;
        void                      Execute(RenderFrameContext& ctx) override;
    };

    class CompositeFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "Composite"; }
        void                      Rebuild(RenderFrameContext&   ctx,
                                          skr::ServiceProvider& sp) override;
        void                      Execute(RenderFrameContext& ctx) override;
    };

    class DebugDrawFrameStage : public IFrameStage
    {
      public:
        [[nodiscard]] const char* Name() const override { return "DebugDraw"; }
        void                      Rebuild(RenderFrameContext&   ctx,
                                          skr::ServiceProvider& sp) override;
        void                      Execute(RenderFrameContext& ctx) override;
    };

} // namespace FREYA_NAMESPACE
