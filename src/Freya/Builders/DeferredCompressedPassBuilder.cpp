#include "DeferredCompressedPassBuilder.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    Ref<DeferredCompressedPass> DeferredCompressedPassBuilder::Build(
        const Ref<SwapChain>& swapChain)
    {
        auto renderPass = createRenderPass();

        // ------------------------------------------------------------------
        // Load all shader modules
        // ------------------------------------------------------------------
        auto loadShader = [&](const std::string& path) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(path)
                .Build();
        };

        auto depthVert =
            loadShader("./Resources/Shaders/DeferredCompressed/depth.vert.spv");
        auto depthFrag =
            loadShader("./Resources/Shaders/DeferredCompressed/depth.frag.spv");
        auto gbufVert = loadShader(
            "./Resources/Shaders/DeferredCompressed/gbuffer.vert.spv");
        auto gbufFrag = loadShader(
            "./Resources/Shaders/DeferredCompressed/gbuffer.frag.spv");
        auto lightVert = loadShader(
            "./Resources/Shaders/DeferredCompressed/lighting.vert.spv");
        auto lightFrag = loadShader(
            "./Resources/Shaders/DeferredCompressed/lighting.frag.spv");
        auto transVert = loadShader(
            "./Resources/Shaders/DeferredCompressed/translucency.vert.spv");
        auto transFrag = loadShader(
            "./Resources/Shaders/DeferredCompressed/translucency.frag.spv");

        auto makeStage = [](vk::ShaderModule        module,
                            vk::ShaderStageFlagBits stage) {
            return vk::PipelineShaderStageCreateInfo()
                .setStage(stage)
                .setModule(module)
                .setPName("main");
        };

        auto depthStages = {
            makeStage(depthVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(depthFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto gbufStages = {
            makeStage(gbufVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(gbufFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto lightStages = {
            makeStage(lightVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(lightFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };
        auto transStages = {
            makeStage(transVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(transFrag->Get(), vk::ShaderStageFlagBits::eFragment)
        };

        // ------------------------------------------------------------------
        // Vertex input descriptions (shared by depth, gbuffer, translucent)
        // ------------------------------------------------------------------
        auto vertexBinding    = Vertex::GetBindingDescription();
        auto vertexAttributes = Vertex::GetAttributesDescription();

        auto vertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(vertexBinding)
                .setVertexAttributeDescriptions(vertexAttributes);

        auto emptyVertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions({})
                .setVertexAttributeDescriptions({});

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
                .setCullMode(vk::CullModeFlagBits::eBack)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f)
                .setDepthBiasEnable(false);

        auto fullscreenRasterizer =
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

        constexpr auto vkSampleCount = vk::SampleCountFlagBits::e1;

        auto multisampling = vk::PipelineMultisampleStateCreateInfo()
                                 .setSampleShadingEnable(false)
                                 .setRasterizationSamples(vkSampleCount);

        auto noBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false);

        auto colorBlending =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

        // ------------------------------------------------------------------
        // Depth/stencil states
        // ------------------------------------------------------------------
        auto depthPrepassDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto gbufferDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreaterOrEqual
                                       : vk::CompareOp::eLessOrEqual)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto noDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto translucentDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        // ------------------------------------------------------------------
        // Color blend attachments per subpass
        // ------------------------------------------------------------------
        // Subpass 1 (gbuffer): 5 color attachments
        auto gbufferBlendAttachments = std::vector {
            noBlendAttachment, // position
            noBlendAttachment, // normal
            noBlendAttachment, // albedo
            noBlendAttachment, // emissive
            noBlendAttachment  // material
        };

        // Subpass 2 (lighting): 1 color attachment (opaque)
        // Subpass 3 (translucent): 1 color attachment (translucent) with alpha
        auto translucentBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                .setAlphaBlendOp(vk::BlendOp::eAdd);

        // ------------------------------------------------------------------
        // Descriptor resources (UBO layout)
        // ------------------------------------------------------------------
        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(mFreyaOptions->frameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(mFreyaOptions->frameCount);

        auto descriptorPool = mDevice->Get().createDescriptorPool(poolInfo);

        auto uboLayoutBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                               vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto descriptorSetBindings = std::array { uboLayoutBinding };

        auto descriptorSetLayoutCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(
                descriptorSetBindings);

        auto frameLayouts = std::vector<vk::DescriptorSetLayout> {};
        for (auto i = 0; i < mFreyaOptions->frameCount; i++)
        {
            frameLayouts.push_back(mDevice->Get().createDescriptorSetLayout(
                descriptorSetLayoutCreateInfo));
        }

        auto descriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(frameLayouts)
                .setDescriptorPool(descriptorPool);

        auto descriptorSets =
            mDevice->Get().allocateDescriptorSets(descriptorSetAllocInfo);

        // ------------------------------------------------------------------
        // Sampler descriptor pool and layout
        // ------------------------------------------------------------------
        auto samplerPoolSize =
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(2 << 16);

        auto samplerPoolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(1)
                .setPPoolSizes(&samplerPoolSize)
                .setMaxSets(2 << 16);

        auto samplerDescriptorPool =
            mDevice->Get().createDescriptorPool(samplerPoolInfo);

        auto samplerDescriptorSetBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
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
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
        };

        auto samplerDescriptorSetCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(
                samplerDescriptorSetBindings);

        auto samplerLayout = mDevice->Get().createDescriptorSetLayout(
            samplerDescriptorSetCreateInfo);

        // ------------------------------------------------------------------
        // Uniform buffer
        // ------------------------------------------------------------------
        uint64_t minimumBufferSize = 1024 * 1024;
        uint64_t bufferSize =
            sizeof(ProjectionUniformBuffer) * mFreyaOptions->frameCount;

        auto uniformBuffer =
            mServiceProvider->GetService<BufferBuilder>()
                ->SetUsage(BufferUsage::Uniform)
                .SetSize(std::max(bufferSize, minimumBufferSize))
                .Build();

        for (auto i = 0; i < mFreyaOptions->frameCount; i++)
        {
            auto bufInfo = vk::DescriptorBufferInfo()
                               .setBuffer(uniformBuffer->Get())
                               .setOffset(sizeof(ProjectionUniformBuffer) * i)
                               .setRange(sizeof(ProjectionUniformBuffer));

            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufInfo);

            mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
        }

        // ------------------------------------------------------------------
        // Vertex pipeline layout (set 0 = UBO, set 1 = samplers)
        // ------------------------------------------------------------------
        const auto vertexPipelineSetLayouts =
            std::array { frameLayouts[0], samplerLayout };

        auto vertexPipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo().setSetLayouts(
                vertexPipelineSetLayouts);

        auto vertexPipelineLayout =
            mDevice->Get().createPipelineLayout(vertexPipelineLayoutInfo);

        // ------------------------------------------------------------------
        // G-buffer and intermediate images
        // ------------------------------------------------------------------
        const auto extent = mSurface->QueryExtent();

        auto createImage =
            [&](ImageUsage                usage,
                std::optional<vk::Format> format = std::nullopt) {
                auto builder =
                    mServiceProvider->GetService<ImageBuilder>()
                        ->SetUsage(usage)
                        .SetWidth(extent.width)
                        .SetHeight(extent.height)
                        .SetSamples(vk::SampleCountFlagBits::e1);
                if (format.has_value())
                    builder.SetFormat(format.value());
                return builder.Build();
            };

        auto positionImage = createImage(ImageUsage::GBufferPosition);
        auto normalImage   = createImage(ImageUsage::GBufferNormal);
        auto albedoImage   = createImage(ImageUsage::GBufferAlbedo);
        auto emissiveImage = createImage(ImageUsage::GBufferEmissive);
        auto materialImage = createImage(ImageUsage::GBufferMetalness);
        auto depthImage    = createImage(ImageUsage::Depth);
        auto translucentImage =
            createImage(ImageUsage::Color, vk::Format::eR8G8B8A8Unorm);
        auto opaqueImage =
            createImage(ImageUsage::Color, vk::Format::eR8G8B8A8Unorm);

        std::vector<Ref<Image>> gbufferImages = {
            positionImage, normalImage, albedoImage, emissiveImage,
            materialImage
        };

        // ------------------------------------------------------------------
        // Input attachment descriptor set layout and pool
        // ------------------------------------------------------------------
        auto inputBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
            vk::DescriptorSetLayoutBinding()
                .setBinding(6)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
        };

        auto inputLayoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(inputBindings);

        auto inputAttachmentLayout =
            mDevice->Get().createDescriptorSetLayout(inputLayoutInfo);

        std::array inputPoolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(12),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1),
        };

        auto inputPoolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(
                    static_cast<std::uint32_t>(inputPoolSizes.size()))
                .setPPoolSizes(inputPoolSizes.data())
                .setMaxSets(1);

        auto inputAttachmentPool =
            mDevice->Get().createDescriptorPool(inputPoolInfo);

        // Allocate lighting input descriptor set
        auto inputSetLayouts = std::vector { inputAttachmentLayout };

        auto inputSetAlloc = vk::DescriptorSetAllocateInfo()
                                 .setDescriptorPool(inputAttachmentPool)
                                 .setSetLayouts(inputSetLayouts);

        auto lightingInputSet =
            mDevice->Get().allocateDescriptorSets(inputSetAlloc)[0];

        // --- Update lighting input descriptor set ---
        auto depthInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                .setImageView(depthImage->GetImageView());

        auto posInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(positionImage->GetImageView());

        auto normInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(normalImage->GetImageView());

        auto albedoInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(albedoImage->GetImageView());

        auto emissiveInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(emissiveImage->GetImageView());

        auto materialInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(materialImage->GetImageView());

        auto lightingInputWrites = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(depthInputInfo),
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(posInputInfo),
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(2)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(normInputInfo),
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(3)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(albedoInputInfo),
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(4)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(emissiveInputInfo),
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(5)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(materialInputInfo),
        };

        mDevice->Get().updateDescriptorSets(
            static_cast<uint32_t>(lightingInputWrites.size()),
            lightingInputWrites.data(), 0, nullptr);

        // --- Update light buffer binding (binding 6) ---
        auto frameIndex = 0;
        auto lightBufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mLightService->GetBuffer()->Get())
                .setOffset(frameIndex * sizeof(LightUniformBuffer))
                .setRange(sizeof(LightUniformBuffer));

        auto lightBufferWrite =
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(6)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(lightBufferInfo);

        mDevice->Get().updateDescriptorSets(1, &lightBufferWrite, 0, nullptr);

        // ------------------------------------------------------------------
        // Fullscreen pipeline layout (input attachments only)
        // ------------------------------------------------------------------
        auto fullscreenLayoutInfo =
            vk::PipelineLayoutCreateInfo().setSetLayouts(inputAttachmentLayout);

        auto fullscreenPipelineLayout =
            mDevice->Get().createPipelineLayout(fullscreenLayoutInfo);

        // ------------------------------------------------------------------
        // Create pipelines
        // ------------------------------------------------------------------
        auto depthInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(depthStages)
                .setPVertexInputState(&vertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPDepthStencilState(&depthPrepassDepthStencil)
                .setPMultisampleState(&multisampling)
                .setPDynamicState(&dynamicState)
                .setLayout(vertexPipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(DefDepthPrePass)
                .setBasePipelineHandle(nullptr);

        auto gbufferBlendState = colorBlending;
        gbufferBlendState.setAttachments(gbufferBlendAttachments);

        auto gbufferInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(gbufStages)
                .setPVertexInputState(&vertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPDepthStencilState(&gbufferDepthStencil)
                .setPMultisampleState(&multisampling)
                .setPColorBlendState(&gbufferBlendState)
                .setPDynamicState(&dynamicState)
                .setLayout(vertexPipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(DefGBufferPass)
                .setBasePipelineHandle(nullptr);

        auto lightingBlendAttachment = noBlendAttachment;
        auto lightingBlendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                .setAttachmentCount(1)
                .setPAttachments(&lightingBlendAttachment);

        auto lightingInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(lightStages)
                .setPVertexInputState(&emptyVertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&fullscreenRasterizer)
                .setPDepthStencilState(&noDepthStencil)
                .setPMultisampleState(&multisampling)
                .setPColorBlendState(&lightingBlendState)
                .setPDynamicState(&dynamicState)
                .setLayout(fullscreenPipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(DefLightingPass)
                .setBasePipelineHandle(nullptr);

        auto transBlendAttachment = translucentBlendAttachment;
        auto transBlendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                .setAttachmentCount(1)
                .setPAttachments(&transBlendAttachment);

        auto translucentInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(transStages)
                .setPVertexInputState(&vertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPDepthStencilState(&translucentDepthStencil)
                .setPMultisampleState(&multisampling)
                .setPColorBlendState(&transBlendState)
                .setPDynamicState(&dynamicState)
                .setLayout(vertexPipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(DefTranslucentPass)
                .setBasePipelineHandle(nullptr);

        auto depthPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, depthInfo).value;
        auto gbufferPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, gbufferInfo).value;
        auto lightingPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, lightingInfo).value;
        auto translucentPipeline =
            mDevice->Get()
                .createGraphicsPipeline(nullptr, translucentInfo)
                .value;

        // Cleanup shader modules
        auto destroyShader = [&](const Ref<ShaderModule>& mod) {
            mDevice->Get().destroyShaderModule(mod->Get());
        };
        destroyShader(depthVert);
        destroyShader(depthFrag);
        destroyShader(gbufVert);
        destroyShader(gbufFrag);
        destroyShader(lightVert);
        destroyShader(lightFrag);
        destroyShader(transVert);
        destroyShader(transFrag);

        // ------------------------------------------------------------------
        // Framebuffers (one per swapchain image)
        // ------------------------------------------------------------------
        auto frames = swapChain->GetFrames();

        auto framebuffers = std::vector<vk::Framebuffer>(frames.size());

        for (std::size_t i = 0; i < frames.size(); i++)
        {
            auto fbAttachments = std::vector<vk::ImageView> {
                depthImage->GetImageView(),       // 0: depth
                positionImage->GetImageView(),    // 1: position
                normalImage->GetImageView(),      // 2: normal
                albedoImage->GetImageView(),      // 3: albedo
                emissiveImage->GetImageView(),    // 4: emissive
                materialImage->GetImageView(),    // 5: material
                translucentImage->GetImageView(), // 6: translucent
                opaqueImage->GetImageView(),      // 7: opaque
            };

            auto fbInfo =
                vk::FramebufferCreateInfo()
                    .setRenderPass(renderPass)
                    .setAttachments(fbAttachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1);

            framebuffers[i] = mDevice->Get().createFramebuffer(fbInfo);
        }

        return skr::MakeRef<DeferredCompressedPass>(
            mDevice,
            mFreyaOptions,
            mSurface,
            renderPass,
            vertexPipelineLayout,
            fullscreenPipelineLayout,
            depthPipeline,
            gbufferPipeline,
            lightingPipeline,
            translucentPipeline,
            uniformBuffer,
            frameLayouts,
            descriptorSets,
            descriptorPool,
            gbufferImages,
            emissiveImage,
            depthImage,
            translucentImage,
            opaqueImage,
            framebuffers,
            inputAttachmentLayout,
            inputAttachmentPool,
            lightingInputSet,
            samplerLayout,
            samplerDescriptorPool);
    }

    // ------------------------------------------------------------------
    // createRenderPass: 4 subpasses, no bloom or composite
    // ------------------------------------------------------------------
    vk::RenderPass DeferredCompressedPassBuilder::createRenderPass() const
    {
        // 8 attachments (no backbuffer — composite pass handles it):
        //   0: Depth
        //   1: Position G-buffer
        //   2: Normal G-buffer
        //   3: Albedo G-buffer
        //   4: Emissive G-buffer (stored for bloom pass)
        //   5: Material G-buffer
        //   6: Translucent (stored for composite pass)
        //   7: Opaque (stored for composite pass)
        auto attachments = std::vector<vk::AttachmentDescription> {
            // Depth
            vk::AttachmentDescription()
                .setFormat(mPhysicalDevice->GetDepthFormat())
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(
                    vk::ImageLayout::eDepthStencilAttachmentOptimal),
            // Position G-buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Normal G-buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Albedo G-buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Srgb)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Emissive G-buffer (stored for bloom pass)
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Material buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Translucent (stored for composite pass)
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // Opaque (stored for composite pass)
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Subpass 0: Depth pre-pass
        auto depthRef =
            vk::AttachmentReference()
                .setAttachment(DefDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // Subpass 1: G-buffer color attachments
        auto gbufferColorRefs = std::vector {
            vk::AttachmentReference()
                .setAttachment(DefPositionAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefNormalAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefAlbedoAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefEmissiveAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefMaterialAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        // Subpass 2: Lighting input attachments
        auto lightingInputRefs = std::vector {
            vk::AttachmentReference()
                .setAttachment(DefDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DefPositionAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DefNormalAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DefAlbedoAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DefEmissiveAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DefMaterialAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        auto opaqueRef =
            vk::AttachmentReference()
                .setAttachment(DefOpaqueAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Subpass 3: Translucent
        auto translucentRef =
            vk::AttachmentReference()
                .setAttachment(DefTranslucentAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto subpasses = std::vector<vk::SubpassDescription> {
            // Subpass 0: Depth pre-pass
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthRef),
            // Subpass 1: G-buffer
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(gbufferColorRefs)
                .setPDepthStencilAttachment(&depthRef),
            // Subpass 2: Lighting
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setInputAttachments(lightingInputRefs)
                .setColorAttachments(opaqueRef),
            // Subpass 3: Translucent
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(translucentRef),
        };

        auto dependencies = std::vector<vk::SubpassDependency> {
            // External -> Depth pre-pass (depth write only)
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(DefDepthPrePass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite),
            // Depth pre-pass -> G-buffer
            vk::SubpassDependency()
                .setSrcSubpass(DefDepthPrePass)
                .setDstSubpass(DefGBufferPass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentRead |
                    vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // G-buffer -> Lighting
            vk::SubpassDependency()
                .setSrcSubpass(DefGBufferPass)
                .setDstSubpass(DefLightingPass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentRead)
                .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead |
                                  vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Lighting -> Translucent
            vk::SubpassDependency()
                .setSrcSubpass(DefLightingPass)
                .setDstSubpass(DefTranslucentPass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Translucent -> External (for next pass)
            vk::SubpassDependency()
                .setSrcSubpass(DefTranslucentPass)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eNone),
        };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        return mDevice->Get().createRenderPass(renderPassInfo);
    }

} // namespace FREYA_NAMESPACE
