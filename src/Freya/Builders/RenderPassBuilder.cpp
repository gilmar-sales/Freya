#include "Freya/Builders/RenderPassBuilder.hpp"

#include "Freya/Asset/Material.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "RenderPassBuilder.hpp"

namespace FREYA_NAMESPACE
{
    skr::Arc<RenderPass> RenderPassBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::RenderPass':");

        auto renderPass = createRenderPass();

        auto vertShaderModule =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Forward/Vert.vert.spv")
                .Build();

        auto fragShaderModule =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Forward/Frag.frag.spv")
                .Build();

        auto vertShaderStageInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(vertShaderModule->Get())
                .setPName("main");

        auto fragShaderStageInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(fragShaderModule->Get())
                .setPName("main");

        auto shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

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
                .setDepthClampEnable(false)
                .setRasterizerDiscardEnable(false)
                .setPolygonMode(vk::PolygonMode::eFill)
                .setCullMode(vk::CullModeFlagBits::eBack)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f)
                .setDepthBiasEnable(false);

        auto colorBlendAttachment =
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
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                .setAttachmentCount(1)
                .setPAttachments(&colorBlendAttachment);

        auto dynamicStates = std::vector { vk::DynamicState::eViewport,
                                           vk::DynamicState::eScissor };

        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        std::array poolSizes = {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(mFreyaOptions->frameCount * 3),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(mFreyaOptions->frameCount * 12),
        };

        auto poolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(static_cast<std::uint32_t>(poolSizes.size()))
                .setPPoolSizes(poolSizes.data())
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

        auto lightBufferLayoutBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto irradianceBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto prefilterBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto brdfBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto ltcMatrixBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto ltcAmplBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(6)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto shadowUboBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(7)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto cascadeShadowBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(8)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto spotShadowBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(9)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto pointShadowBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(10)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto cascadeDepthBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(11)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto spotDepthBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(12)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto directionalMaskBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(13)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto descriptorSetBindings = std::array {
            uboLayoutBinding,     lightBufferLayoutBinding,
            irradianceBinding,    prefilterBinding,
            brdfBinding,          ltcMatrixBinding,
            ltcAmplBinding,       shadowUboBinding,
            cascadeShadowBinding, spotShadowBinding,
            pointShadowBinding,   cascadeDepthBinding,
            spotDepthBinding,     directionalMaskBinding,
        };

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

        constexpr std::uint32_t maxMaterialSets = 2 << 16;

        auto samplerPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(5 * maxMaterialSets),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(maxMaterialSets),
        };

        auto samplerPoolInfo = vk::DescriptorPoolCreateInfo()
                                   .setPoolSizes(samplerPoolSizes)
                                   .setMaxSets(maxMaterialSets);

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
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr),
        };

        auto samplerDescriptorSetCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(
                samplerDescriptorSetBindings);

        auto samplerLayout = mDevice->Get().createDescriptorSetLayout(
            samplerDescriptorSetCreateInfo);

        const auto pipelineLayouts =
            std::array { frameLayouts[0], samplerLayout };

        auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo().setSetLayouts(pipelineLayouts);

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
            auto bufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(uniformBuffer->Get())
                    .setOffset(sizeof(ProjectionUniformBuffer) * i)
                    .setRange(sizeof(ProjectionUniformBuffer));

            auto descriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufferInfo);

            mDevice->Get().updateDescriptorSets(
                1, &descriptorWriter, 0, nullptr);
        }

        // Update light buffer descriptor sets (binding 1)
        for (auto i = 0; i < mFreyaOptions->frameCount; i++)
        {
            auto lightBufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mLightService->GetBuffer()->Get())
                    .setOffset(i * sizeof(LightUniformBuffer))
                    .setRange(sizeof(LightUniformBuffer));

            auto lightBufferWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(1)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(lightBufferInfo);

            mDevice->Get().updateDescriptorSets(
                1, &lightBufferWriter, 0, nullptr);
        }

        // IBL samplers (bindings 2-4) — static for all frames
        for (auto i = 0; i < mFreyaOptions->frameCount; i++)
        {
            auto irradianceInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mIblService->GetIrradianceSampler())
                    .setImageView(
                        mIblService->GetIrradianceMap()->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto prefilterInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mIblService->GetEnvironmentSampler())
                    .setImageView(
                        mIblService->GetEnvironmentMap()->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto brdfInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mIblService->GetBrdfSampler())
                    .setImageView(mIblService->GetBrdfLut()->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto ltcMatInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mIblService->GetLtcSampler())
                    .setImageView(
                        mIblService->GetLtcMatrixMap()->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto ltcAmplInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mIblService->GetLtcSampler())
                    .setImageView(mIblService->GetLtcAmplMap()->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto iblWrites = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(irradianceInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(3)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(prefilterInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(4)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(brdfInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(5)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(ltcMatInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(6)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(ltcAmplInfo),
            };

            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(iblWrites.size()), iblWrites.data(),
                0, nullptr);
        }

        constexpr auto shadowMapLayout =
            vk::ImageLayout::eShaderReadOnlyOptimal;

        for (auto i = 0; i < mFreyaOptions->frameCount; i++)
        {
            auto shadowUboInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mShadowPass->GetUniformBuffer()->Get())
                    .setOffset(mShadowPass->GetUniformBufferOffset(
                        static_cast<std::uint32_t>(i)))
                    .setRange(sizeof(ShadowUniformBuffer));

            auto shadowUboWrite =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(7)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(shadowUboInfo);

            auto cascadeInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mShadowPass->GetCompareSampler())
                    .setImageView(mShadowPass->GetCascadeView())
                    .setImageLayout(shadowMapLayout);

            auto spotInfo = vk::DescriptorImageInfo()
                                .setSampler(mShadowPass->GetCompareSampler())
                                .setImageView(mShadowPass->GetSpotView())
                                .setImageLayout(shadowMapLayout);

            auto pointInfo = vk::DescriptorImageInfo()
                                 .setSampler(mShadowPass->GetCompareSampler())
                                 .setImageView(mShadowPass->GetPointView())
                                 .setImageLayout(shadowMapLayout);

            auto cascadeDepthInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mShadowPass->GetSampler())
                    .setImageView(mShadowPass->GetCascadeView())
                    .setImageLayout(shadowMapLayout);

            auto spotDepthInfo =
                vk::DescriptorImageInfo()
                    .setSampler(mShadowPass->GetSampler())
                    .setImageView(mShadowPass->GetSpotView())
                    .setImageLayout(shadowMapLayout);

            // Temporary white until Renderer binds the denoise mask.
            auto maskInfo = vk::DescriptorImageInfo()
                                .setSampler(mShadowPass->GetSampler())
                                .setImageView(mShadowPass->GetCascadeView())
                                .setImageLayout(shadowMapLayout);

            auto shadowWrites = std::array {
                shadowUboWrite,
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(8)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(cascadeInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(9)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(spotInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(10)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(pointInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(11)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(cascadeDepthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(12)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(spotDepthInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(13)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(maskInfo),
            };

            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(shadowWrites.size()),
                shadowWrites.data(), 0, nullptr);
        }

        auto pipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        mLogger->Assert(pipelineLayout, "\tFailed to create pipeline layout.");

        auto depthStencilInfo =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        const auto vkSampleCount =
            static_cast<vk::SampleCountFlagBits>(mFreyaOptions->sampleCount);

        auto multisamplingInfo =
            vk::PipelineMultisampleStateCreateInfo()
                .setSampleShadingEnable(false)
                .setRasterizationSamples(vkSampleCount);

        auto pipelineInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(shaderStages)
                .setPVertexInputState(&vertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPColorBlendState(&colorBlending)
                .setPDynamicState(&dynamicState)
                .setLayout(pipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(0)
                .setBasePipelineHandle(nullptr)
                .setPDepthStencilState(&depthStencilInfo)
                .setPMultisampleState(&multisamplingInfo);

        auto graphicsPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, pipelineInfo).value;

        mLogger->Assert(graphicsPipeline,
                        "\tFailed to create the graphics pipeline.");

        mDevice->Get().destroyShaderModule(vertShaderModule->Get());
        mDevice->Get().destroyShaderModule(fragShaderModule->Get());

        // ── Fallback textures ────────────────────────────────────────────────
        // White texture for albedo/normal/roughness slots (bindings 0-2)
        // Black texture for emissive slot (binding 3)
        mLogger->LogTrace("\tCreating fallback textures...");

        std::uint32_t whitePixel =
            0xFFFFFFFF; // RGBA: R=255, G=255, B=255, A=255 (white)
        std::uint32_t blackPixel =
            0xFF000000; // RGBA: R=0, G=0, B=0, A=255 (black)
        constexpr vk::DeviceSize pixelSize = sizeof(whitePixel);
        constexpr std::uint32_t  texWidth  = 1;
        constexpr std::uint32_t  texHeight = 1;

        // Helper to create a 1x1 texture with given pixel data
        auto createFallbackTexture = [&](std::uint32_t pixel) {
            // 1. Staging buffer
            auto staging =
                BufferBuilder(mDevice)
                    .SetData(&pixel)
                    .SetSize(pixelSize)
                    .SetUsage(BufferUsage::Staging)
                    .Build();

            // 2. Image
            auto imgInfo =
                vk::ImageCreateInfo()
                    .setImageType(vk::ImageType::e2D)
                    .setFormat(vk::Format::eR8G8B8A8Unorm)
                    .setExtent({ texWidth, texHeight, 1 })
                    .setMipLevels(1)
                    .setArrayLayers(1)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setTiling(vk::ImageTiling::eOptimal)
                    .setUsage(vk::ImageUsageFlagBits::eTransferDst |
                              vk::ImageUsageFlagBits::eSampled)
                    .setSharingMode(vk::SharingMode::eExclusive)
                    .setInitialLayout(vk::ImageLayout::eUndefined);
            auto img = mDevice->Get().createImage(imgInfo);

            // 3. Device-local memory
            auto memReqs = mDevice->Get().getImageMemoryRequirements(img);
            auto memType = mPhysicalDevice->QueryCompatibleMemoryType(
                memReqs.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
            auto memAlloc  = vk::MemoryAllocateInfo()
                                 .setAllocationSize(memReqs.size)
                                 .setMemoryTypeIndex(memType);
            auto imgMemory = mDevice->Get().allocateMemory(memAlloc);
            mDevice->Get().bindImageMemory(img, imgMemory, 0);

            // 4. Copy data
            const auto gfxFamily =
                mDevice->GetQueueFamilyIndices().graphicsFamily.value();
            auto poolInfo =
                vk::CommandPoolCreateInfo()
                    .setQueueFamilyIndex(gfxFamily)
                    .setFlags(vk::CommandPoolCreateFlagBits::eTransient);
            auto cmdPool  = mDevice->Get().createCommandPool(poolInfo);
            auto cmdAlloc = vk::CommandBufferAllocateInfo()
                                .setCommandPool(cmdPool)
                                .setLevel(vk::CommandBufferLevel::ePrimary)
                                .setCommandBufferCount(1);
            auto cmdBufs  = mDevice->Get().allocateCommandBuffers(cmdAlloc);
            auto cmdBuf   = cmdBufs[0];

            cmdBuf.begin(vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

            auto bar1 =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setImage(img)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
            cmdBuf.pipelineBarrier(
                vk::PipelineStageFlagBits::eTopOfPipe,
                vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0,
                nullptr, 0, nullptr, 1, &bar1);

            auto copy =
                vk::BufferImageCopy()
                    .setBufferOffset(0)
                    .setBufferRowLength(0)
                    .setBufferImageHeight(0)
                    .setImageSubresource(
                        vk::ImageSubresourceLayers()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setMipLevel(0)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setImageOffset({ 0, 0, 0 })
                    .setImageExtent({ texWidth, texHeight, 1 });
            cmdBuf.copyBufferToImage(
                staging->Get(), img, vk::ImageLayout::eTransferDstOptimal,
                copy);

            auto bar2 =
                vk::ImageMemoryBarrier()
                    .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                    .setImage(img)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            cmdBuf.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlags(), 0, nullptr, 0, nullptr, 1, &bar2);

            cmdBuf.end();
            auto submit =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &cmdBuf);
            mDevice->GetGraphicsQueue().submit(submit);
            mDevice->GetGraphicsQueue().waitIdle();
            mDevice->Get().destroyCommandPool(cmdPool);

            // 5. Image view
            auto viewInfo =
                vk::ImageViewCreateInfo()
                    .setImage(img)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(vk::Format::eR8G8B8A8Unorm)
                    .setComponents(vk::ComponentMapping()
                                       .setR(vk::ComponentSwizzle::eIdentity)
                                       .setG(vk::ComponentSwizzle::eIdentity)
                                       .setB(vk::ComponentSwizzle::eIdentity)
                                       .setA(vk::ComponentSwizzle::eIdentity))
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            auto imgView = mDevice->Get().createImageView(viewInfo);

            // 6. Sampler
            auto sampInfo =
                vk::SamplerCreateInfo()
                    .setMagFilter(vk::Filter::eLinear)
                    .setMinFilter(vk::Filter::eLinear)
                    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                    .setAnisotropyEnable(false)
                    .setMaxAnisotropy(1.0f)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                    .setUnnormalizedCoordinates(false)
                    .setCompareEnable(false)
                    .setCompareOp(vk::CompareOp::eAlways)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setMipLodBias(0.0f)
                    .setMinLod(0.0f)
                    .setMaxLod(0.0f);
            auto samp = mDevice->Get().createSampler(sampInfo);

            return std::tuple { img, imgMemory, imgView, samp };
        };

        vk::Image        fbImage;
        vk::DeviceMemory fbImageMemory;
        vk::ImageView    fbImageView;
        vk::Sampler      fbSampler;
        vk::Image        fbEmissiveImage;
        vk::DeviceMemory fbEmissiveMemory;
        vk::ImageView    fbEmissiveView;
        vk::Sampler      fbEmissiveSampler;

        // Create white fallback (for albedo/normal/roughness)
        {
            auto result   = createFallbackTexture(whitePixel);
            fbImage       = std::get<0>(result);
            fbImageMemory = std::get<1>(result);
            fbImageView   = std::get<2>(result);
            fbSampler     = std::get<3>(result);
        }

        // Create black fallback for emissive (binding 3)
        {
            auto result       = createFallbackTexture(blackPixel);
            fbEmissiveImage   = std::get<0>(result);
            fbEmissiveMemory  = std::get<1>(result);
            fbEmissiveView    = std::get<2>(result);
            fbEmissiveSampler = std::get<3>(result);
        }

        // 7. Descriptor set (allocated from the shared sampler pool)
        auto fbSetAlloc    = vk::DescriptorSetAllocateInfo()
                                 .setDescriptorPool(samplerDescriptorPool)
                                 .setSetLayouts(samplerLayout);
        auto fbSamplerSets = mDevice->Get().allocateDescriptorSets(fbSetAlloc);
        auto fbFallbackSet = fbSamplerSets[0];

        auto fbImgInfoDesc =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fbImageView)
                .setSampler(fbSampler);

        // Separate image infos for fallback set - white for bindings 0-2, black
        // for binding 3
        auto fbWhiteImgInfoDesc =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fbImageView)
                .setSampler(fbSampler);

        auto fbBlackImgInfoDesc =
            vk::DescriptorImageInfo()
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setImageView(fbEmissiveView)
                .setSampler(fbEmissiveSampler);

        auto identityFactors = MaterialFactorsUniform {};
        auto fallbackFactorsBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(MaterialFactorsUniform))
                .SetData(&identityFactors)
                .Build();

        auto fbFactorsBufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(fallbackFactorsBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(MaterialFactorsUniform));

        const auto fbWrites = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbWhiteImgInfoDesc),
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbWhiteImgInfoDesc),
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(2)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbWhiteImgInfoDesc),
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(3)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbBlackImgInfoDesc),
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(4)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(
                    fbBlackImgInfoDesc), // metalness fallback = 0.0 (black)
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(5)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(fbFactorsBufferInfo),
        };
        mDevice->Get().updateDescriptorSets(fbWrites, nullptr);

        return skr::MakeArc<RenderPass>(
            mDevice,
            mFreyaOptions,
            renderPass,
            pipelineLayout,
            graphicsPipeline,
            uniformBuffer,
            frameLayouts,
            descriptorSets,
            descriptorPool,
            samplerLayout,
            samplerDescriptorPool,
            fbFallbackSet,
            fallbackFactorsBuffer,
            fbImage,
            fbImageMemory,
            fbImageView,
            fbSampler,
            fbEmissiveImage,
            fbEmissiveMemory,
            fbEmissiveView,
            fbEmissiveSampler);
    }

    vk::RenderPass RenderPassBuilder::createRenderPass() const
    {
        auto attachments = createAttachments();

        auto dependencies = createDependencies();

        const auto vkSampleCount =
            static_cast<vk::SampleCountFlagBits>(mFreyaOptions->sampleCount);

        auto colorAttachmentRef =
            vk::AttachmentReference()
                .setAttachment(ColorAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto depthAttachmentRef =
            vk::AttachmentReference()
                .setAttachment(DepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto colorAttachmentResolveRef =
            vk::AttachmentReference()
                .setAttachment(ColorResolveAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto mainPass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorAttachmentRef)
                .setPDepthStencilAttachment(&depthAttachmentRef);

        if (vkSampleCount != vk::SampleCountFlagBits::e1)
        {
            mainPass.setPResolveAttachments(&colorAttachmentResolveRef);
        }

        auto subpasses = std::vector { mainPass };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        auto renderPass = mDevice->Get().createRenderPass(renderPassInfo);

        return renderPass;
    }

    std::vector<vk::AttachmentDescription> RenderPassBuilder::
        createAttachments() const
    {
        auto format      = mSurface->QuerySurfaceFormat().format;
        auto depthFormat = mPhysicalDevice->GetDepthFormat();

        mLogger->LogTrace("\tColor Format: {}", vk::to_string(format));
        mLogger->LogTrace("\tDepth Format: {}", to_string(depthFormat));

        const auto vkSampleCount =
            static_cast<vk::SampleCountFlagBits>(mFreyaOptions->sampleCount);

        auto colorAttachment =
            vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(vkSampleCount)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        if (vkSampleCount != vk::SampleCountFlagBits::e1)
        {
            colorAttachment
                .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare);
        }

        auto depthAttachment =
            vk::AttachmentDescription()
                .setFormat(depthFormat)
                .setSamples(vkSampleCount)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(
                    vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto attachments = std::vector { colorAttachment, depthAttachment };

        if (vkSampleCount != vk::SampleCountFlagBits::e1)
        {
            auto colorAttachmentResolve =
                vk::AttachmentDescription()
                    .setFormat(format)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                    .setStoreOp(vk::AttachmentStoreOp::eStore)
                    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setInitialLayout(vk::ImageLayout::eUndefined)
                    .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

            attachments.push_back(colorAttachmentResolve);
        }

        return attachments;
    }

    std::vector<vk::SubpassDependency> RenderPassBuilder::createDependencies()
        const
    {
        // Forward render pass always has exactly 1 subpass — all dependencies
        // must reference only SubpassExternal and subpass 0.
        // Multi-subpass dependencies belong in DeferredCompressedPassBuilder.
        return std::vector {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
        };
    }

} // namespace FREYA_NAMESPACE
