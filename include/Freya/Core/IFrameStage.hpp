#pragma once

#include <memory>

namespace FREYA_NAMESPACE
{
    struct RenderFrameContext;

    /**
     * @brief One ordered step in the Renderer frame graph.
     *
     * Default stages wrap existing passes (Pick, Shadow, Deferred, …).
     * Apps can InsertFrameStage / ReplaceFrameStage without forking Renderer.
     */
    class IFrameStage
    {
      public:
        virtual ~IFrameStage() = default;

        [[nodiscard]] virtual const char* Name() const = 0;

        /**
         * @brief Recreate extent-dependent GPU resources (optional).
         */
        virtual void Rebuild(RenderFrameContext& /*ctx*/,
                             skr::ServiceProvider& /*sp*/)
        {
        }

        /**
         * @brief Record GPU work for this frame.
         */
        virtual void Execute(RenderFrameContext& ctx) = 0;
    };

    using FrameStagePtr = std::shared_ptr<IFrameStage>;

} // namespace FREYA_NAMESPACE
