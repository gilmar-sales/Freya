#include "Builders/DeferredPassBuilder.hpp"

#include "Builders/ShaderModuleBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<DeferredPass> DeferredPassBuilder::Build() const
    {
        auto renderPass = createRenderPass();

        const auto gBufferVertShaderModule =
            ShaderModuleBuilder()
                .SetDevice(mDevice)
                .SetFilePath("./Shaders/Deferred/gbuffer.vert.spv")
                .Build();

        const auto gBufferFragShaderModule =
            ShaderModuleBuilder()
                .SetDevice(mDevice)
                .SetFilePath("./Shaders/Deferred/gbuffer.frag.spv")
                .Build();

        const auto gBufferVertShaderStageInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(gBufferVertShaderModule->Get())
                .setPName("main");

        const auto gBufferFragShaderStageInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(gBufferFragShaderModule->Get())
                .setPName("main");

        auto shaderStages = { gBufferVertShaderStageInfo, gBufferFragShaderStageInfo };

        return MakeRef<DeferredPass>(mDevice, mSurface, renderPass);
    }

    vk::RenderPass DeferredPassBuilder::createRenderPass() const
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
                .setAttachment(DeferredDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto gBufferWriteReference =
            vk::AttachmentReference()
                .setAttachment(DeferredGBufferAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto gBufferReadReference = {
            vk::AttachmentReference()
                .setAttachment(DeferredDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredGBufferAttachment)
                .setLayout(vk::ImageLayout::eReadOnlyOptimal),
        };

        auto translucentBufferWriteReference = {
            vk::AttachmentReference()
                .setAttachment(DeferredTranslucentAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto opaqueBufferWriteReference = {
            vk::AttachmentReference()
                .setAttachment(DeferredOpaqueAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto compositeReadReference = {
            vk::AttachmentReference()
                .setAttachment(DeferredTranslucentAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredOpaqueAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Final pass-back buffer render reference
        auto backBufferRenderReference = {
            vk::AttachmentReference()
                .setAttachment(DeferredBackAttachment)
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
                .setSrcSubpass(DeferredDepthPrePass)
                .setDstSubpass(DeferredGBufferPass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Lighting pass depends on g-buffer.
            vk::SubpassDependency()
                .setSrcSubpass(DeferredGBufferPass)
                .setDstSubpass(DeferredLightingPass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite pass depends on translucent pass
            vk::SubpassDependency()
                .setSrcSubpass(DeferredTranslucentPass)
                .setDstSubpass(DeferredCompositePass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite pass also depends on lightning
            vk::SubpassDependency()
                .setSrcSubpass(DeferredLightingPass)
                .setDstSubpass(DeferredCompositePass)
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

        return mDevice->Get().createRenderPass(renderPassCreateInfo);
    }
} // namespace FREYA_NAMESPACE
