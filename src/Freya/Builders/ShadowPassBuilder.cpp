#include "Freya/Builders/ShadowPassBuilder.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <algorithm>
#include <array>

namespace FREYA_NAMESPACE
{
    skr::Arc<ShadowPass> ShadowPassBuilder::Build()
    {
        const auto depthFormat = mPhysicalDevice->GetDepthFormat();
        const auto resolution  = mFreyaOptions->shadowMapResolution;

        const auto cascadeCount =
            std::clamp(mFreyaOptions->shadowCascadeCount,
                       1u,
                       MAX_SHADOW_CASCADES);
        const auto maxSpot =
            std::clamp(mFreyaOptions->maxSpotShadows, 0u, MAX_SPOT_SHADOWS);
        const auto maxPoint =
            std::clamp(mFreyaOptions->maxPointShadows, 0u, MAX_POINT_SHADOWS);

        auto renderPass = createRenderPass(depthFormat);

        // ------------------------------------------------------------------
        // Depth-only pipeline (push constant mat4, no descriptor sets)
        // ------------------------------------------------------------------
        auto vertShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Shadow/depth.vert.spv")
                .Build();

        auto fragShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Shadow/depth.frag.spv")
                .Build();

        auto stages = std::array {
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(vertShader->Get())
                .setPName("main"),
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(fragShader->Get())
                .setPName("main"),
        };

        auto vertexBinding    = Vertex::GetBindingDescription();
        auto vertexAttributes = Vertex::GetAttributesDescription();

        auto vertexInputInfo =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(vertexBinding)
                .setVertexAttributeDescriptions(vertexAttributes);

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

        auto multisampling =
            vk::PipelineMultisampleStateCreateInfo()
                .setSampleShadingEnable(false)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

        auto depthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto dynamicStates = std::vector { vk::DynamicState::eViewport,
                                           vk::DynamicState::eScissor };

        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        auto pushConstantRange =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setOffset(0)
                .setSize(sizeof(glm::mat4));

        auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo().setPushConstantRanges(
                pushConstantRange);

        auto pipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        auto pipelineInfo =
            vk::GraphicsPipelineCreateInfo()
                .setStages(stages)
                .setPVertexInputState(&vertexInputInfo)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPMultisampleState(&multisampling)
                .setPDepthStencilState(&depthStencil)
                .setPDynamicState(&dynamicState)
                .setLayout(pipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(0)
                .setBasePipelineHandle(nullptr);

        auto pipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, pipelineInfo).value;

        mDevice->Get().destroyShaderModule(vertShader->Get());
        mDevice->Get().destroyShaderModule(fragShader->Get());

        // ------------------------------------------------------------------
        // Depth image arrays (cascades, spot, point cube array)
        // ------------------------------------------------------------------
        auto cascade = createArrayImage(
            depthFormat,
            resolution,
            cascadeCount,
            false,
            vk::ImageViewType::e2DArray);

        auto spot = createArrayImage(
            depthFormat,
            resolution,
            std::max(1u, maxSpot),
            false,
            vk::ImageViewType::e2DArray);

        auto point = createArrayImage(
            depthFormat,
            resolution,
            std::max(6u, maxPoint * 6),
            true,
            vk::ImageViewType::eCubeArray);

        auto cascadeFramebuffers =
            createFramebuffers(renderPass, cascade.layerViews, resolution);
        auto spotFramebuffers =
            createFramebuffers(renderPass, spot.layerViews, resolution);
        auto pointFramebuffers =
            createFramebuffers(renderPass, point.layerViews, resolution);

        // ------------------------------------------------------------------
        // Shadow uniform buffer (single host-visible copy, not ring-buffered)
        // ------------------------------------------------------------------
        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(ShadowUniformBuffer))
                .Build();

        // ------------------------------------------------------------------
        // Samplers: hardware comparison sampler + unused regular sampler
        // ------------------------------------------------------------------
        auto compareSamplerInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
                .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
                .setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
                .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                .setAnisotropyEnable(false)
                .setMaxAnisotropy(1.0f)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(true)
                .setCompareOp(mFreyaOptions->ReverseZ ? vk::CompareOp::eGreater
                                                      : vk::CompareOp::eLess)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setMipLodBias(0.0f);

        auto compareSampler = mDevice->Get().createSampler(compareSamplerInfo);

        auto regularSamplerInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                .setAnisotropyEnable(false)
                .setMaxAnisotropy(1.0f)
                .setUnnormalizedCoordinates(false)
                .setCompareEnable(false)
                .setMinLod(0.0f)
                .setMaxLod(0.0f)
                .setMipLodBias(0.0f);

