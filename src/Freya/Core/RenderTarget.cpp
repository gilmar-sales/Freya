#include "Freya/Core/RenderTarget.hpp"

namespace FREYA_NAMESPACE
{
    RenderTarget::RenderTarget(const skr::Arc<Device>& device,
                               const skr::Arc<Image>&  colorImage,
                               const vk::Extent2D      extent,
                               const vk::Format        format,
                               const vk::RenderPass    renderPass,
                               const vk::Framebuffer   framebuffer,
                               const vk::Sampler       sampler) :
        mDevice(device), mColorImage(colorImage), mExtent(extent),
        mFormat(format), mRenderPass(renderPass), mFramebuffer(framebuffer),
        mSampler(sampler)
    {
    }

    RenderTarget::~RenderTarget()
    {
        mDevice->Get().waitIdle();
        auto& vkDevice = mDevice->Get();

        if (mFramebuffer)
            vkDevice.destroyFramebuffer(mFramebuffer);

        if (mRenderPass)
            vkDevice.destroyRenderPass(mRenderPass);

        if (mSampler)
            vkDevice.destroySampler(mSampler);

        mColorImage.reset();
    }

} // namespace FREYA_NAMESPACE
