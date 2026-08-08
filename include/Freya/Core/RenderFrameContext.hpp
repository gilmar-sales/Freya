#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/SsaoPass.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/TaaPass.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <functional>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Image;
    class RenderTarget;

    /**
     * @brief Shared state and callbacks passed to IFrameStage::Execute.
     *
     * Pointers alias Renderer-owned Arcs; stages must not outlive the
     * Renderer::EndScene / Rebuild call that created the context.
     */
    struct RenderFrameContext
    {
        skr::Arc<CommandPool>  commandPool;
        skr::Arc<SwapChain>    swapChain;
        skr::Arc<FreyaOptions> options;
        vk::Extent2D           renderExtent {};
        std::uint32_t          frameIndex = 0;
        float                  cameraNear = 1.0f;

        ProjectionUniformBuffer* projection = nullptr;

        skr::Arc<DeferredCompressedPass>* deferred           = nullptr;
        skr::Arc<SsaoPass>*               ssao               = nullptr;
        skr::Arc<TaaPass>*                taa                = nullptr;
        skr::Arc<BloomPass>*              bloom              = nullptr;
        skr::Arc<CompositePass>*          composite          = nullptr;
        skr::Arc<ShadowPass>*             shadow             = nullptr;
        skr::Arc<PickPass>*               pick               = nullptr;
        skr::Arc<LightService>*           lights             = nullptr;
        skr::Arc<Image>*                  bloomResultImage   = nullptr;
        skr::Arc<Image>*                  ssaoFallbackImage  = nullptr;
        vk::Sampler*                      bloomResultSampler = nullptr;
        skr::Arc<RenderTarget>*           outputTarget       = nullptr;

        bool*          pickRequested        = nullptr;
        std::uint32_t* pickX                = nullptr;
        std::uint32_t* pickY                = nullptr;
        bool*          pickAwaitingReadback = nullptr;

        std::function<void(const glm::mat4&, CullMode)> dispatchCull;
        std::function<void(bool bindMaterials)>         executeDraws;
        std::function<void()>                           executePickDraws;
        std::function<void()>                           buildHiZ;
        std::function<void()>                           blitBloomToFullRes;
        std::function<void(std::uint32_t, const skr::Arc<Image>&,
                           const skr::Arc<Image>&, bool)>
                                          beginComposite;
        std::function<void()>             commitTaaHistory;
        std::function<void(vk::Extent2D)> resizePickPass;
        std::function<skr::Arc<Image>()>  createSsaoFallback;
    };

} // namespace FREYA_NAMESPACE
