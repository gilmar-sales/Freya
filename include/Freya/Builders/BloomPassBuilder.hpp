#pragma once

#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    class ShaderModuleBuilder;

    class BloomPassBuilder
    {
      public:
        BloomPassBuilder(const skr::Arc<Device>&               device,
                         const skr::Arc<PhysicalDevice>&       physicalDevice,
                         const skr::Arc<Surface>&              surface,
                         const skr::Arc<FreyaOptions>&         freyaOptions,
                         const skr::Arc<skr::ServiceProvider>& serviceProvider);

        skr::Arc<BloomPass> Build(const skr::Arc<SwapChain>& swapChain,
                                  const skr::Arc<Image>&     emissiveImage,
                                  vk::Extent2D               fullExtent = {});

      private:
        vk::RenderPass createRenderPass() const;

        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<Surface>              mSurface;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
