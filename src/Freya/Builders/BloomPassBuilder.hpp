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
        BloomPassBuilder(const Ref<Device>&               device,
                         const Ref<PhysicalDevice>&       physicalDevice,
                         const Ref<Surface>&              surface,
                         const Ref<FreyaOptions>&         freyaOptions,
                         const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<BloomPass> Build(const Ref<SwapChain>& swapChain,
                             const Ref<Image>&     emissiveImage);

      private:
        vk::RenderPass createRenderPass() const;

        Ref<Device>               mDevice;
        Ref<PhysicalDevice>       mPhysicalDevice;
        Ref<Surface>              mSurface;
        Ref<FreyaOptions>         mFreyaOptions;
        Ref<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
