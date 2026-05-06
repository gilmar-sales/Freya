#pragma once

#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class CompositePassBuilder
    {
      public:
        CompositePassBuilder(const Ref<Device>&               device,
                             const Ref<PhysicalDevice>&       physicalDevice,
                             const Ref<Surface>&              surface,
                             const Ref<FreyaOptions>&         freyaOptions,
                             const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<CompositePass> Build(const Ref<SwapChain>& swapChain);

      private:
        vk::RenderPass createRenderPass() const;

        Ref<Device>               mDevice;
        Ref<PhysicalDevice>       mPhysicalDevice;
        Ref<Surface>              mSurface;
        Ref<FreyaOptions>         mFreyaOptions;
        Ref<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
