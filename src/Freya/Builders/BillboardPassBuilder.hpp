#pragma once

#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/BillboardPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class BillboardPassBuilder
    {
      public:
        BillboardPassBuilder(
            const skr::Arc<Device>&                      device,
            const skr::Arc<PhysicalDevice>&              physicalDevice,
            const skr::Arc<Surface>&                     surface,
            const skr::Arc<FreyaOptions>&                freyaOptions,
            const skr::Arc<MaterialDescriptorResources>& materials,
            const skr::Arc<skr::ServiceProvider>&        serviceProvider);

        skr::Arc<BillboardPass> Build(const skr::Arc<SwapChain>& swapChain,
                                      const skr::Arc<Image>&     depthImage);

      private:
        [[nodiscard]] vk::RenderPass createHdrRenderPass(
            vk::Format depthFormat) const;
        [[nodiscard]] vk::RenderPass createLdrRenderPass(
            vk::Format depthFormat) const;
        [[nodiscard]] vk::Pipeline createPipeline(
            vk::ShaderModule vert, vk::ShaderModule frag,
            vk::PipelineLayout layout, vk::RenderPass renderPass, bool additive,
            bool depthTest) const;

        skr::Arc<Device>                      mDevice;
        skr::Arc<PhysicalDevice>              mPhysicalDevice;
        skr::Arc<Surface>                     mSurface;
        skr::Arc<FreyaOptions>                mFreyaOptions;
        skr::Arc<MaterialDescriptorResources> mMaterials;
        skr::Arc<skr::ServiceProvider>        mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
