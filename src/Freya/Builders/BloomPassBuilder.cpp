#include "BloomPassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    BloomPassBuilder::BloomPassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<BloomPass> BloomPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain,
        const skr::Arc<Image>&     emissiveImage,
        vk::Extent2D               fullExtent)
    {
        auto renderPass = createRenderPass();

        // ------------------------------------------------------------------
        // Shaders
        // ------------------------------------------------------------------
        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto vertShader = loadShader("DeferredCompressed/composing.vert.spv");
        auto threshFrag = loadShader("DeferredCompressed/threshold.frag.spv");
        auto downFrag   = loadShader("DeferredCompressed/downsample.frag.spv");
        auto upFrag     = loadShader("DeferredCompressed/upsample.frag.spv");

        auto makeStage = [](vk::ShaderModule        module,
                            vk::ShaderStageFlagBits stage) {
            return vk::PipelineShaderStageCreateInfo()
                .setStage(stage)
                .setModule(module)
                .setPName("main");
        };

        auto vertStage =
            makeStage(vertShader->Get(), vk::ShaderStageFlagBits::eVertex);
        auto threshStages = {
            vertStage,
            makeStage(threshFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto downStages = {
            vertStage,
            makeStage(downFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto upStages = { vertStage,
                          makeStage(upFrag->Get(),
                                    vk::ShaderStageFlagBits::eFragment) };

        // ------------------------------------------------------------------
        // Half-resolution extent
        // ------------------------------------------------------------------
        if (fullExtent.width == 0 || fullExtent.height == 0)
            fullExtent = mSurface->QueryExtent();
        const auto halfExtent =
            ScaledExtent(fullExtent, mFreyaOptions->bloomResolutionDivisor);

        // ------------------------------------------------------------------
        // Bloom images at half resolution (one set per in-flight frame)
        // ------------------------------------------------------------------
        auto createBloomImage = [&]() {
            return mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(halfExtent.width)
                .SetHeight(halfExtent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();
        };

        const auto                   frameCount = mFreyaOptions->frameCount;
        std::vector<skr::Arc<Image>> bloomThresholdImages(frameCount);
        std::vector<skr::Arc<Image>> bloomDownImages(frameCount);
        std::vector<skr::Arc<Image>> bloomUpImages(frameCount);
        for (std::uint32_t i = 0; i < frameCount; ++i)
        {
            bloomThresholdImages[i] = createBloomImage();
            bloomDownImages[i]      = createBloomImage();
            bloomUpImages[i]        = createBloomImage();
        }

        // ------------------------------------------------------------------
        // Descriptor set layout:
        //   binding 0 = combined image sampler (all subpasses read via sampler)
        //   Input attachment refs in subpass descriptions handle layout
        //   transitions; the shader reads via texture() with offsets.
        // ------------------------------------------------------------------
        auto samplerBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto layoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(samplerBinding);
        auto descriptorSetLayout =
            mDevice->Get().createDescriptorSetLayout(layoutInfo);

        // ------------------------------------------------------------------
        // Descriptor pool: 3 subpasses × frameCount sets, 1 sampler each
        // ------------------------------------------------------------------
        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eCombinedImageSampler)
                            .setDescriptorCount(3 * mFreyaOptions->frameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(3 * mFreyaOptions->frameCount);

        auto descriptorPool = mDevice->Get().createDescriptorPool(poolInfo);

        // Allocate 3 descriptor sets per frame
        auto layouts = std::vector<vk::DescriptorSetLayout> {};
        for (auto i = 0; i < 3 * mFreyaOptions->frameCount; i++)
            layouts.push_back(descriptorSetLayout);

        auto allocInfo = vk::DescriptorSetAllocateInfo()
                             .setDescriptorPool(descriptorPool)
                             .setSetLayouts(layouts);

        auto allSets = mDevice->Get().allocateDescriptorSets(allocInfo);

        // Sampler for emissive read
        auto defaultSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // Write descriptor sets per frame
        for (std::uint32_t frame = 0; frame < frameCount; frame++)
        {
            auto baseIdx = frame * 3;

            // Threshold set: binding 0 = emissive / HDR scene sampler
            auto emissiveInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(emissiveImage->GetImageView())
                    .setSampler(defaultSampler);

            mDevice->Get().updateDescriptorSets(
                vk::WriteDescriptorSet()
                    .setDstSet(allSets[baseIdx + BloomThresholdSubpass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(emissiveInfo),
                nullptr);

            // Downsample set: binding 0 = this frame's threshold texture
            auto thresholdSamplerInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(bloomThresholdImages[frame]->GetImageView())
                    .setSampler(defaultSampler);

            mDevice->Get().updateDescriptorSets(
                vk::WriteDescriptorSet()
                    .setDstSet(allSets[baseIdx + BloomDownsampleSubpass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(thresholdSamplerInfo),
                nullptr);

            // Upsample set: binding 0 = this frame's down texture
            auto downSamplerInfo =
                vk::DescriptorImageInfo()
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setImageView(bloomDownImages[frame]->GetImageView())
                    .setSampler(defaultSampler);

            mDevice->Get().updateDescriptorSets(
                vk::WriteDescriptorSet()
                    .setDstSet(allSets[baseIdx + BloomUpsampleSubpass])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(downSamplerInfo),
                nullptr);
        }

        // ------------------------------------------------------------------
        // Pipeline layout (+ push constants for bloom threshold)
        // ------------------------------------------------------------------
        auto pushRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                             .setOffset(0)
                             .setSize(sizeof(float) * 2);

        auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(descriptorSetLayout)
                .setPushConstantRanges(pushRange);
        auto pipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        // ------------------------------------------------------------------
        // Common pipeline state
        // ------------------------------------------------------------------
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
                .setLineWidth(1.0f)
                .setDepthBiasEnable(false);

        auto dynamicStates = std::vector { vk::DynamicState::eViewport,
                                           vk::DynamicState::eScissor };
        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        auto multisampling =
            vk::PipelineMultisampleStateCreateInfo()
                .setSampleShadingEnable(false)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

        auto noBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false);

        auto noDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto emptyVertexInput = vk::PipelineVertexInputStateCreateInfo()
                                    .setVertexBindingDescriptions({})
                                    .setVertexAttributeDescriptions({});

        // ------------------------------------------------------------------
        // Create pipelines
        // ------------------------------------------------------------------
        auto makePipeline = [&](const auto& stages, uint32_t subpass) {
            auto blendState =
                vk::PipelineColorBlendStateCreateInfo()
                    .setLogicOpEnable(false)
                    .setLogicOp(vk::LogicOp::eCopy)
                    .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                    .setAttachmentCount(1)
                    .setPAttachments(&noBlendAttachment);

            return mDevice->Get()
                .createGraphicsPipeline(
                    nullptr,
                    vk::GraphicsPipelineCreateInfo()
                        .setStages(stages)
                        .setPVertexInputState(&emptyVertexInput)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPDepthStencilState(&noDepthStencil)
                        .setPMultisampleState(&multisampling)
                        .setPColorBlendState(&blendState)
                        .setPDynamicState(&dynamicState)
                        .setLayout(pipelineLayout)
                        .setRenderPass(renderPass)
                        .setSubpass(subpass)
                        .setBasePipelineHandle(nullptr))
                .value;
        };

        auto thresholdPipeline =
            makePipeline(threshStages, BloomThresholdSubpass);
        auto downsamplePipeline =
            makePipeline(downStages, BloomDownsampleSubpass);
        auto upsamplePipeline = makePipeline(upStages, BloomUpsampleSubpass);

        // Cleanup shaders
        mDevice->Get().destroyShaderModule(vertShader->Get());
        mDevice->Get().destroyShaderModule(threshFrag->Get());
        mDevice->Get().destroyShaderModule(downFrag->Get());
        mDevice->Get().destroyShaderModule(upFrag->Get());

        // ------------------------------------------------------------------
        // Framebuffers (indexed by in-flight frame, not swapchain image)
        // ------------------------------------------------------------------
        auto framebuffers = std::vector<vk::Framebuffer>(frameCount);

        for (std::uint32_t i = 0; i < frameCount; i++)
        {
            auto fbAttachments = std::vector<vk::ImageView> {
                bloomThresholdImages[i]->GetImageView(),
                bloomDownImages[i]->GetImageView(),
                bloomUpImages[i]->GetImageView(),
            };
            auto fbInfo =
                vk::FramebufferCreateInfo()
                    .setRenderPass(renderPass)
                    .setAttachments(fbAttachments)
                    .setWidth(halfExtent.width)
                    .setHeight(halfExtent.height)
                    .setLayers(1);

            framebuffers[i] = mDevice->Get().createFramebuffer(fbInfo);
        }

        auto finalSets = std::vector<vk::DescriptorSet>(allSets);

        return skr::MakeArc<BloomPass>(
            mDevice, mFreyaOptions, mSurface, halfExtent, renderPass,
            pipelineLayout, thresholdPipeline, downsamplePipeline,
            upsamplePipeline, std::move(bloomThresholdImages),
            std::move(bloomDownImages), std::move(bloomUpImages), framebuffers,
            descriptorPool,
            std::vector<vk::DescriptorSetLayout>(
                layouts.size(), descriptorSetLayout),
            finalSets, defaultSampler);
    }

    vk::RenderPass BloomPassBuilder::createRenderPass() const
    {
        auto attachments = std::vector<vk::AttachmentDescription> {
            // 0: Bloom threshold (color, cleared before use)
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // 1: Bloom downsample
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // 2: Bloom upsample (stored for blit)
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Subpass 0: threshold (writes attachment 0, no inputs)
        auto threshColorRef =
            vk::AttachmentReference()
                .setAttachment(BloomThresholdAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Subpass 1: downsample (reads attachment 0 as input, writes attachment
        // 1)
        auto thresholdInputRef =
            vk::AttachmentReference()
                .setAttachment(BloomThresholdAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto downColorRef =
            vk::AttachmentReference()
                .setAttachment(BloomDownAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Subpass 2: upsample (reads attachment 1 as input, writes attachment
        // 2)
        auto downInputRef =
            vk::AttachmentReference()
                .setAttachment(BloomDownAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto upColorRef =
            vk::AttachmentReference()
                .setAttachment(BloomUpAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto subpasses = std::vector<vk::SubpassDescription> {
            // Subpass 0: threshold — writes to attachment 0
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(threshColorRef),
            // Subpass 1: downsample — reads attachment 0, writes attachment 1
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setInputAttachments(thresholdInputRef)
                .setColorAttachments(downColorRef),
            // Subpass 2: upsample — reads attachment 1, writes attachment 2
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setInputAttachments(downInputRef)
                .setColorAttachments(upColorRef),
        };

        auto dependencies = std::vector<vk::SubpassDependency> {
            // External → threshold
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(BloomThresholdSubpass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
            // Threshold → downsample (transition threshold to shader read)
            vk::SubpassDependency()
                .setSrcSubpass(BloomThresholdSubpass)
                .setDstSubpass(BloomDownsampleSubpass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Downsample → upsample (transition down to shader read)
            vk::SubpassDependency()
                .setSrcSubpass(BloomDownsampleSubpass)
                .setDstSubpass(BloomUpsampleSubpass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Upsample → external
            vk::SubpassDependency()
                .setSrcSubpass(BloomUpsampleSubpass)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eNone)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        return mDevice->Get().createRenderPass(renderPassInfo);
    }
} // namespace FREYA_NAMESPACE
