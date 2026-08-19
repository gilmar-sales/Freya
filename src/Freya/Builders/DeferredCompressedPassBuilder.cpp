#include "DeferredCompressedPassBuilder.hpp"

#include "Freya/Internal/LightServiceGpu.hpp"
#include "Freya/Internal/VertexInput.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/UniformBuffer.hpp"

namespace FREYA_NAMESPACE
{
    namespace
    {
        vk::DescriptorSetLayoutBinding cisBinding(std::uint32_t binding)
        {
            return vk::DescriptorSetLayoutBinding()
                .setBinding(binding)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment);
        }

        vk::DescriptorSetLayoutBinding uboBinding(std::uint32_t binding)
        {
            return vk::DescriptorSetLayoutBinding()
                .setBinding(binding)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment);
        }

        vk::DescriptorSetLayoutBinding inputAttBinding(std::uint32_t binding)
        {
            return vk::DescriptorSetLayoutBinding()
                .setBinding(binding)
                .setDescriptorType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment);
        }

        vk::AttachmentDescription colorAttachment(vk::Format      format,
                                                  vk::ImageLayout finalLayout)
        {
            return vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(finalLayout);
        }
    } // namespace

    skr::Arc<DeferredCompressedPass> DeferredCompressedPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain, vk::Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
            extent = mSurface->QueryExtent();

        auto renderPass         = createGeometryRenderPass();
        auto lightingRenderPass = createLightingRenderPass();

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto depthVert = loadShader("DeferredCompressed/depth.vert.spv");
        auto depthFrag = loadShader("DeferredCompressed/depth.frag.spv");
        auto gbufVert  = loadShader("DeferredCompressed/gbuffer.vert.spv");
        auto gbufFrag  = loadShader("DeferredCompressed/gbuffer.frag.spv");
        auto lightVert = loadShader("DeferredCompressed/lighting.vert.spv");
        auto lightFrag = loadShader("DeferredCompressed/lighting.frag.spv");

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

        auto vertexBinding    = GetVertexBindingDescription();
        auto vertexAttributes = GetVertexAttributesDescription();

        auto vertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(vertexBinding)
                .setVertexAttributeDescriptions(vertexAttributes);

        auto emptyVertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
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

        auto fullscreenRasterizer =
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

        auto noBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false);

        auto additiveBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eOne)
                .setDstColorBlendFactor(vk::BlendFactor::eOne)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(vk::BlendFactor::eOne)
                .setAlphaBlendOp(vk::BlendOp::eAdd);

        auto depthPrepassDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess);

        auto gbufferDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreaterOrEqual
                                       : vk::CompareOp::eLessOrEqual);

        auto noDepthStencil = vk::PipelineDepthStencilStateCreateInfo()
                                  .setDepthTestEnable(false)
                                  .setDepthWriteEnable(false);

        // --- Geometry UBO descriptors ---
        auto uboPoolSize = vk::DescriptorPoolSize()
                               .setType(vk::DescriptorType::eUniformBuffer)
                               .setDescriptorCount(mFreyaOptions->frameCount);
        auto descriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(1)
                .setPPoolSizes(&uboPoolSize)
                .setMaxSets(mFreyaOptions->frameCount));

        auto uboLayoutBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                               vk::ShaderStageFlagBits::eFragment);

        auto frameLayouts = std::vector<vk::DescriptorSetLayout> {};
        for (auto i = 0u; i < mFreyaOptions->frameCount; i++)
        {
            frameLayouts.push_back(mDevice->Get().createDescriptorSetLayout(
                vk::DescriptorSetLayoutCreateInfo().setBindings(
                    uboLayoutBinding)));
        }

        auto descriptorSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(frameLayouts)
                .setDescriptorPool(descriptorPool));

        auto& samplerLayout = mMaterialResources->GetBindlessLayout();

        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(ProjectionUniformBuffer) *
                         mFreyaOptions->frameCount)
                .Build();

        for (auto i = 0u; i < mFreyaOptions->frameCount; i++)
        {
            auto bufInfo = vk::DescriptorBufferInfo()
                               .setBuffer(uniformBuffer->Get())
                               .setOffset(sizeof(ProjectionUniformBuffer) * i)
                               .setRange(sizeof(ProjectionUniformBuffer));
            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufInfo);
            mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
        }

        const auto vertexPipelineSetLayouts = std::array {
            frameLayouts[0],
            samplerLayout,
            mBoneResources->GetLayout(),
        };
        auto vertexPipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setSetLayouts(
                vertexPipelineSetLayouts));

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

        auto albedoImage     = createImage(ImageUsage::GBufferAlbedo);
        auto normalImage     = createImage(ImageUsage::GBufferNormal);
        auto pbrImage        = createImage(ImageUsage::GBufferPbr);
        auto sceneColorImage = createImage(ImageUsage::GBufferSceneColor);
        auto velocityImage   = createImage(ImageUsage::GBufferVelocity);
        auto depthImage      = createImage(ImageUsage::Depth);

        std::vector<skr::Arc<Image>> gbufferImages = { albedoImage, normalImage,
                                                       pbrImage };

        auto gbufferSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // Lighting set: 0-3 G-buffer samplers, 4 camera UBO, 5 lights,
        // 6-10 IBL, 11 shadow UBO, 12-14 shadow maps, 15 SSAO
        auto lightingBindings = std::array {
            cisBinding(0),  cisBinding(1),  cisBinding(2),  cisBinding(3),
            uboBinding(4),  uboBinding(5),  cisBinding(6),  cisBinding(7),
            cisBinding(8),  cisBinding(9),  cisBinding(10), uboBinding(11),
            cisBinding(12), cisBinding(13), cisBinding(14), cisBinding(15),
        };

        auto lightingSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(lightingBindings));

        std::array lightingPoolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(13 * mFreyaOptions->frameCount),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(3 * mFreyaOptions->frameCount),
        };

        auto lightingDescriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(
                    static_cast<std::uint32_t>(lightingPoolSizes.size()))
                .setPPoolSizes(lightingPoolSizes.data())
                .setMaxSets(mFreyaOptions->frameCount));

        auto lightingSetLayouts = std::vector<vk::DescriptorSetLayout>(
            mFreyaOptions->frameCount, lightingSetLayout);
        auto lightingSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(lightingDescriptorPool)
                .setSetLayouts(lightingSetLayouts));

        auto makeCisInfo = [&](vk::ImageView view, vk::ImageLayout layout) {
            return vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(view)
                .setImageLayout(layout);
        };

        auto depthSampleInfo =
            makeCisInfo(depthImage->GetImageView(),
                        vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        auto albedoSampleInfo =
            makeCisInfo(albedoImage->GetImageView(),
                        vk::ImageLayout::eShaderReadOnlyOptimal);
        auto normalSampleInfo =
            makeCisInfo(normalImage->GetImageView(),
                        vk::ImageLayout::eShaderReadOnlyOptimal);
        auto pbrSampleInfo =
            makeCisInfo(pbrImage->GetImageView(),
                        vk::ImageLayout::eShaderReadOnlyOptimal);

        auto irradianceInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetIrradianceSampler())
                .setImageView(mIblService->GetIrradianceMap()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto prefilterInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetEnvironmentSampler())
                .setImageView(mIblService->GetEnvironmentMap()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto brdfInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetBrdfSampler())
                .setImageView(mIblService->GetBrdfLut()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto ltcMatInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetLtcSampler())
                .setImageView(mIblService->GetLtcMatrixMap()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto ltcAmplInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetLtcSampler())
                .setImageView(mIblService->GetLtcAmplMap()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        constexpr auto shadowMapLayout =
            vk::ImageLayout::eShaderReadOnlyOptimal;

        for (std::uint32_t frameIndex = 0;
             frameIndex < mFreyaOptions->frameCount;
             ++frameIndex)
        {
            const auto set = lightingSets[frameIndex];

            auto cameraBufInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(uniformBuffer->Get())
                    .setOffset(sizeof(ProjectionUniformBuffer) * frameIndex)
                    .setRange(sizeof(ProjectionUniformBuffer));

            auto gbufferWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthSampleInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(albedoSampleInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normalSampleInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(3)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(pbrSampleInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(4)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(cameraBufInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<uint32_t>(gbufferWrites.size()),
                gbufferWrites.data(), 0, nullptr);

            auto lightBufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(LightServiceGpu::Buffer(*mLightService)->Get())
                    .setOffset(frameIndex * sizeof(LightUniformBuffer))
                    .setRange(sizeof(LightUniformBuffer));
            auto lightWrite =
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(5)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(lightBufferInfo);
            mDevice->Get().updateDescriptorSets(1, &lightWrite, 0, nullptr);

            auto iblWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(6)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(irradianceInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(7)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(prefilterInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(8)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(brdfInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(9)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(ltcMatInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(10)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(ltcAmplInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(iblWrites.size()), iblWrites.data(),
                0, nullptr);

            auto shadowUboInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mShadowPass->GetUniformBuffer()->Get())
                    .setOffset(mShadowPass->GetUniformBufferOffset(frameIndex))
                    .setRange(sizeof(ShadowUniformBuffer));

            auto spotInfo  = vk::DescriptorImageInfo()
                                 .setSampler(mShadowPass->GetCompareSampler())
                                 .setImageView(mShadowPass->GetSpotView())
                                 .setImageLayout(shadowMapLayout);
            auto pointInfo = vk::DescriptorImageInfo()
                                 .setSampler(mShadowPass->GetCompareSampler())
                                 .setImageView(mShadowPass->GetPointView())
                                 .setImageLayout(shadowMapLayout);
            auto cascadeInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mShadowPass->GetCompareSampler())
                    .setImageView(mShadowPass->GetCascadeView())
                    .setImageLayout(shadowMapLayout);

            auto shadowWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(11)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(shadowUboInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(12)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(cascadeInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(13)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(spotInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(14)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(pointInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(shadowWrites.size()),
                shadowWrites.data(), 0, nullptr);
        }

        auto lightingPushRange =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setOffset(0)
                .setSize(sizeof(std::uint32_t) * 4);

        auto fullscreenPipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(lightingSetLayout)
                .setPushConstantRanges(lightingPushRange));

        auto depthAttributes = GetVertexDepthAttributesDescription();
        auto depthVertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(vertexBinding)
                .setVertexAttributeDescriptions(depthAttributes);

        auto depthInfoPipe =
            vk::GraphicsPipelineCreateInfo()
                .setStages(depthStages)
                .setPVertexInputState(&depthVertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPDepthStencilState(&depthPrepassDepthStencil)
                .setPMultisampleState(&multisampling)
                .setPDynamicState(&dynamicState)
                .setLayout(vertexPipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(DefDepthPrePass);

        auto gbufferBlendAttachments =
            std::vector { noBlendAttachment, noBlendAttachment,
                          noBlendAttachment, noBlendAttachment,
                          noBlendAttachment };
        auto gbufferBlendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setAttachments(gbufferBlendAttachments);

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
                .setSubpass(DefGBufferPass);

        auto lightingBlendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setAttachments(additiveBlendAttachment);

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
                .setRenderPass(lightingRenderPass)
                .setSubpass(0);

        auto depthPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, depthInfoPipe).value;
        auto gbufferPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, gbufferInfo).value;
        auto lightingPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, lightingInfo).value;

        auto destroyShader = [&](const skr::Arc<ShaderModule>& mod) {
            mDevice->Get().destroyShaderModule(mod->Get());
        };
        destroyShader(depthVert);
        destroyShader(depthFrag);
        destroyShader(gbufVert);
        destroyShader(gbufFrag);
        destroyShader(lightVert);
        destroyShader(lightFrag);

        auto frames       = swapChain->GetFrames();
        auto framebuffers = std::vector<vk::Framebuffer>(frames.size());

        for (std::size_t i = 0; i < frames.size(); i++)
        {
            auto attachments = std::vector<vk::ImageView> {
                depthImage->GetImageView(),      albedoImage->GetImageView(),
                normalImage->GetImageView(),     pbrImage->GetImageView(),
                sceneColorImage->GetImageView(), velocityImage->GetImageView(),
            };
            framebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(renderPass)
                    .setAttachments(attachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }

        auto lightingFramebuffer = mDevice->Get().createFramebuffer(
            vk::FramebufferCreateInfo()
                .setRenderPass(lightingRenderPass)
                .setAttachments(sceneColorImage->GetImageView())
                .setWidth(extent.width)
                .setHeight(extent.height)
                .setLayers(1));

        return skr::MakeArc<DeferredCompressedPass>(
            mDevice, mFreyaOptions, mSurface, renderPass, vertexPipelineLayout,
            fullscreenPipelineLayout, depthPipeline, gbufferPipeline,
            lightingPipeline, uniformBuffer, frameLayouts, descriptorSets,
            descriptorPool, gbufferImages, sceneColorImage, velocityImage,
            depthImage, framebuffers, lightingRenderPass, lightingFramebuffer,
            lightingSetLayout, lightingDescriptorPool, lightingSets,
            mMaterialResources, mBoneResources, gbufferSampler, extent);
    }

    vk::RenderPass DeferredCompressedPassBuilder::createGeometryRenderPass()
        const
    {
        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(mPhysicalDevice->GetDepthFormat())
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
            colorAttachment(vk::Format::eR8G8B8A8Unorm,
                            vk::ImageLayout::eShaderReadOnlyOptimal),
            colorAttachment(vk::Format::eA2B10G10R10UnormPack32,
                            vk::ImageLayout::eShaderReadOnlyOptimal),
            colorAttachment(vk::Format::eR8G8B8A8Unorm,
                            vk::ImageLayout::eShaderReadOnlyOptimal),
            colorAttachment(vk::Format::eR16G16B16A16Sfloat,
                            vk::ImageLayout::eColorAttachmentOptimal),
            colorAttachment(vk::Format::eR16G16Sfloat,
                            vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        auto depthAttRef =
            vk::AttachmentReference()
                .setAttachment(DefDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto gbufferColorRefs = std::vector {
            vk::AttachmentReference()
                .setAttachment(DefAlbedoAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefNormalAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefPbrAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefSceneColorAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference()
                .setAttachment(DefVelocityAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal),
        };

        auto subpasses = std::vector<vk::SubpassDescription> {
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthAttRef),
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(gbufferColorRefs)
                .setPDepthStencilAttachment(&depthAttRef),
        };

        auto dependencies = std::vector<vk::SubpassDependency> {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(DefDepthPrePass)
                .setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite),
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
            vk::SubpassDependency()
                .setSrcSubpass(DefGBufferPass)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eLateFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eComputeShader |
                                 vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        };

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies));
    }

    vk::RenderPass DeferredCompressedPassBuilder::createLightingRenderPass()
        const
    {
        auto sceneColorAttachment =
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto sceneColorRef =
            vk::AttachmentReference().setAttachment(0).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(sceneColorRef);

        auto dependency =
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eComputeShader |
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead |
                                  vk::AccessFlagBits::eColorAttachmentWrite);

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(sceneColorAttachment)
                .setSubpasses(subpass)
                .setDependencies(dependency));
    }

} // namespace FREYA_NAMESPACE
