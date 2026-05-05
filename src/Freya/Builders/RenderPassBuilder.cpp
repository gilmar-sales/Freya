#include "Freya/Builders/RenderPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "RenderPassBuilder.hpp"

namespace FREYA_NAMESPACE
{
    Ref<RenderPass> RenderPassBuilder::Build()
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

        auto descriptorSetBindings = std::array {
            uboLayoutBinding,
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
                .setPImmutableSamplers(nullptr)
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

        // ── Fallback 1×1 white texture descriptor set ──────────────────
        // Bound at set 1 in RenderPass::Begin() so that pipelines which
        // statically use sampler descriptors never find set 1 unbound.
        mLogger->LogTrace("\tCreating fallback white texture...");

        std::uint32_t            whitePixel = 0xFFFFFFFF;
        constexpr vk::DeviceSize pixelSize  = sizeof(whitePixel);
        constexpr std::uint32_t  texWidth   = 1;
        constexpr std::uint32_t  texHeight  = 1;

        // 1. Staging buffer
        auto fbStaging =
            BufferBuilder(mDevice)
                .SetData(&whitePixel)
                .SetSize(pixelSize)
                .SetUsage(BufferUsage::Staging)
                .Build();

        // 2. Image
        auto fbImageInfo =
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
        auto fbImage = mDevice->Get().createImage(fbImageInfo);

        // 3. Device-local memory
        auto fbMemReqs = mDevice->Get().getImageMemoryRequirements(fbImage);
        auto fbMemType = mPhysicalDevice->QueryCompatibleMemoryType(
            fbMemReqs.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        auto fbMemAlloc    = vk::MemoryAllocateInfo()
                                 .setAllocationSize(fbMemReqs.size)
                                 .setMemoryTypeIndex(fbMemType);
        auto fbImageMemory = mDevice->Get().allocateMemory(fbMemAlloc);
        mDevice->Get().bindImageMemory(fbImage, fbImageMemory, 0);

        // 4. Temporary command pool + buffer (graphics queue)
        const auto gfxFamily =
            mDevice->GetQueueFamilyIndices().graphicsFamily.value();
        auto fbPoolInfo =
            vk::CommandPoolCreateInfo().setQueueFamilyIndex(gfxFamily).setFlags(
                vk::CommandPoolCreateFlagBits::eTransient);
        auto fbCmdPool = mDevice->Get().createCommandPool(fbPoolInfo);

        auto fbCmdAlloc =
            vk::CommandBufferAllocateInfo()
                .setCommandPool(fbCmdPool)
                .setLevel(vk::CommandBufferLevel::ePrimary)
                .setCommandBufferCount(1);
        auto fbCmdBufs = mDevice->Get().allocateCommandBuffers(fbCmdAlloc);
        auto fbCmdBuf  = fbCmdBufs[0];

        fbCmdBuf.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        // Undefined → TransferDst
        auto fbBar1 =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setImage(fbImage)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1))
                .setSrcAccessMask({})
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        fbCmdBuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), 0,
            nullptr, 0, nullptr, 1, &fbBar1);

        // Copy staging → image
        auto fbCopy =
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
        fbCmdBuf.copyBufferToImage(
            fbStaging->Get(), fbImage, vk::ImageLayout::eTransferDstOptimal,
            fbCopy);

        // TransferDst → ShaderReadOnly
        auto fbBar2 =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
                .setImage(fbImage)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1))
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        fbCmdBuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(),
            0, nullptr, 0, nullptr, 1, &fbBar2);

        fbCmdBuf.end();

        auto fbSubmit =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                &fbCmdBuf);
        mDevice->GetGraphicsQueue().submit(fbSubmit);
        mDevice->GetGraphicsQueue().waitIdle();

        mDevice->Get().destroyCommandPool(fbCmdPool);
        fbStaging.reset();

        // 5. Image view
        auto fbViewInfo =
            vk::ImageViewCreateInfo()
                .setImage(fbImage)
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
        auto fbImageView = mDevice->Get().createImageView(fbViewInfo);

        // 6. Sampler
        auto fbSamplerInfo =
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
        auto fbSampler = mDevice->Get().createSampler(fbSamplerInfo);

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

        const auto fbWrites = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbImgInfoDesc),
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbImgInfoDesc),
            vk::WriteDescriptorSet()
                .setDstSet(fbFallbackSet)
                .setDstBinding(2)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(fbImgInfoDesc),
        };
        mDevice->Get().updateDescriptorSets(fbWrites, nullptr);

        return skr::MakeRef<RenderPass>(
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
            fbImage,
            fbImageMemory,
            fbImageView,
            fbSampler);
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
