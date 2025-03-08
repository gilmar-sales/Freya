#include "Freya/Builders/DeferredPassBuilder.hpp"

#include "Freya/Builders/ShaderModuleBuilder.hpp"

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

        auto shaderStages = { gBufferVertShaderStageInfo,
                              gBufferFragShaderStageInfo };

        return MakeRef<DeferredPass>(mDevice, mSurface, renderPass);
    }

    vk::RenderPass DeferredPassBuilder::createRenderPass() const
    {
        const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = {
            // Positions
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Normals
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Albedo
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Depth buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eD32Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(
                    vk::ImageLayout::eDepthStencilAttachmentOptimal),
        };

        auto gBufferReferences = std::vector {
            vk::AttachmentReference()
                .setAttachment(DeferredPositionsAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredNormalsAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredAlbedoAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal)
        };

        const auto depthBufferReference =
            vk::AttachmentReference()
                .setAttachment(DeferredDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto subpasses = {
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(gBufferReferences)
                .setPDepthStencilAttachment(&depthBufferReference),
        };

        auto dependencies = {
            // Starting from G-buffer.
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead |
                                  vk::AccessFlagBits::eColorAttachmentWrite)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Lighting pass depends on g-buffer.
            vk::SubpassDependency()
                .setSrcSubpass(DeferredGBufferPass)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead |
                                  vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eMemoryRead)
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
