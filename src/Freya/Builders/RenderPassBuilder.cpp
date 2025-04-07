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

        auto shaderStages = std::vector<vk::PipelineShaderStageCreateInfo>();

        auto shaderModules = std::vector<Ref<ShaderModule>>();

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            auto vertShaderModule =
                mServiceProvider->GetService<ShaderModuleBuilder>()
                    ->SetFilePath(
                        "./Resources/Shaders/Deferred/GBuffer.vert.spv")
                    .Build();

            auto fragShaderModule =
                mServiceProvider->GetService<ShaderModuleBuilder>()
                    ->SetFilePath(
                        "./Resources/Shaders/Deferred/GBuffer.frag.spv")
                    .Build();

            shaderModules.push_back(vertShaderModule);
            shaderModules.push_back(fragShaderModule);

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

            shaderStages.push_back(vertShaderStageInfo);
            shaderStages.push_back(fragShaderStageInfo);
        }
        else
        {
            auto vertShaderModule =
                mServiceProvider->GetService<ShaderModuleBuilder>()
                    ->SetFilePath("./Resources/Shaders/Forward/Vert.spv")
                    .Build();

            auto fragShaderModule =
                mServiceProvider->GetService<ShaderModuleBuilder>()
                    ->SetFilePath("./Resources/Shaders/Forward/Frag.spv")
                    .Build();

            shaderModules.push_back(vertShaderModule);
            shaderModules.push_back(fragShaderModule);

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

            shaderStages.push_back(vertShaderStageInfo);
            shaderStages.push_back(fragShaderStageInfo);
        }

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

        auto pipelineBlendAttachments = std::vector { colorBlendAttachment };

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            pipelineBlendAttachments.push_back(
                vk::PipelineColorBlendAttachmentState()
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                       vk::ColorComponentFlagBits::eG |
                                       vk::ColorComponentFlagBits::eB |
                                       vk::ColorComponentFlagBits::eA)
                    .setBlendEnable(false));

            pipelineBlendAttachments.push_back(
                vk::PipelineColorBlendAttachmentState()
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                       vk::ColorComponentFlagBits::eG |
                                       vk::ColorComponentFlagBits::eB |
                                       vk::ColorComponentFlagBits::eA)
                    .setBlendEnable(false));

            pipelineBlendAttachments.push_back(
                vk::PipelineColorBlendAttachmentState()
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                       vk::ColorComponentFlagBits::eG |
                                       vk::ColorComponentFlagBits::eB |
                                       vk::ColorComponentFlagBits::eA)
                    .setBlendEnable(false));
        }

        auto colorBlending =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                .setAttachments(pipelineBlendAttachments);

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

        unsigned long long minimumBufferSize = 1024 * 1024;

        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(std::max(
                    sizeof(ProjectionUniformBuffer) * mFreyaOptions->frameCount,
                    minimumBufferSize))
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
                .setDepthCompareOp(vk::CompareOp::eLessOrEqual)
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

        for (auto& shaderModule : shaderModules)
        {
            mDevice->Get().destroyShaderModule(shaderModule->Get());
        }

        shaderStages.clear();
        shaderModules.clear();

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            auto vertShaderModule =
                mServiceProvider->GetService<ShaderModuleBuilder>()
                    ->SetFilePath(
                        "./Resources/Shaders/Deferred/Composition.vert.spv")
                    .Build();

            auto fragShaderModule =
                mServiceProvider->GetService<ShaderModuleBuilder>()
                    ->SetFilePath(
                        "./Resources/Shaders/Deferred/Composition.frag.spv")
                    .Build();

            shaderModules.push_back(vertShaderModule);
            shaderModules.push_back(fragShaderModule);

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

            shaderStages.push_back(vertShaderStageInfo);
            shaderStages.push_back(fragShaderStageInfo);

            pipelineInfo.setStages(shaderStages).setSubpass(1);
        }

        auto compositionPoolSize =
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eInputAttachment)
                .setDescriptorCount(2 << 16);

        auto compositionPoolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(1)
                .setPPoolSizes(&compositionPoolSize)
                .setMaxSets(2 << 16);

        auto compositionDescriptorPool =
            mDevice->Get().createDescriptorPool(compositionPoolInfo);

        auto compositionDescriptorSetBindings = std::array {
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
                .setPImmutableSamplers(nullptr)
        };

        auto compositionDescriptorSetCreateInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(
                compositionDescriptorSetBindings);

        auto compositionLayout = mDevice->Get().createDescriptorSetLayout(
            compositionDescriptorSetCreateInfo);

        const auto compositionPipelineLayouts =
            std::array { compositionLayout };

        auto compositionPipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo().setSetLayouts(
                compositionPipelineLayouts);

        auto compositionPipelineLayout =
            mDevice->Get().createPipelineLayout(compositionPipelineLayoutInfo);

        auto emptyInputState = vk::PipelineVertexInputStateCreateInfo();

        auto blendStates = std::vector {
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false)
        };

        colorBlending.setAttachments(blendStates);

        pipelineInfo.setLayout(compositionPipelineLayout)
            .setPVertexInputState(&emptyInputState);

        depthStencilInfo.setDepthWriteEnable(false);

        auto compositionPipeline =
            mDevice->Get().createGraphicsPipeline(nullptr, pipelineInfo).value;

        mLogger->Assert(compositionPipeline,
                        "\tFailed to create the composition pipeline.");

        auto compositionDescriptorSetAllocInfo =
            vk::DescriptorSetAllocateInfo()
                .setSetLayouts(compositionPipelineLayouts)
                .setDescriptorPool(compositionDescriptorPool);

        auto compositionDescriptorSets = mDevice->Get().allocateDescriptorSets(
            compositionDescriptorSetAllocInfo);

        for (auto& shaderModule : shaderModules)
        {
            mDevice->Get().destroyShaderModule(shaderModule->Get());
        }

        return skr::MakeRef<RenderPass>(
            mDevice,
            mFreyaOptions,
            renderPass,
            pipelineLayout,
            graphicsPipeline,
            compositionPipelineLayout,
            compositionPipeline,
            compositionDescriptorSets,
            uniformBuffer,
            frameLayouts,
            descriptorSets,
            descriptorPool,
            samplerLayout,
            samplerDescriptorPool);
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

        auto mainPassColorAttachments = std::vector { colorAttachmentRef };

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            mainPassColorAttachments.push_back(
                vk::AttachmentReference().setAttachment(2).setLayout(
                    vk::ImageLayout::eColorAttachmentOptimal));

            mainPassColorAttachments.push_back(
                vk::AttachmentReference().setAttachment(3).setLayout(
                    vk::ImageLayout::eColorAttachmentOptimal));

            mainPassColorAttachments.push_back(
                vk::AttachmentReference().setAttachment(4).setLayout(
                    vk::ImageLayout::eColorAttachmentOptimal));
        }

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
                .setColorAttachments(mainPassColorAttachments)
                .setPDepthStencilAttachment(&depthAttachmentRef);

        if (vkSampleCount != vk::SampleCountFlagBits::e1)
        {
            mainPass.setPResolveAttachments(&colorAttachmentResolveRef);
        }

        auto subpasses = std::vector { mainPass };

        auto gBufferReferences =
            std::vector { vk::AttachmentReference().setAttachment(2).setLayout(
                              vk::ImageLayout::eShaderReadOnlyOptimal),
                          vk::AttachmentReference().setAttachment(3).setLayout(
                              vk::ImageLayout::eShaderReadOnlyOptimal),
                          vk::AttachmentReference().setAttachment(4).setLayout(
                              vk::ImageLayout::eShaderReadOnlyOptimal) };

        auto compositionPass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorAttachmentRef)
                .setPDepthStencilAttachment(&depthAttachmentRef)
                .setInputAttachments(gBufferReferences);

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            subpasses.push_back(compositionPass);
        }

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

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            auto albedoAttachment =
                vk::AttachmentDescription()
                    .setFormat(vk::Format::eR8G8B8A8Unorm)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setLoadOp(vk::AttachmentLoadOp::eClear)
                    .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setInitialLayout(vk::ImageLayout::eUndefined)
                    .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

            auto normalAttachment =
                vk::AttachmentDescription()
                    .setFormat(vk::Format::eR16G16B16A16Sfloat)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setLoadOp(vk::AttachmentLoadOp::eClear)
                    .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setInitialLayout(vk::ImageLayout::eUndefined)
                    .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

            auto positionAttachment =
                vk::AttachmentDescription()
                    .setFormat(vk::Format::eR16G16B16A16Sfloat)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setLoadOp(vk::AttachmentLoadOp::eClear)
                    .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                    .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                    .setInitialLayout(vk::ImageLayout::eUndefined)
                    .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

            attachments.push_back(albedoAttachment);
            attachments.push_back(normalAttachment);
            attachments.push_back(positionAttachment);
        }

        return attachments;
    }

    std::vector<vk::SubpassDependency> RenderPassBuilder::createDependencies()
        const
    {
        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Forward)
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

        return std::vector {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(1)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),

        };
    }

} // namespace FREYA_NAMESPACE
