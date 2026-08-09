#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/TranslucentPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class TranslucentPassBuilder
    {
      public:
        TranslucentPassBuilder(
            const skr::Arc<Device>&                      device,
            const skr::Arc<PhysicalDevice>&              physicalDevice,
            const skr::Arc<Surface>&                     surface,
            const skr::Arc<FreyaOptions>&                freyaOptions,
            const skr::Arc<MaterialDescriptorResources>& materialResources,
            const skr::Arc<BoneMatrixResources>&         boneResources,
            const skr::Arc<LightService>&                lightService,
            const skr::Arc<skr::ServiceProvider>&        serviceProvider);

        /**
         * @brief Build OIT pass; depthImage must match deferred depth.
         */
        skr::Arc<TranslucentPass> Build(const skr::Arc<SwapChain>& swapChain,
                                        const skr::Arc<Image>&     depthImage,
                                        vk::Extent2D               extent = {});

      private:
        skr::Arc<Device>                      mDevice;
        skr::Arc<PhysicalDevice>              mPhysicalDevice;
        skr::Arc<Surface>                     mSurface;
        skr::Arc<FreyaOptions>                mFreyaOptions;
        skr::Arc<MaterialDescriptorResources> mMaterialResources;
        skr::Arc<BoneMatrixResources>         mBoneResources;
        skr::Arc<LightService>                mLightService;
        skr::Arc<skr::ServiceProvider>        mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
