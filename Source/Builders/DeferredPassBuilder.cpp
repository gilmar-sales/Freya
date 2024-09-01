#include "Builders/DeferredPassBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<DeferredPass> DeferredPassBuilder::Build() const
    {
        // const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = {
            // Back buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
            // Depth buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eD32Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eUndefined),
            // G buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR32G32B32A32Uint)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eUndefined),
            // Translucent buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eUndefined),
        };

        constexpr auto depthBufferReference =
            vk::AttachmentReference()
                .setAttachment(DepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto gBufferWriteReference =
            vk::AttachmentReference()
                .setAttachment(GBufferAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto gBufferReadReference = {
            vk::AttachmentReference()
                .setAttachment(GBufferAttachment)
                .setLayout(vk::ImageLayout::eReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
        };

        auto translucentBufferWriteReference = {
            vk::AttachmentReference()
                .setAttachment(TranslucentAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto opaqueBufferWriteReference = {
            vk::AttachmentReference()
                .setAttachment(OpaqueAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto compositeReadReference = {
            vk::AttachmentReference()
                .setAttachment(TranslucentAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(OpaqueAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Final pass-back buffer render reference
        auto backBufferRenderReference = {
            vk::AttachmentReference()
                .setAttachment(0)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto subpasses = {
            // Subpass 0 - depth prepass
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthBufferReference),
            // Subpass 1 - g-buffer generation
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(gBufferWriteReference)
                .setPDepthStencilAttachment(&depthBufferReference),
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
                .setInputAttachments(compositeReadReference)
                .setColorAttachments(backBufferRenderReference),
        };

        auto dependencies = {
            // G-buffer pass depends on depth prepass.
            vk::SubpassDependency()
                .setSrcSubpass(DepthPrePass)
                .setDstSubpass(GBufferPass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Lighting pass depends on g-buffer.
            vk::SubpassDependency()
                .setSrcSubpass(GBufferPass)
                .setDstSubpass(LightingPass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite pass depends on translucent pass
            vk::SubpassDependency()
                .setSrcSubpass(TranslucentPass)
                .setDstSubpass(CompositePass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite pass also depends on lightning
            vk::SubpassDependency()
                .setSrcSubpass(LightingPass)
                .setDstSubpass(CompositePass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        };

        const auto renderPassCreateInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        auto renderPass = mDevice->Get().createRenderPass(renderPassCreateInfo);

        return MakeRef<DeferredPass>(mDevice, mSurface, renderPass);
    }
} // namespace FREYA_NAMESPACE
