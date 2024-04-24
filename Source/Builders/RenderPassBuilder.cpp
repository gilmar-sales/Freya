#include "Builders/RenderPassBuilder.hpp"

#include "Builders/BufferBuilder.hpp"
#include "Builders/ShaderModuleBuilder.hpp"

#include "Asset/Vertex.hpp"
#include "Core/Device.hpp"
#include "Core/PhysicalDevice.hpp"
#include "Core/ShaderModule.hpp"
#include "Core/Surface.hpp"
#include "Core/UniformBuffer.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace FREYA_NAMESPACE
{

    std::shared_ptr<RenderPass> RenderPassBuilder::Build()
    {
        auto format = mSurface->QuerySurfaceFormat().format;
        std::cout << "Color attachment format: " << vk::to_string(format) << "\n";

        auto colorAttachment =
            vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        if (mSamples != vk::SampleCountFlagBits::e1)
            colorAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto colorAttachmentRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto depthAttachment =
            vk::AttachmentDescription()
                .setFormat(getDepthFormat())
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStencilStoreOp(vk::AttachmentStoreOp::eStore)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                .setFlags(vk::AttachmentDescriptionFlagBits::eMayAlias);

        auto depthAttachmentRef =
            vk::AttachmentReference()
                .setAttachment(1)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

        auto colorAttachmentResolveRef =
            vk::AttachmentReference().setAttachment(2).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachmentCount(1)
                .setPColorAttachments(&colorAttachmentRef)
                .setPDepthStencilAttachment(&depthAttachmentRef);

        if (mSamples != vk::SampleCountFlagBits::e1)
        {
            subpass.setPResolveAttachments(&colorAttachmentResolveRef);
        }

        auto dependency =
            vk::SubpassDependency()
                .setSrcSubpass(~0u)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                 vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                 vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                                  vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        auto attachments =
            mSamples != vk::SampleCountFlagBits::e1
                ? std::vector<vk::AttachmentDescription> { colorAttachment,
                                                           depthAttachment,
                                                           colorAttachmentResolve }
                : std::vector<vk::AttachmentDescription> {
                      colorAttachment, depthAttachment
                  };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(dependency);

        auto renderPass = mDevice->Get().createRenderPass(renderPassInfo);

        auto vertShaderModule =
            ShaderModuleBuilder()
                .SetDevice(mDevice)
                .SetFilePath("./Shaders/Vert.spv")
                .Build();

        auto fragShaderModule =
            ShaderModuleBuilder()
                .SetDevice(mDevice)
                .SetFilePath("./Shaders/Frag.spv")
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

        auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo()
                                 .setTopology(vk::PrimitiveTopology::eTriangleList)
                                 .setPrimitiveRestartEnable(false);

        auto viewportState =
            vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);

        auto rasterizer =
            vk::PipelineRasterizationStateCreateInfo()
                .setDepthClampEnable(true)
                .setRasterizerDiscardEnable(false)
                .setPolygonMode(vk::PolygonMode::eFill)
                .setCullMode(vk::CullModeFlagBits::eBack)
                .setFrontFace(vk::FrontFace::eClockwise)
                .setLineWidth(1.0f)
                .setDepthBiasEnable(false);

        auto colorBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(
                    vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                    vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false);

        auto colorBlending =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setAttachmentCount(1)
                .setPAttachments(&colorBlendAttachment)
                .setBlendConstants({ 0, 0, 0, 0 });

        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport, vk::DynamicState::eScissor
        };

        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamicStates);

        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(mFrameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(mFrameCount);

        auto descriptorPool = mDevice->Get().createDescriptorPool(poolInfo);
        auto uboLayoutBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setPImmutableSamplers(nullptr);

        auto descriptorSetCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindingCount(1).setPBindings(
                &uboLayoutBinding);

        auto layouts = std::vector<vk::DescriptorSetLayout> {};
        for (auto i = 0; i < mFrameCount; i++)
        {
            layouts.push_back(
                mDevice->Get().createDescriptorSetLayout(descriptorSetCreateInfo));
        }

        auto descriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo().setSetLayouts(layouts).setDescriptorPool(
                descriptorPool);

        auto descriptorSets =
            mDevice->Get().allocateDescriptorSets(descriptorSetAllocInfo);

        auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
                                      .setSetLayouts(layouts);

        auto uniformBuffers = std::vector<std::shared_ptr<Buffer>>();

        for (auto i = 0; i < mFrameCount; i++)
        {
            auto uniformBuffer =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Uniform)
                    .SetSize(sizeof(ProjectionUniformBuffer))
                    .Build();

            uniformBuffers.push_back(uniformBuffer);

            auto bufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(uniformBuffer->Get())
                    .setOffset(0)
                    .setRange(sizeof(ProjectionUniformBuffer));

            auto descriptorWriter =
                vk::WriteDescriptorSet()
                    .setDstSet(descriptorSets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufferInfo);

            mDevice->Get().updateDescriptorSets(1, &descriptorWriter, 0, nullptr);
        }

        auto pipelineLayout = mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        assert(pipelineLayout && "Failed to create pipeline layout.");

        auto stencilOpState        = vk::StencilOpState();
        stencilOpState.failOp      = vk::StencilOp::eKeep;
        stencilOpState.passOp      = vk::StencilOp::eKeep;
        stencilOpState.compareOp   = vk::CompareOp::eAlways;
        stencilOpState.depthFailOp = vk::StencilOp::eKeep;

        auto depthStencilInfo =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setStencilTestEnable(false)
                .setDepthBoundsTestEnable(false)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setBack(stencilOpState)
                .setFront(stencilOpState)
                .setMinDepthBounds(0.0f)
                .setMaxDepthBounds(1.0f)
                .setFront({})
                .setBack({});

        auto multisamplingInfo =
            vk::PipelineMultisampleStateCreateInfo()
                .setSampleShadingEnable(0)
                .setRasterizationSamples(mSamples);

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

        assert(graphicsPipeline && "Failed to create graphics pipeline.");

        mDevice->Get().destroyShaderModule(vertShaderModule->Get());
        mDevice->Get().destroyShaderModule(fragShaderModule->Get());

        return std::make_shared<RenderPass>(
            mDevice,
            mSurface,
            renderPass,
            pipelineLayout,
            graphicsPipeline,
            uniformBuffers,
            layouts,
            descriptorSets,
            descriptorPool);
    }

    vk::Format RenderPassBuilder::getDepthFormat()
    {
        auto candidates = std::vector<vk::Format> { vk::Format::eD32Sfloat,
                                                    vk::Format::eD32SfloatS8Uint,
                                                    vk::Format::eD24UnormS8Uint };

        auto depthFeature = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
        for (auto& format : candidates)
        {
            auto props = mPhysicalDevice->Get().getFormatProperties(format);

            if ((props.optimalTilingFeatures & depthFeature) == depthFeature)
            {
                return format;
            }
        }

        return vk::Format();
    }

} // namespace FREYA_NAMESPACE
