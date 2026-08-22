#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Core/BillboardPass.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DebugDrawPass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/GpuAnimPass.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/SsaoPass.hpp"
#include "Freya/Core/StageContext.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/TaaPass.hpp"
#include "Freya/Core/TranslucentPass.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"
#include "Freya/Internal/VulkanCompat.hpp"

#include <functional>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Image;
    class RenderTarget;

    /**
     * @brief Internal stage context: public StageContext plus pass pointers.
     *
     * Pointers alias Renderer-owned Arcs; stages must not outlive the
     * Renderer::EndScene / Rebuild call that created the context.
     */
    struct RenderFrameContext : StageContext
    {
        [[nodiscard]] vk::Extent2D VkExtent() const
        {
            return ToVkExtent(renderExtent);
        }

        skr::Arc<CommandPool> commandPool;
        skr::Arc<SwapChain>   swapChain;

        ProjectionUniformBuffer* projection = nullptr;

        skr::Arc<DeferredCompressedPass>* deferred           = nullptr;
        skr::Arc<SsaoPass>*               ssaoPass           = nullptr;
        skr::Arc<TaaPass>*                taa                = nullptr;
        skr::Arc<TranslucentPass>*        translucent        = nullptr;
        skr::Arc<BloomPass>*              bloom              = nullptr;
        skr::Arc<CompositePass>*          composite          = nullptr;
        skr::Arc<ShadowPass>*             shadow             = nullptr;
        skr::Arc<PickPass>*               pick               = nullptr;
        skr::Arc<LightService>*           lights             = nullptr;
        std::vector<skr::Arc<Image>>*     bloomResultImages  = nullptr;
        skr::Arc<Image>*                  ssaoFallbackImage  = nullptr;
        vk::Sampler*                      bloomResultSampler = nullptr;
        skr::Arc<RenderTarget>*           outputTarget       = nullptr;

        skr::Arc<DebugDrawPass>* debugDrawPass = nullptr;
        skr::Arc<BillboardPass>* billboardPass = nullptr;
        BillboardDraw*           billboardDraw = nullptr;
        skr::Arc<GpuAnimPass>*   gpuAnim       = nullptr;

        bool*          pickRequested        = nullptr;
        std::uint32_t* pickX                = nullptr;
        std::uint32_t* pickY                = nullptr;
        bool*          pickAwaitingReadback = nullptr;

        vk::PipelineLayout* drawPipelineLayoutOverride = nullptr;
        std::uint32_t*      usedTechniqueMask          = nullptr;

        // Internal-only callbacks (public DispatchCull / ExecuteDraws are on
        // StageContext and alias these during makeFrameContext).
        std::function<void()> executePickDraws;
        std::function<void()> buildHiZ;
        std::function<void()> blitBloomToFullRes;
        std::function<void(std::uint32_t, const skr::Arc<Image>&, bool)>
                                          beginComposite;
        std::function<void()>             commitTaaHistory;
        std::function<void(vk::Extent2D)> resizePickPass;
        std::function<skr::Arc<Image>()>  createSsaoFallback;
        std::function<void()>             drawDebugOverlay;
    };

    inline RenderFrameContext& AsRenderFrameContext(StageContext& ctx)
    {
        return static_cast<RenderFrameContext&>(ctx);
    }

    inline const RenderFrameContext& AsRenderFrameContext(
        const StageContext& ctx)
    {
        return static_cast<const RenderFrameContext&>(ctx);
    }

} // namespace FREYA_NAMESPACE
