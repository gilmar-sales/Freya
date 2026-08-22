#pragma once

#include "Freya/Config.hpp"
#include "Freya/Core/StageContext.hpp"

#include <memory>

#include <Skirnir/Skirnir.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief One ordered step in the Renderer frame graph.
     *
     * Default stages wrap existing passes (Pick, Shadow, Deferred, …).
     * Apps may implement custom stages and insert/replace them via
     * Renderer::InsertFrameStage / ReplaceFrameStage. Use StageContext
     * taps and Native* handles (cast to Vulkan in the app) for GPU work.
     * Freya factories such as PostProcess::MakeStage remain available.
     */
    class IFrameStage
    {
      public:
        virtual ~IFrameStage() = default;

        [[nodiscard]] virtual const char* Name() const = 0;

        /**
         * @brief Recreate extent-dependent GPU resources (optional).
         */
        virtual void Rebuild(StageContext& /*ctx*/,
                             skr::ServiceProvider& /*sp*/)
        {
        }

        /**
         * @brief Record GPU work for this frame.
         */
        virtual void Execute(StageContext& ctx) = 0;
    };

    using FrameStagePtr = std::shared_ptr<IFrameStage>;

} // namespace FREYA_NAMESPACE
