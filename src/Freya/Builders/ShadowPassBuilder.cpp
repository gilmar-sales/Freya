#include "Freya/Builders/ShadowPassBuilder.hpp"

#include "Freya/Asset/InstanceTransform.hpp"
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
        // Depth-only pipeline (set 0 = bone SSBO + push constant light VP)
        // ------------------------------------------------------------------
        auto vertShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(
                    mFreyaOptions->shaderRoot + "/Shadow/depth.vert.spv")
                .Build();

        auto fragShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(
                    mFreyaOptions->shaderRoot + "/Shadow/depth.frag.spv")
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

        // Depth.vert: position, instance model, boneOffset (loc13),
        // joints/weights. Stride still covers prevModel; unused attrs
        // are omitted to avoid validation warnings.
        auto vertexBinding = Vertex::GetBindingDescription();
        auto vertexAttributes =
            std::vector<vk::VertexInputAttributeDescription> {
                vk::VertexInputAttributeDescription()
                    .setBinding(0)
                    .setLocation(0)
                    .setFormat(vk::Format::eR32G32B32Sfloat)
                    .setOffset(offsetof(Vertex, position)),
                vk::VertexInputAttributeDescription()
                    .setBinding(0)
                    .setLocation(14)
                    .setFormat(vk::Format::eR32G32B32A32Uint)
                    .setOffset(offsetof(Vertex, joints)),
                vk::VertexInputAttributeDescription()
                    .setBinding(0)
                    .setLocation(15)
                    .setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(offsetof(Vertex, weights)),
                vk::VertexInputAttributeDescription()
                    .setBinding(1)
                    .setLocation(5)
                    .setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(0),
                vk::VertexInputAttributeDescription()
                    .setBinding(1)
                    .setLocation(6)
                    .setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(sizeof(glm::vec4)),
                vk::VertexInputAttributeDescription()
                    .setBinding(1)
                    .setLocation(7)
                    .setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(sizeof(glm::vec4) * 2),
                vk::VertexInputAttributeDescription()
                    .setBinding(1)
                    .setLocation(8)
                    .setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(sizeof(glm::vec4) * 3),
                vk::VertexInputAttributeDescription()
                    .setBinding(1)
                    .setLocation(13)
                    .setFormat(vk::Format::eR32G32B32A32Uint)
                    .setOffset(offsetof(InstanceTransform, materialId)),
            };

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
                // Front-face cull stores back-face depths (AAA default):
                // reduces acne and keeps hollow/inner shells out of the map.
                .setCullMode(vk::CullModeFlagBits::eFront)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f)
                .setDepthBiasEnable(true)
                .setDepthBiasConstantFactor(
                    mFreyaOptions->ReverseZ ? -2.25f : 2.25f)
                .setDepthBiasClamp(0.0f)
                .setDepthBiasSlopeFactor(
                    mFreyaOptions->ReverseZ ? -2.75f : 2.75f);

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
                .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                               vk::ShaderStageFlagBits::eFragment)
                .setOffset(0)
                .setSize(sizeof(ShadowPushConstant));

        const auto boneLayout = mBoneResources->GetLayout();
        auto       pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(boneLayout)
                .setPushConstantRanges(pushConstantRange);

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

        // Zero slots still need a sampled view for lighting descriptors.
        // Use a 1×1 stub so Low/zero configs avoid full-resolution VRAM.
        const auto spotLayers      = maxSpot == 0 ? 1u : maxSpot;
        const auto spotResolution  = maxSpot == 0 ? 1u : resolution;
        const auto pointLayers     = maxPoint == 0 ? 6u : maxPoint * 6;
        const auto pointResolution = maxPoint == 0 ? 1u : resolution;

        auto spot = createArrayImage(
            depthFormat,
            spotResolution,
            spotLayers,
            false,
            vk::ImageViewType::e2DArray);

        auto point = createArrayImage(
            depthFormat,
            pointResolution,
            pointLayers,
            true,
            vk::ImageViewType::eCubeArray);

        auto cascadeFramebuffers =
            createFramebuffers(renderPass, cascade.layerViews, resolution);
        auto spotFramebuffers =
            createFramebuffers(renderPass, spot.layerViews, spotResolution);
        auto pointFramebuffers =
            createFramebuffers(renderPass, point.layerViews, pointResolution);

        // ------------------------------------------------------------------
        // Shadow uniform buffer (ring-buffered per in-flight frame)
        // ------------------------------------------------------------------
        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(ShadowUniformBuffer) *
                         mFreyaOptions->frameCount)
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
                // Reverse-Z clear/far is 0; white (1) makes out-of-bounds PCF
                // fail Greater and look like grain at cascade UV edges.
                .setBorderColor(mFreyaOptions->ReverseZ
                                    ? vk::BorderColor::eFloatOpaqueBlack
                                    : vk::BorderColor::eFloatOpaqueWhite)
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

        return skr::MakeArc<ShadowPass>(
            mDevice,
            mPhysicalDevice,
            mFreyaOptions,
            mBoneResources,
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
