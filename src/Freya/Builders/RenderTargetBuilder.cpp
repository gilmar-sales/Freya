#include "Freya/Builders/RenderTargetBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"

namespace FREYA_NAMESPACE
{
    RenderTargetBuilder::RenderTargetBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mSurface(surface), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<RenderTarget> RenderTargetBuilder::Build()
    {
        const auto format =
            mFormat == vk::Format::eUndefined
                ? mSurface->QuerySurfaceFormat().format
                : mFormat;

        const auto extent = vk::Extent2D { std::max(1u, mWidth),
                                           std::max(1u, mHeight) };

        auto colorImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(format)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        auto renderPass = createRenderPass(format);

        auto fbInfo =
            vk::FramebufferCreateInfo()
                .setRenderPass(renderPass)
                .setAttachments(colorImage->GetImageView())
                .setWidth(extent.width)
                .setHeight(extent.height)
                .setLayers(1);

        auto framebuffer = mDevice->Get().createFramebuffer(fbInfo);

        auto sampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        return skr::MakeArc<RenderTarget>(mDevice,
                                          colorImage,
                                          extent,
                                          format,
                                          renderPass,
                                          framebuffer,
                                          sampler);
    }

    vk::RenderPass RenderTargetBuilder::createRenderPass(
        const vk::Format format) const
    {
        // Compatible with CompositePass (same format/samples); final layout is
        // ShaderReadOnlyOptimal so the color can be sampled (e.g. ImGui).
        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        auto colorRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto subpasses = std::vector<vk::SubpassDescription> {
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef),
        };

        auto dependencies = std::vector<vk::SubpassDependency> {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
        };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        return mDevice->Get().createRenderPass(renderPassInfo);
    }

} // namespace FREYA_NAMESPACE
