#pragma once

#include "Freya/Config.hpp"

#include <memory>

#include <Skirnir/Skirnir.hpp>

namespace FREYA_NAMESPACE
{
    struct RenderFrameContext;

    /**
     * @brief One ordered step in the Renderer frame graph.
     *
     * Default stages wrap existing passes (Pick, Shadow, Deferred, …).
     * Apps insert stages created by Freya factories (e.g.
     * PostProcess::MakeStage).
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