        auto regularSampler = mDevice->Get().createSampler(regularSamplerInfo);

        return skr::MakeArc<ShadowPass>(
            mDevice,
            mPhysicalDevice,
            mFreyaOptions,
            renderPass,
            pipelineLayout,
            pipeline,
            cascade.image,
            cascade.memory,
            cascade.arrayView,
            cascade.layerViews,
            cascadeFramebuffers,
            spot.image,
            spot.memory,
            spot.arrayView,
            spot.layerViews,
            spotFramebuffers,
            point.image,
            point.memory,
            point.arrayView,
            point.layerViews,
            pointFramebuffers,
            uniformBuffer,
            compareSampler,
            regularSampler,
            cascadeCount,
            maxSpot,
            maxPoint);
    }

    vk::RenderPass ShadowPassBuilder::createRenderPass(
        const vk::Format depthFormat) const
    {
        auto attachment =
            vk::AttachmentDescription()
                .setFormat(depthFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto depthRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setPDepthStencilAttachment(&depthRef);

        auto dependencies = std::vector<vk::SubpassDependency> {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader |
                                 vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(vk::PipelineStageFlagBits::eLateFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachment)
                .setSubpasses(subpass)
                .setDependencies(dependencies);

        return mDevice->Get().createRenderPass(renderPassInfo);
    }

    ShadowPassBuilder::ArrayImage ShadowPassBuilder::createArrayImage(
        const vk::Format        format,
        const std::uint32_t     resolution,
        const std::uint32_t     layerCount,
        const bool              cubeCompatible,
        const vk::ImageViewType arrayViewType) const
    {
        if (layerCount == 0)
            return {};

        auto imageInfo =
            vk::ImageCreateInfo()
                .setImageType(vk::ImageType::e2D)
                .setFormat(format)
                .setExtent(vk::Extent3D(resolution, resolution, 1))
                .setMipLevels(1)
                .setArrayLayers(layerCount)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setTiling(vk::ImageTiling::eOptimal)
                .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment |
                          vk::ImageUsageFlagBits::eSampled)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setInitialLayout(vk::ImageLayout::eUndefined);

        if (cubeCompatible)
            imageInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);

        auto image = mDevice->Get().createImage(imageInfo);

        const auto requirements =
            mDevice->Get().getImageMemoryRequirements(image);

        const auto memoryTypeIndex = mPhysicalDevice->QueryCompatibleMemoryType(
            requirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        const auto memoryInfo = vk::MemoryAllocateInfo()
                                    .setAllocationSize(requirements.size)
                                    .setMemoryTypeIndex(memoryTypeIndex);

        auto memory = mDevice->Get().allocateMemory(memoryInfo);
        mDevice->Get().bindImageMemory(image, memory, 0);

        const auto arrayViewInfo =
            vk::ImageViewCreateInfo()
                .setImage(image)
                .setViewType(arrayViewType)
                .setFormat(format)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(layerCount));

        const auto arrayView = mDevice->Get().createImageView(arrayViewInfo);

        auto layerViews = std::vector<vk::ImageView>(layerCount);
        for (std::uint32_t i = 0; i < layerCount; ++i)
        {
            const auto layerViewInfo =
                vk::ImageViewCreateInfo()
                    .setImage(image)
                    .setViewType(vk::ImageViewType::e2D)
                    .setFormat(format)
                    .setSubresourceRange(
                        vk::ImageSubresourceRange()
                            .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                            .setBaseMipLevel(0)
                            .setLevelCount(1)
                            .setBaseArrayLayer(i)
                            .setLayerCount(1));

            layerViews[i] = mDevice->Get().createImageView(layerViewInfo);
        }

        return ArrayImage { image, memory, arrayView, layerViews };
    }

    std::vector<vk::Framebuffer> ShadowPassBuilder::createFramebuffers(
        const vk::RenderPass              renderPass,
        const std::vector<vk::ImageView>& layerViews,
        const std::uint32_t               resolution) const
    {
        auto framebuffers = std::vector<vk::Framebuffer>(layerViews.size());

        for (std::size_t i = 0; i < layerViews.size(); ++i)
        {
            const auto fbInfo =
                vk::FramebufferCreateInfo()
                    .setRenderPass(renderPass)
                    .setAttachments(layerViews[i])
                    .setWidth(resolution)
                    .setHeight(resolution)
                    .setLayers(1);

            framebuffers[i] = mDevice->Get().createFramebuffer(fbInfo);
        }

        return framebuffers;
    }

} // namespace FREYA_NAMESPACE
