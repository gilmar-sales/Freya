#include "Builders/DeferredPassBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<DeferredPass> DeferredPassBuilder::Build()
    {
        auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = {
            // Back buffer
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
            // Depth buffer
            vk::AttachmentDescription()
                .setFormat(mDevice->GetPhysicalDevice()->GetDepthFormat())
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
            // G buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR32G32B32A32Uint)
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eUndefined),
            // Translucency buffer
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eUndefined),
        };

        // Depth pre pass
        auto depthAttachmentReference =
            vk::AttachmentReference().setAttachment(1).setLayout(
                vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto gBufferReference = vk::AttachmentReference().setAttachment(2).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto gBufferReadReference = {
            vk::AttachmentReference().setAttachment(2).setLayout(
                vk::ImageLayout::eReadOnlyOptimal),
            vk::AttachmentReference().setAttachment(1).setLayout(
                vk::ImageLayout::eDepthStencilReadOnlyOptimal),
        };

        auto translucentBufferWriteReference = {
            vk::AttachmentReference().setAttachment(3).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto opaqueBufferWriteReference = {
            vk::AttachmentReference().setAttachment(4).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto compositeReference = {
            vk::AttachmentReference().setAttachment(3).setLayout(
                vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference().setAttachment(4).setLayout(
                vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Final pass-back buffer render reference
        auto backBufferRenderReference = {
            vk::AttachmentReference().setAttachment(0).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto subpasses = {
            // Subpass 0 - depth prepass
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthAttachmentReference),
            // Subpass 1 - g-buffer generation
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(gBufferReference)
                .setPDepthStencilAttachment(&depthAttachmentReference),
            // Subpass 2 - lighting
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setInputAttachments(gBufferReadReference)
                .setColorAttachments(opaqueBufferWriteReference),
            // Subpass 3 - translucents
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(translucentBufferWriteReference),
            // Subpass 4 - composite
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(backBufferRenderReference),
        };

        auto dependencies = {
            // G-buffer pass depends on depth prepass.
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(1)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Lighting pass depends on g-buffer.
            vk::SubpassDependency()
                .setSrcSubpass(1)
                .setDstSubpass(2)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite pass depends on translucent pass
            vk::SubpassDependency()
                .setSrcSubpass(4)
                .setDstSubpass(3)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite pass also depends on lightning
            vk::SubpassDependency()
                .setSrcSubpass(4)
                .setDstSubpass(2)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        };

        auto renderPassCreateInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        auto renderPass = mDevice->Get().createRenderPass(renderPassCreateInfo);

        return MakeRef<DeferredPass>();
    }
} // namespace FREYA_NAMESPACE
