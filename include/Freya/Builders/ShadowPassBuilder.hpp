#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for ShadowPass objects.
     *
     * Creates the depth-only shadow render pass, depth pipeline (bone
     * SSBO set 0 + push-constant light VP), cascade/spot/point depth
     * image arrays with their per-layer/per-face views and framebuffers,
     * the ring-buffered host-visible ShadowUniformBuffer, and the
     * hardware comparison sampler used for shadow sampling.
     */
    class ShadowPassBuilder
    {
      public:
        ShadowPassBuilder(const skr::Arc<Device>&               device,
                          const skr::Arc<PhysicalDevice>&       physicalDevice,
                          const skr::Arc<FreyaOptions>&         freyaOptions,
                          const skr::Arc<skr::ServiceProvider>& serviceProvider,
                          const skr::Arc<BoneMatrixResources>&  boneResources) :
            mDevice(device), mPhysicalDevice(physicalDevice),
            mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
            mBoneResources(boneResources)
        {
        }

        /**
         * @brief Builds and returns the ShadowPass object.
         * @return Shared pointer to created ShadowPass
         */
        skr::Arc<ShadowPass> Build();

      private:
        /**
         * @brief Result of creating a depth image array (or cube array).
         */
        struct ArrayImage
        {
            vk::Image                  image;
            vk::DeviceMemory           memory;
            vk::ImageView              arrayView;
            std::vector<vk::ImageView> layerViews;
        };

        vk::RenderPass createRenderPass(vk::Format depthFormat) const;

        ArrayImage createArrayImage(vk::Format        format,
                                    std::uint32_t     resolution,
                                    std::uint32_t     layerCount,
                                    bool              cubeCompatible,
                                    vk::ImageViewType arrayViewType) const;

        std::vector<vk::Framebuffer> createFramebuffers(
            vk::RenderPass                    renderPass,
            const std::vector<vk::ImageView>& layerViews,
            std::uint32_t                     resolution) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<PhysicalDevice>       mPhysicalDevice;
        skr::Arc<FreyaOptions>         mFreyaOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        skr::Arc<BoneMatrixResources>  mBoneResources;
    };

} // namespace FREYA_NAMESPACE
