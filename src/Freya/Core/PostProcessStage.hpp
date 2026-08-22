#pragma once

#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/PostProcess.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief IFrameStage adapter for a PostProcess.
     */
    class PostProcessStage : public IFrameStage
    {
      public:
        explicit PostProcessStage(skr::Arc<PostProcess> effect) :
            mEffect(std::move(effect))
        {
        }

        [[nodiscard]] const char* Name() const override
        {
            return mEffect ? mEffect->Name() : "PostProcess";
        }

        void Rebuild(StageContext& ctx, skr::ServiceProvider& sp) override
        {
            if (mEffect)
                mEffect->Rebuild(ctx, sp);
        }

        void Execute(StageContext& ctx) override
        {
            if (mEffect)
                mEffect->Execute(ctx);
        }

      private:
        skr::Arc<PostProcess> mEffect;
    };

} // namespace FREYA_NAMESPACE
