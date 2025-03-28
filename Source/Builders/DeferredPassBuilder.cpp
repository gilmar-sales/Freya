#include "Freya/Builders/DeferredPassBuilder.hpp"

#include "Freya/Builders/ShaderModuleBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<DeferredPass> DeferredPassBuilder::Build() const
    {
        auto renderPass = createRenderPass();

        auto attachments = DeferredPassAttachments {
            .Position = ImageBuilder(MakeRef<skr::Logger<ImageBuilder>>(
                                         MakeRef<skr::LoggerOptions>()))
                            .SetUsage(ImageUsage::GBufferPosition)
                            .SetDevice(mDevice)
                            .SetHeight(mSurface->QueryExtent().height)
                            .SetWidth(mSurface->QueryExtent().width)
                            .Build(),
            .Normal = ImageBuilder(MakeRef<skr::Logger<ImageBuilder>>(
                                       MakeRef<skr::LoggerOptions>()))
                          .SetUsage(ImageUsage::GBufferNormal)
                          .SetDevice(mDevice)
                          .SetHeight(mSurface->QueryExtent().height)
                          .SetWidth(mSurface->QueryExtent().width)
                          .Build(),
            .Albedo = ImageBuilder(MakeRef<skr::Logger<ImageBuilder>>(
                                       MakeRef<skr::LoggerOptions>()))
                          .SetUsage(ImageUsage::GBufferAlbedo)
                          .SetDevice(mDevice)
                          .SetHeight(mSurface->QueryExtent().height)
                          .SetWidth(mSurface->QueryExtent().width)
                          .Build(),
        };

        auto descriptors = createDescriptors(attachments);

        auto pipeline = createPipeline(renderPass, descriptors);

        const auto gBufferVertShaderModule =
            ShaderModuleBuilder()
                .SetDevice(mDevice)
                .SetFilePath("./Resources/Shaders/Deferred/Vert.spv")
                .Build();

        const auto gBufferFragShaderModule =
            ShaderModuleBuilder()
                .SetDevice(mDevice)
                .SetFilePath("./Resources/Shaders/Deferred/Frag.spv")
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

        return MakeRef<DeferredPass>(
            mDevice,
            mSurface,
            renderPass,
            vk::PipelineLayout(),
            vk::Pipeline());
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

    DeferredPassPipeline DeferredPassBuilder::createPipeline(
        vk::RenderPass                 renderPass,
        const DeferredPassDescriptors& descriptors) const
    {
        auto pipelineLayoutCreateInfo =
            vk::PipelineLayoutCreateInfo().setSetLayoutCount(1).setPSetLayouts(
                &descriptors.layout);

        auto pipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutCreateInfo);

        auto vertexBindingDescription    = Vertex::GetBindingDescription();
        auto vertexAttributesDescription = Vertex::GetAttributesDescription();

        auto vertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(vertexBindingDescription)
                .setVertexAttributeDescriptions(vertexAttributesDescription);

        auto inputAssembly =
            vk::PipelineInputAssemblyStateCreateInfo()
                .setTopology(vk::PrimitiveTopology::eTriangleList)
                .setPrimitiveRestartEnable(false);

        auto viewportState = vk::PipelineViewportStateCreateInfo()
                                 .setViewportCount(1)
                                 .setScissorCount(1);

        auto rasterizer =
            vk::PipelineRasterizationStateCreateInfo()
                .setDepthClampEnable(true)
                .setRasterizerDiscardEnable(false)
                .setPolygonMode(vk::PolygonMode::eFill)
                .setCullMode(vk::CullModeFlagBits::eBack)
                .setFrontFace(vk::FrontFace::eClockwise)
                .setLineWidth(1.0f)
                .setDepthBiasEnable(false);

        auto pipelineCreateInfo =
            vk::GraphicsPipelineCreateInfo()
                .setLayout(pipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(0)
                .setBasePipelineHandle(nullptr)
                .setBasePipelineIndex(-1);

        return DeferredPassPipeline { .pipelineLayout = pipelineLayout,
                                      .pipeline       = vk::Pipeline() };
    }

    DeferredPassDescriptors DeferredPassBuilder::createDescriptors(
        const DeferredPassAttachments& attachments) const
    {
        auto descriptorPoolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(8),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(9)
        };

        auto descriptorPoolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(descriptorPoolSizes)
                .setMaxSets(mFrameCount);

        auto descriptorPool =
            mDevice->Get().createDescriptorPool(descriptorPoolInfo);

        assert(descriptorPool && "Failed to create descriptor pool");

        auto setLayoutBindings = {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
        };

        auto setLayoutCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(setLayoutBindings);

        auto descriptorSetLayout =
            mDevice->Get().createDescriptorSetLayout(setLayoutCreateInfo);

        assert(descriptorSetLayout && "Failed to create descriptor set layout");

        auto allocInfo = vk::DescriptorSetAllocateInfo()
                             .setDescriptorPool(descriptorPool)
                             .setSetLayouts({ descriptorSetLayout })
                             .setDescriptorSetCount(1);

        constexpr auto samplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setBorderColor(vk::BorderColor::eIntOpaqueWhite)
                .setUnnormalizedCoordinates(false)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMipLodBias(0.0f)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(1.0f);

        const auto sampler = mDevice->Get().createSampler(samplerCreateInfo);

        auto texDescriptorPosition =
            vk::DescriptorImageInfo()
                .setSampler(sampler)
                .setImageView(attachments.Position->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto texDescriptorNormal =
            vk::DescriptorImageInfo()
                .setSampler(sampler)
                .setImageView(attachments.Normal->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto texDescriptorAlbedo =
            vk::DescriptorImageInfo()
                .setSampler(sampler)
                .setImageView(attachments.Albedo->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto compositionDescriptor =
            mDevice->Get().allocateDescriptorSets(allocInfo).at(0);

        auto writeDescriptorSets = std::vector<vk::WriteDescriptorSet> {
            vk::WriteDescriptorSet()
                .setDstSet(compositionDescriptor)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setPImageInfo(&texDescriptorPosition),
            vk::WriteDescriptorSet()
                .setDstSet(compositionDescriptor)
                .setDstBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setPImageInfo(&texDescriptorNormal),
            vk::WriteDescriptorSet()
                .setDstSet(compositionDescriptor)
                .setDstBinding(3)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setPImageInfo(&texDescriptorAlbedo),
            vk::WriteDescriptorSet()
                .setDstSet(compositionDescriptor)
                .setDstBinding(4)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setPImageInfo(&texDescriptorAlbedo)

        };

        mDevice->Get().updateDescriptorSets(writeDescriptorSets, nullptr);

        return DeferredPassDescriptors { .pool        = descriptorPool,
                                         .layout      = descriptorSetLayout,
                                         .composition = compositionDescriptor };
    }
} // namespace FREYA_NAMESPACE
