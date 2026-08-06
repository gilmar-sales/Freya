#include "DeferredCompressedPassBuilder.hpp"

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
    } // namespace

    skr::Arc<DeferredCompressedPass> DeferredCompressedPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain, vk::Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
            extent = mSurface->QueryExtent();

        auto gbufferRenderPass  = createRenderPass();
        auto lightingRenderPass = createLightingRenderPass();

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

        constexpr std::uint32_t maxMaterialSets = MAX_MATERIAL_SETS;

        auto samplerPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(5 * maxMaterialSets),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(maxMaterialSets),
        };
        auto samplerDescriptorPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setPoolSizes(samplerPoolSizes)
                .setMaxSets(maxMaterialSets));

        auto samplerBindings =
            std::array { cisBinding(0), cisBinding(1), cisBinding(2),
                         cisBinding(3), cisBinding(4), uboBinding(5) };
        auto samplerLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(samplerBindings));

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

        const auto vertexPipelineSetLayouts =
            std::array { frameLayouts[0], samplerLayout };
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

        std::vector<skr::Arc<Image>> gbufferImages = {
            positionImage, normalImage, albedoImage, emissiveImage,
            materialImage
        };

        auto gbufferSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // 12 shadow UBO, 13 cascade cmp, 14-15 spot/point cmp
        auto lightingBindings = std::array {
            cisBinding(0),  cisBinding(1),  cisBinding(2),  cisBinding(3),
            cisBinding(4),  cisBinding(5),  uboBinding(6),  cisBinding(7),
            cisBinding(8),  cisBinding(9),  cisBinding(10), cisBinding(11),
            uboBinding(12), cisBinding(13), cisBinding(14), cisBinding(15),
        };

        auto lightingSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(lightingBindings));

        std::array lightingPoolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(15 * mFreyaOptions->frameCount),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(2 * mFreyaOptions->frameCount),
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

        auto depthInfo =
            vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(depthImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);
        auto posInfo =
            vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(positionImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto normInfo =
            vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(normalImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto albedoInfo =
            vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(albedoImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto emissiveInfo =
            vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(emissiveImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto materialInfo =
            vk::DescriptorImageInfo()
                .setSampler(gbufferSampler)
                .setImageView(materialImage->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

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

            auto gbufferWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(depthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(posInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(normInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(3)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(albedoInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(4)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(emissiveInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(5)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(materialInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<uint32_t>(gbufferWrites.size()),
                gbufferWrites.data(), 0, nullptr);

            auto lightBufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mLightService->GetBuffer()->Get())
                    .setOffset(frameIndex * sizeof(LightUniformBuffer))
                    .setRange(sizeof(LightUniformBuffer));
            auto lightWrite =
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(6)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(lightBufferInfo);
            mDevice->Get().updateDescriptorSets(1, &lightWrite, 0, nullptr);

            auto iblWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(7)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(irradianceInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(8)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(prefilterInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(9)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(brdfInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(10)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(ltcMatInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(11)
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
                    .setDstBinding(12)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(shadowUboInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(13)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(cascadeInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(14)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(spotInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(15)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(pointInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(shadowWrites.size()),
                shadowWrites.data(), 0, nullptr);
        }

        auto fullscreenPipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setSetLayouts(lightingSetLayout));

        auto depthInfoPipe =
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
                .setRenderPass(gbufferRenderPass)
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
                .setRenderPass(gbufferRenderPass)
                .setSubpass(DefGBufferPass);

        auto lightingBlendAttachments = std::array {
            noBlendAttachment,
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask({})
                .setBlendEnable(false),
        };
        auto lightingBlendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setAttachments(lightingBlendAttachments);

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

        auto frames               = swapChain->GetFrames();
        auto gbufferFramebuffers  = std::vector<vk::Framebuffer>(frames.size());
        auto lightingFramebuffers = std::vector<vk::Framebuffer>(frames.size());

        for (std::size_t i = 0; i < frames.size(); i++)
        {
            auto gbAttachments = std::vector<vk::ImageView> {
                depthImage->GetImageView(),    positionImage->GetImageView(),
                normalImage->GetImageView(),   albedoImage->GetImageView(),
                emissiveImage->GetImageView(), materialImage->GetImageView(),
            };
            gbufferFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(gbufferRenderPass)
                    .setAttachments(gbAttachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));

            auto lightAttachments = std::vector<vk::ImageView> {
                opaqueImage->GetImageView(),
                translucentImage->GetImageView(),
            };
            lightingFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(lightingRenderPass)
                    .setAttachments(lightAttachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }

        return skr::MakeArc<DeferredCompressedPass>(
            mDevice, mFreyaOptions, mSurface, gbufferRenderPass,
            lightingRenderPass, vertexPipelineLayout, fullscreenPipelineLayout,
            depthPipeline, gbufferPipeline, lightingPipeline, uniformBuffer,
            frameLayouts, descriptorSets, descriptorPool, gbufferImages,
            emissiveImage, depthImage, translucentImage, opaqueImage,
            gbufferFramebuffers, lightingFramebuffers, lightingSetLayout,
            lightingDescriptorPool, lightingSets, samplerLayout,
            samplerDescriptorPool, gbufferSampler, extent);
    }

    vk::RenderPass DeferredCompressedPassBuilder::createRenderPass() const
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
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Srgb)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        auto depthRef =
            vk::AttachmentReference()
                .setAttachment(DefDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

        auto subpasses = std::vector<vk::SubpassDescription> {
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthRef),
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(gbufferColorRefs)
                .setPDepthStencilAttachment(&depthRef),
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
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
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
        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8G8B8A8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
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

        auto opaqueRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);
        auto translucentRef =
            vk::AttachmentReference().setAttachment(1).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal);

        auto colorRefs = std::vector { opaqueRef, translucentRef };

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRefs);

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
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(dependencies));
    }

} // namespace FREYA_NAMESPACE
