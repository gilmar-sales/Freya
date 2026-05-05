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
        auto compVert = loadShader(
            "./Resources/Shaders/DeferredCompressed/composing.vert.spv");
        auto compFrag = loadShader(
            "./Resources/Shaders/DeferredCompressed/composing.frag.spv");

        auto makeStage = [](vk::ShaderModule        module,
                            vk::ShaderStageFlagBits stage) {
            return vk::PipelineShaderStageCreateInfo()
                .setStage(stage)
                .setModule(module)
                .setPName("main");
        };

        // Per-subpass shader stages
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
        auto compStages = {
            makeStage(compVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(compFrag->Get(), vk::ShaderStageFlagBits::eFragment)
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

        // Fullscreen passes (lighting, composite) have no vertex input
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

        // Fullscreen passes (lighting, composite) don't need culling
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

        // Deferred pipeline always uses single-sample attachments — MSAA is
        // not supported in the deferred path (intermediate targets are 1x).
        constexpr auto vkSampleCount = vk::SampleCountFlagBits::e1;

        auto multisampling = vk::PipelineMultisampleStateCreateInfo()
                                 .setSampleShadingEnable(false)
                                 .setRasterizationSamples(vkSampleCount);

        // No color blend by default
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
        // Reverse-Z depth prepass: greater-or-equal, write enabled
        auto depthPrepassDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        // G-buffer pass: depth test must pass for fragments at the same
        // depth written by the prepass. Use EQUAL or GREATER_OR_EQUAL
        // (reverse-Z) / LESS_OR_EQUAL (normal-Z). Depth write is disabled
        // since the prepass already wrote depth.
        auto gbufferDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreaterOrEqual
                                       : vk::CompareOp::eLessOrEqual)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        // Lighting/composite passes: depth test off (fullscreen quad)
        auto noDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        // Translucent pass: depth test on, write off
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
        // Subpass 0 (depth prepass): no color attachments
        // Subpass 1 (gbuffer): 3 color attachments (position, normal, albedo)
        auto gbufferBlendAttachments = std::vector {
            noBlendAttachment, // position
            noBlendAttachment, // normal
            noBlendAttachment  // albedo
        };

        // Subpass 2 (lighting): 1 color attachment (opaque)
        // Subpass 3 (translucent): 1 color attachment (translucent) with alpha
        // blending
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

        // Subpass 4 (composite): 1 color attachment (backbuffer)

        // ------------------------------------------------------------------
        // Descriptor resources (UBO layout — shared with forward pass pattern)
        // ------------------------------------------------------------------
        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(mFreyaOptions->frameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(mFreyaOptions->frameCount);

        auto descriptorPool = mDevice->Get().createDescriptorPool(poolInfo);

        // UBO layout binding at set 0, binding 0
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

        // Allocate descriptor sets (one per frame)
        auto descriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(frameLayouts)
                .setDescriptorPool(descriptorPool);

        auto descriptorSets =
            mDevice->Get().allocateDescriptorSets(descriptorSetAllocInfo);

        // ------------------------------------------------------------------
        // Sampler descriptor pool and layout (shared with forward pass
        // pattern)
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
        };

        auto samplerDescriptorSetCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(
                samplerDescriptorSetBindings);

        auto samplerLayout = mDevice->Get().createDescriptorSetLayout(
            samplerDescriptorSetCreateInfo);

        // ------------------------------------------------------------------
        // Uniform buffer (shared projection)
        // ------------------------------------------------------------------
        uint64_t minimumBufferSize = 1024 * 1024;
        uint64_t bufferSize =
            sizeof(ProjectionUniformBuffer) * mFreyaOptions->frameCount;

        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
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
        // G-buffer, depth, and intermediate images
        // ------------------------------------------------------------------
        const auto extent = mSurface->QueryExtent();

        auto createImage =
            [&](ImageUsage                usage,
                std::optional<vk::Format> format = std::nullopt) {
                // Deferred intermediate targets are always single-sample
                auto builder =
                    mServiceProvider->GetService<ImageBuilder>()
                        ->SetUsage(usage)
                        .SetWidth(extent.width)
                        .SetHeight(extent.height)
                        .SetSamples(vk::SampleCountFlagBits::e1);
                if (format.has_value())
                {
                    builder.SetFormat(format.value());
                }
                return builder.Build();
            };

        auto positionImage = createImage(ImageUsage::GBufferPosition);
        auto normalImage   = createImage(ImageUsage::GBufferNormal);
        auto albedoImage   = createImage(ImageUsage::GBufferAlbedo);
        auto depthImage    = createImage(ImageUsage::Depth);
        // Use fixed eR8G8B8A8Unorm for intermediate buffers so the format
        // matches the render pass declaration regardless of surface format.
        auto translucentImage =
            createImage(ImageUsage::Color, vk::Format::eR8G8B8A8Unorm);
        auto opaqueImage =
            createImage(ImageUsage::Color, vk::Format::eR8G8B8A8Unorm);

        std::vector<Ref<Image>> gbufferImages = { positionImage, normalImage,
                                                  albedoImage };

        // ------------------------------------------------------------------
        // Input attachment descriptor set layout and pool
        // ------------------------------------------------------------------
        // Lighting pass needs 4 input attachments (depth, position, normal,
        // albedo) at bindings 0-3, plus 1 light buffer uniform at binding 4
        // We allocate enough for the max needed across all fullscreen subpasses
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
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
        };

        auto inputLayoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(inputBindings);

        auto inputAttachmentLayout =
            mDevice->Get().createDescriptorSetLayout(inputLayoutInfo);

        // Pool for input attachment descriptor sets
        // 2 sets × 5 bindings each = 10 descriptors total (4 input + 1 uniform
        // per set)
        std::array inputPoolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(8),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(2),
        };

        auto inputPoolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(
                    static_cast<std::uint32_t>(inputPoolSizes.size()))
                .setPPoolSizes(inputPoolSizes.data())
                .setMaxSets(2);

        auto inputAttachmentPool =
            mDevice->Get().createDescriptorPool(inputPoolInfo);

        // Allocate both descriptor sets in one call:
        //   - set 0: lighting (bindings 0-3: depth, position, normal, albedo)
        //   - set 1: composite (bindings 0-1: opaque, translucent)
        auto inputSetLayouts =
            std::vector { inputAttachmentLayout, inputAttachmentLayout };

        auto inputSetAlloc = vk::DescriptorSetAllocateInfo()
                                 .setDescriptorPool(inputAttachmentPool)
                                 .setSetLayouts(inputSetLayouts);

        auto inputSets = mDevice->Get().allocateDescriptorSets(inputSetAlloc);
        auto lightingInputSet  = inputSets[0];
        auto compositeInputSet = inputSets[1];

        // --- Update lighting input descriptor set ---
        // Depth format images used as input attachments must use
        // eDepthStencilReadOnlyOptimal layout to match the subpass
        // attachment reference (Vulkan spec requirement).
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
        };

        mDevice->Get().updateDescriptorSets(
            static_cast<uint32_t>(lightingInputWrites.size()),
            lightingInputWrites.data(), 0, nullptr);

        // --- Update light buffer binding (binding 4) ---
        auto frameIndex = 0; // Will be updated per-frame at render time
        auto lightBufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mLightService->GetBuffer()->Get())
                .setOffset(frameIndex * sizeof(LightUniformBuffer))
                .setRange(sizeof(LightUniformBuffer));

        auto lightBufferWrite =
            vk::WriteDescriptorSet()
                .setDstSet(lightingInputSet)
                .setDstBinding(4)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(lightBufferInfo);

        mDevice->Get().updateDescriptorSets(1, &lightBufferWrite, 0, nullptr);

        // --- Update composite input descriptor set ---
        auto opaqueInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(opaqueImage->GetImageView());

        auto translInputInfo =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(translucentImage->GetImageView());

        auto compositeInputWrites = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(compositeInputSet)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(opaqueInputInfo),
            vk::WriteDescriptorSet()
                .setDstSet(compositeInputSet)
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setImageInfo(translInputInfo),
        };

        mDevice->Get().updateDescriptorSets(
            static_cast<uint32_t>(compositeInputWrites.size()),
            compositeInputWrites.data(), 0, nullptr);

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
                .setSubpass(DeferredDepthPrePass)
                .setBasePipelineHandle(nullptr);

        // GBuffer pass: 3 color attachments
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
                .setSubpass(DeferredGBufferPass)
                .setBasePipelineHandle(nullptr);

        // Lighting pass: no vertex input, fullscreen
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
                .setSubpass(DeferredLightingPass)
                .setBasePipelineHandle(nullptr);

        // Translucent pass
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
                .setSubpass(DeferredTranslucentPass)
                .setBasePipelineHandle(nullptr);

        // Composite pass: no vertex input, read opaque + translucent, write
        // backbuffer
        auto compositeBlendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                .setAttachmentCount(1)
                .setPAttachments(&noBlendAttachment);

        auto compositeInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(compStages)
                .setPVertexInputState(&emptyVertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&fullscreenRasterizer)
                .setPDepthStencilState(&noDepthStencil)
                .setPMultisampleState(&multisampling)
                .setPColorBlendState(&compositeBlendState)
                .setPDynamicState(&dynamicState)
                .setLayout(fullscreenPipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(DeferredCompositePass)
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
        auto compositePipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, compositeInfo).value;

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
        destroyShader(compVert);
        destroyShader(compFrag);

        // ------------------------------------------------------------------
        // Framebuffers (one per swapchain image)
        // ------------------------------------------------------------------
        auto frames = swapChain->GetFrames();

        auto framebuffers = std::vector<vk::Framebuffer>(frames.size());

        for (std::size_t i = 0; i < frames.size(); i++)
        {
            auto fbAttachments = std::vector<vk::ImageView> {
                frames[i].imageView,              // 0: backbuffer
                depthImage->GetImageView(),       // 1: depth
                positionImage->GetImageView(),    // 2: position
                normalImage->GetImageView(),      // 3: normal
                albedoImage->GetImageView(),      // 4: albedo
                translucentImage->GetImageView(), // 5: translucent
                opaqueImage->GetImageView()       // 6: opaque
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
            compositePipeline,
            uniformBuffer,
            frameLayouts,
            descriptorSets,
            descriptorPool,
            gbufferImages,
            depthImage,
            translucentImage,
            opaqueImage,
            framebuffers,
            inputAttachmentLayout,
            inputAttachmentPool,
            lightingInputSet,
            compositeInputSet,
            samplerLayout,
            samplerDescriptorPool);
    }

    // ------------------------------------------------------------------
    // createRenderPass
    // ------------------------------------------------------------------
    vk::RenderPass DeferredCompressedPassBuilder::createRenderPass() const
    {
        // 7 attachments (all VK_SAMPLE_COUNT_1_BIT — deferred uses dedicated
        // intermediate targets, not MSAA):
        //   0: Back buffer   (surface format, e.g. B8G8R8A8Unorm)
        //   1: Depth         (D32Sfloat)
        //   2: Position      (R16G16B16A16Sfloat)
        //   3: Normal        (R16G16B16A16Sfloat)
        //   4: Albedo        (R8G8B8A8Srgb)
        //   5: Translucent   (R8G8B8A8Unorm)
        //   6: Opaque        (R8G8B8A8Unorm)

        const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = std::vector<vk::AttachmentDescription> {
            // 0: Back buffer — must match the swapchain image format
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
            // 1: Depth
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
            // 2: Position G-buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // 3: Normal G-buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // 4: Albedo G-buffer (SRGB for proper gamma handling)
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Srgb)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // 5: Translucent buffer
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            // 6: Opaque buffer
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

        // Attachment references
        auto depthRef =
            vk::AttachmentReference()
                .setAttachment(DeferredDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // G-buffer color attachments (subpass 1 writes to 2, 3, 4)
        auto gbufferColorRefs = std::vector {
            vk::AttachmentReference()
                .setAttachment(DeferredPositionAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredNormalAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredAlbedoAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        // Lighting pass input attachments (read depth + g-buffer)
        auto lightingInputRefs = std::vector {
            vk::AttachmentReference()
                .setAttachment(DeferredDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredPositionAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredNormalAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredAlbedoAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Lighting writes to opaque
        auto opaqueRef =
            vk::AttachmentReference()
                .setAttachment(DeferredOpaqueAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Translucent writes to translucent buffer
        auto translucentRef =
            vk::AttachmentReference()
                .setAttachment(DeferredTranslucentAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Composite reads opaque + translucent
        auto compositeInputRefs = std::vector {
            vk::AttachmentReference()
                .setAttachment(DeferredOpaqueAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentReference()
                .setAttachment(DeferredTranslucentAttachment)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        // Composite writes to back buffer
        auto backbufferRef =
            vk::AttachmentReference()
                .setAttachment(DeferredBackAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Subpasses
        auto subpasses = std::vector {
            // Subpass 0: Depth pre-pass
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthRef),
            // Subpass 1: G-buffer generation
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
            // Subpass 4: Composite
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setInputAttachments(compositeInputRefs)
                .setColorAttachments(backbufferRef),
        };

        // Dependencies
        auto dependencies = std::vector {
            // External → Depth pre-pass (color + depth)
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(DeferredDepthPrePass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite),
            // Depth pre-pass → G-buffer (depth write done → gbuffer reads
            // depth)
            vk::SubpassDependency()
                .setSrcSubpass(DeferredDepthPrePass)
                .setDstSubpass(DeferredGBufferPass)
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
            // G-buffer → Lighting (color write + depth read → input read)
            // The depth attachment transitions from
            // eDepthStencilAttachmentOptimal (subpass 1 depth test) to
            // eDepthStencilReadOnlyOptimal (subpass 2 input attachment).
            vk::SubpassDependency()
                .setSrcSubpass(DeferredGBufferPass)
                .setDstSubpass(DeferredLightingPass)
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
            // Lighting → Translucent
            vk::SubpassDependency()
                .setSrcSubpass(DeferredLightingPass)
                .setDstSubpass(DeferredTranslucentPass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Translucent → Composite
            vk::SubpassDependency()
                .setSrcSubpass(DeferredTranslucentPass)
                .setDstSubpass(DeferredCompositePass)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            // Composite → External (presentation)
            vk::SubpassDependency()
                .setSrcSubpass(DeferredCompositePass)
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
