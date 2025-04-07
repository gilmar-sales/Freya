#include "RendererBuilder.hpp"

#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"


namespace FREYA_NAMESPACE
{

    Ref<Renderer> RendererBuilder::Build()
    {
        mLogger->LogTrace("Creating renderer - Frame count: {} - Samples: {}",
                          mFreyaOptions->frameCount,
                          static_cast<int>(mFreyaOptions->sampleCount));

        return skr::MakeRef<Renderer>(
            mInstance,
            mSurface,
            mPhysicalDevice,
            mDevice,
            mSwapChain,
            mRenderPass,
            mCommandPool,
            mServiceProvider,
            mFreyaOptions,
            mEventManager);
    }

} // namespace FREYA_NAMESPACE
