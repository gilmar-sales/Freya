#include "Freya/Builders/ForwardPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/UniformBuffer.hpp"

namespace FREYA_NAMESPACE
{
    Ref<ForwardPass> ForwardPassBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::ForwardPass':");

        auto renderPass = createRenderPass();

        auto vertShaderModule =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Forward/Vert.spv")
                .Build();

        auto fragShaderModule =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Forward/Frag.spv")
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
                .setFrontFace(vk::FrontFace::eClockwise)
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

        auto descriptorSetCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(
                descriptorSetBindings);

        auto layouts = std::vector<vk::DescriptorSetLayout> {};
        for (auto i = 0; i < mFreyaOptions->frameCount; i++)
        {
            layouts.push_back(mDevice->Get().createDescriptorSetLayout(
                descriptorSetCreateInfo));
        }

        auto descriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(layouts)
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

        layouts.push_back(samplerLayout);

        const auto pipelineLayouts = std::array { layouts[0], samplerLayout };

        auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo().setSetLayouts(pipelineLayouts);

        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(std::max(
                    sizeof(ProjectionUniformBuffer) * mFreyaOptions->frameCount,
                    1024uz * 1024uz))
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

            mDevice->Get()
                .updateDescriptorSets(1, &descriptorWriter, 0, nullptr);
        }

        auto pipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        mLogger->Assert(pipelineLayout, "\tFailed to create pipeline layout.");

        auto depthStencilInfo =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto multisamplingInfo =
            vk::PipelineMultisampleStateCreateInfo()
                .setSampleShadingEnable(false)
                .setRasterizationSamples(mFreyaOptions->sampleCount);

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

        return skr::MakeRef<ForwardPass>(
            mDevice,
            mSurface,
            renderPass,
            pipelineLayout,
            graphicsPipeline,
            uniformBuffer,
            layouts,
            descriptorSets,
            descriptorPool,
            samplerLayout,
            samplerDescriptorPool);
    }
} // namespace FREYA_NAMESPACE

vk::RenderPass FREYA_NAMESPACE::ForwardPassBuilder::createRenderPass() const
{
    auto format = mSurface->QuerySurfaceFormat().format;

    mLogger->LogTrace("\tColor Format: {}", vk::to_string(format));

    auto colorAttachment =
        vk::AttachmentDescription()
            .setFormat(format)
            .setSamples(mFreyaOptions->sampleCount)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    if (mFreyaOptions->sampleCount != vk::SampleCountFlagBits::e1)
    {

        colorAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare);
    }

    auto depthAttachment =
        vk::AttachmentDescription()
            .setFormat(mPhysicalDevice->GetDepthFormat())
            .setSamples(mFreyaOptions->sampleCount)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

    auto colorAttachmentRef =
        vk::AttachmentReference()
            .setAttachment(ForwardColorAttachment)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    auto depthAttachmentRef =
        vk::AttachmentReference()
            .setAttachment(ForwardDepthAttachment)
            .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    auto colorAttachmentResolveRef =
        vk::AttachmentReference()
            .setAttachment(ForwardColorResolveAttachment)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    mLogger->LogTrace("\tDepth Format: {}", to_string(depthAttachment.format));

    auto subpass = vk::SubpassDescription()
                       .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                       .setColorAttachments(colorAttachmentRef)
                       .setPDepthStencilAttachment(&depthAttachmentRef);

    if (mFreyaOptions->sampleCount != vk::SampleCountFlagBits::e1)
    {
        subpass.setPResolveAttachments(&colorAttachmentResolveRef);
    }

    auto dependencies = std::vector {
        vk::SubpassDependency()
            .setSrcSubpass(vk::SubpassExternal)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                             vk::PipelineStageFlagBits::eEarlyFragmentTests)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                             vk::PipelineStageFlagBits::eEarlyFragmentTests)
            .setSrcAccessMask(vk::AccessFlagBits::eNone)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                              vk::AccessFlagBits::eDepthStencilAttachmentWrite)
    };

    auto attachments =
        mFreyaOptions->sampleCount == vk::SampleCountFlagBits::e1
            ? std::vector { colorAttachment, depthAttachment }
            : std::vector { colorAttachment, depthAttachment,
                            colorAttachmentResolve };

    auto renderPassInfo =
        vk::RenderPassCreateInfo()
            .setAttachments(attachments)
            .setSubpasses(subpass)
            .setDependencies(dependencies);

    auto renderPass = mDevice->Get().createRenderPass(renderPassInfo);

    return renderPass;
}
