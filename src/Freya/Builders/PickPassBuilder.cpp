#include "Freya/Builders/PickPassBuilder.hpp"

#include "Freya/Internal/VertexInput.hpp"

#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Asset/Vertex.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace FREYA_NAMESPACE
{
    skr::Arc<PickPass> PickPassBuilder::Build(const vk::Extent2D extent)
    {
        const auto depthFormat = mPhysicalDevice->GetDepthFormat();
        const auto width       = std::max(1u, extent.width);
        const auto height      = std::max(1u, extent.height);

        auto renderPass = createRenderPass(depthFormat);

        const auto uboBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex);

        const auto layoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(uboBinding);

        auto descriptorSetLayout =
            mDevice->Get().createDescriptorSetLayout(layoutInfo);

        const auto poolSize = vk::DescriptorPoolSize()
                                  .setType(vk::DescriptorType::eUniformBuffer)
                                  .setDescriptorCount(1);

        const auto poolInfo =
            vk::DescriptorPoolCreateInfo().setPoolSizes(poolSize).setMaxSets(1);

        auto descriptorPool = mDevice->Get().createDescriptorPool(poolInfo);

        const auto allocInfo = vk::DescriptorSetAllocateInfo()
                                   .setDescriptorPool(descriptorPool)
                                   .setSetLayouts(descriptorSetLayout);

        auto descriptorSet =
            mDevice->Get().allocateDescriptorSets(allocInfo).front();

        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(ProjectionUniformBuffer))
                .Build();

        const auto bufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(uniformBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(ProjectionUniformBuffer));

        const auto write =
            vk::WriteDescriptorSet()
                .setDstSet(descriptorSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setBufferInfo(bufferInfo);

        mDevice->Get().updateDescriptorSets(write, nullptr);

        auto stagingBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Readback)
                .SetSize(sizeof(std::uint32_t))
                .Build();

        auto vertShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(mFreyaOptions->shaderRoot + "/Pick/pick.vert.spv")
                .Build();

        auto fragShader =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(mFreyaOptions->shaderRoot + "/Pick/pick.frag.spv")
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

        auto vertexBinding = GetVertexBindingDescription();
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
                // materialId + entityId + flags + boneOffset (uvec4)
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

        // No blending for integer ID attachment.
        auto colorBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA);

        auto colorBlending = vk::PipelineColorBlendStateCreateInfo()
                                 .setLogicOpEnable(false)
                                 .setAttachments(colorBlendAttachment);

        auto dynamicStates = std::vector { vk::DynamicState::eViewport,
                                           vk::DynamicState::eScissor };

        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        auto pushConstantRange =
            vk::PushConstantRange()
                .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                .setOffset(0)
                .setSize(sizeof(std::uint32_t));

        auto setLayouts = std::array {
            descriptorSetLayout,
            mBoneResources->GetLayout(),
        };
        auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayouts)
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
                .setPColorBlendState(&colorBlending)
                .setPDynamicState(&dynamicState)
                .setLayout(pipelineLayout)
                .setRenderPass(renderPass)
                .setSubpass(0)
                .setBasePipelineHandle(nullptr);

        auto pipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, pipelineInfo).value;

        mDevice->Get().destroyShaderModule(vertShader->Get());
        mDevice->Get().destroyShaderModule(fragShader->Get());

        auto colorImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR32Uint)
                .SetWidth(width)
                .SetHeight(height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        auto depthImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Depth)
                .SetFormat(depthFormat)
                .SetWidth(width)
                .SetHeight(height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        const std::array attachments = { colorImage->GetImageView(),
                                         depthImage->GetImageView() };

        const auto fbInfo =
            vk::FramebufferCreateInfo()
                .setRenderPass(renderPass)
                .setAttachments(attachments)
                .setWidth(width)
                .setHeight(height)
                .setLayers(1);

        auto framebuffer = mDevice->Get().createFramebuffer(fbInfo);

        return skr::MakeArc<PickPass>(
            mDevice,
            mPhysicalDevice,
            mFreyaOptions,
            mBoneResources,
            renderPass,
            pipelineLayout,
            pipeline,
            descriptorSetLayout,
            descriptorPool,
            descriptorSet,
            uniformBuffer,
            stagingBuffer,
            colorImage,
            depthImage,
            framebuffer,
            vk::Extent2D { width, height });
    }

    vk::RenderPass PickPassBuilder::createRenderPass(
        const vk::Format depthFormat) const
    {
        const auto colorAttachment =
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR32Uint)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);

        const auto depthAttachment =
            vk::AttachmentDescription()
                .setFormat(depthFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(
                    vk::ImageLayout::eDepthStencilAttachmentOptimal);

        const auto colorRef =
            vk::AttachmentReference().setAttachment(0).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal);

        const auto depthRef =
            vk::AttachmentReference().setAttachment(1).setLayout(
                vk::ImageLayout::eDepthStencilAttachmentOptimal);

        const auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef)
                .setPDepthStencilAttachment(&depthRef);

        const auto dependencies = std::array {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eTransfer |
                                 vk::PipelineStageFlagBits::eFragmentShader)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eTransferRead |
                                  vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eTransfer)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
        };

        const auto attachments =
            std::array { colorAttachment, depthAttachment };

        const auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(dependencies);

        return mDevice->Get().createRenderPass(renderPassInfo);
    }

} // namespace FREYA_NAMESPACE
