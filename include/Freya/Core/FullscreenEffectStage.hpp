#pragma once

#include "Freya/Core/FullscreenEffect.hpp"
#include "Freya/Core/IFrameStage.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief IFrameStage adapter for a FullscreenEffect.
     */
    class FullscreenEffectStage : public IFrameStage
    {
      public:
        explicit FullscreenEffectStage(skr::Arc<FullscreenEffect> effect) :
            mEffect(std::move(effect))
        {
        }

        [[nodiscard]] const char* Name() const override
        {
            return mEffect ? mEffect->Name() : "FullscreenEffect";
        }

        void Rebuild(RenderFrameContext& ctx, skr::ServiceProvider& sp) override
        {
            if (mEffect)
                mEffect->Rebuild(ctx, sp);
        }

        void Execute(RenderFrameContext& ctx) override
        {
            if (mEffect)
                mEffect->Execute(ctx);
        }

      private:
        skr::Arc<FullscreenEffect> mEffect;
    };

} // namespace FREYA_NAMESPACE
