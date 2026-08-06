#include "ShadowDenoisePassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/UniformBuffer.hpp"

namespace FREYA_NAMESPACE
{
    ShadowDenoisePassBuilder::ShadowDenoisePassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider,
        const skr::Arc<ShadowPass>&           shadowPass) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
        mShadowPass(shadowPass)
    {
    }

    vk::RenderPass ShadowDenoisePassBuilder::createColorPass(
        const vk::Format format) const
    {
        auto attachment =
            vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto colorRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef);

        auto dependencies = std::vector {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
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

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachment)
                .setSubpasses(subpass)
                .setDependencies(dependencies));
    }

    skr::Arc<ShadowDenoisePass> ShadowDenoisePassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain,
        const skr::Arc<Image>&     depthImage,
        const skr::Arc<Image>&     normalImage,
        vk::Extent2D               fullExtent)
    {
        if (fullExtent.width == 0 || fullExtent.height == 0)
            fullExtent = mSurface->QueryExtent();

        const auto halfExtent =
            vk::Extent2D { std::max(1u, fullExtent.width / 2),
                           std::max(1u, fullExtent.height / 2) };

        auto loadShader = [&](const std::string& path) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(path)
                .Build();
        };

        auto vertShader = loadShader(
            "./Resources/Shaders/DeferredCompressed/composing.vert.spv");
        auto maskFrag =
            loadShader("./Resources/Shaders/ShadowDenoise/mask.frag.spv");
        auto blurFrag =
            loadShader("./Resources/Shaders/ShadowDenoise/bilateral.frag.spv");
        auto upFrag =
            loadShader("./Resources/Shaders/ShadowDenoise/upsample.frag.spv");

        auto makeStage = [](vk::ShaderModule        module,
                            vk::ShaderStageFlagBits stage) {
            return vk::PipelineShaderStageCreateInfo()
                .setStage(stage)
                .setModule(module)
                .setPName("main");
        };

        auto vertStage =
            makeStage(vertShader->Get(), vk::ShaderStageFlagBits::eVertex);
        auto maskStages = {
            vertStage,
            makeStage(maskFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto blurStages = {
            vertStage,
            makeStage(blurFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto upStages = { vertStage,
                          makeStage(upFrag->Get(),
                                    vk::ShaderStageFlagBits::eFragment) };

        auto createMaskImage = [&](vk::Extent2D extent) {
            return mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16Sfloat)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();
        };

        auto halfMaskImage = createMaskImage(halfExtent);
        auto blurTempImage = createMaskImage(halfExtent);
        auto resultImage   = createMaskImage(fullExtent);

        auto cameraBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(ShadowDenoiseCameraUBO))
                .Build();

        auto sampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // --- Descriptor layouts ---
        auto maskBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
        };

        auto blurBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
        };

        auto upsampleBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
        };

        auto maskSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(maskBindings));
        auto blurSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(blurBindings));
        auto upsampleSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(upsampleBindings));

        std::array poolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(32),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(8),
        };

        auto descriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(static_cast<std::uint32_t>(poolSizes.size()))
                .setPPoolSizes(poolSizes.data())
                .setMaxSets(4));

        auto layouts = std::array { maskSetLayout, blurSetLayout, blurSetLayout,
                                    upsampleSetLayout };
        auto sets    = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(descriptorPool)
                .setSetLayouts(layouts));

        std::array<vk::DescriptorSet, 4> descriptorSets = { sets[0], sets[1],
                                                            sets[2], sets[3] };

        auto pushRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                             .setOffset(0)
                             .setSize(sizeof(ShadowDenoisePushConstants));

        auto maskLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setSetLayouts(maskSetLayout));
        auto blurLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(blurSetLayout)
                .setPushConstantRanges(pushRange));
        auto upsampleLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(upsampleSetLayout)
                .setPushConstantRanges(pushRange));

        auto emptyVertexInput = vk::PipelineVertexInputStateCreateInfo()
                                    .setVertexBindingDescriptions({})
                                    .setVertexAttributeDescriptions({});

        auto inputAssembly =
            vk::PipelineInputAssemblyStateCreateInfo()
                .setTopology(vk::PrimitiveTopology::eTriangleList)
                .setPrimitiveRestartEnable(false);

        auto viewportState = vk::PipelineViewportStateCreateInfo()
                                 .setViewportCount(1)
                                 .setScissorCount(1);

        auto rasterizer =
            vk::PipelineRasterizationStateCreateInfo()
                .setDepthClampEnable(false)
                .setRasterizerDiscardEnable(false)
                .setPolygonMode(vk::PolygonMode::eFill)
                .setCullMode(vk::CullModeFlagBits::eNone)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f);

        auto dynamicStates = std::vector { vk::DynamicState::eViewport,
                                           vk::DynamicState::eScissor };
        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        auto multisampling =
            vk::PipelineMultisampleStateCreateInfo()
                .setSampleShadingEnable(false)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

        auto noDepth = vk::PipelineDepthStencilStateCreateInfo()
                           .setDepthTestEnable(false)
                           .setDepthWriteEnable(false);

        auto noBlend =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false);

        std::array<vk::RenderPass, 4> renderPasses = {
            createColorPass(vk::Format::eR16Sfloat),
            createColorPass(vk::Format::eR16Sfloat),
            createColorPass(vk::Format::eR16Sfloat),
            createColorPass(vk::Format::eR16Sfloat),
        };

        auto makePipeline = [&](const auto& stages, vk::PipelineLayout layout,
                                vk::RenderPass renderPass) {
            auto blendState =
                vk::PipelineColorBlendStateCreateInfo()
                    .setLogicOpEnable(false)
                    .setAttachmentCount(1)
                    .setPAttachments(&noBlend);
            return mDevice->Get()
                .createGraphicsPipeline(
                    nullptr,
                    vk::GraphicsPipelineCreateInfo()
                        .setStages(stages)
                        .setPVertexInputState(&emptyVertexInput)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPDepthStencilState(&noDepth)
                        .setPMultisampleState(&multisampling)
                        .setPColorBlendState(&blendState)
                        .setPDynamicState(&dynamicState)
                        .setLayout(layout)
                        .setRenderPass(renderPass)
                        .setSubpass(0))
                .value;
        };

        std::array<vk::Pipeline, 4> pipelines = {
            makePipeline(maskStages, maskLayout, renderPasses[0]),
            makePipeline(blurStages, blurLayout, renderPasses[1]),
            makePipeline(blurStages, blurLayout, renderPasses[2]),
            makePipeline(upStages, upsampleLayout, renderPasses[3]),
        };

        std::array<vk::Framebuffer, 4> framebuffers {};
        auto makeFb = [&](vk::RenderPass rp, const skr::Arc<Image>& image,
                          vk::Extent2D extent) {
            auto view = image->GetImageView();
            return mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(rp)
                    .setAttachmentCount(1)
                    .setPAttachments(&view)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        };

        framebuffers[0] = makeFb(renderPasses[0], halfMaskImage, halfExtent);
        framebuffers[1] = makeFb(renderPasses[1], blurTempImage, halfExtent);
        framebuffers[2] = makeFb(renderPasses[2], halfMaskImage, halfExtent);
        framebuffers[3] = makeFb(renderPasses[3], resultImage, fullExtent);

        mDevice->Get().destroyShaderModule(vertShader->Get());
        mDevice->Get().destroyShaderModule(maskFrag->Get());
        mDevice->Get().destroyShaderModule(blurFrag->Get());
        mDevice->Get().destroyShaderModule(upFrag->Get());

        auto pass = skr::MakeArc<ShadowDenoisePass>(
            mDevice, mFreyaOptions, mSurface, fullExtent, halfExtent,
            renderPasses, maskLayout, blurLayout, upsampleLayout, pipelines,
            halfMaskImage, blurTempImage, resultImage, framebuffers,
            descriptorPool,
            std::vector<vk::DescriptorSetLayout> {
                maskSetLayout, blurSetLayout, upsampleSetLayout },
            descriptorSets, cameraBuffer, sampler);

        pass->UpdateDescriptors(
            depthImage, normalImage, mShadowPass->GetCascadeView(),
            mShadowPass->GetCascadeView(), mShadowPass->GetCompareSampler(),
            mShadowPass->GetSampler(), mShadowPass->GetUniformBuffer());

        (void) swapChain;
        return pass;
    }
} // namespace FREYA_NAMESPACE
