#include "Builders/ForwardPassBuilder.hpp"

#include "Builders/BufferBuilder.hpp"
#include "Builders/ShaderModuleBuilder.hpp"

#include "Asset/Vertex.hpp"
#include "Core/Device.hpp"
#include "Core/PhysicalDevice.hpp"
#include "Core/ShaderModule.hpp"
#include "Core/Surface.hpp"
#include "Core/UniformBuffer.hpp"

#include <glm/ext/matrix_transform.hpp>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace FREYA_NAMESPACE
{

    Ref<ForwardPass> ForwardPassBuilder::Build()
    {
        auto format = mSurface->QuerySurfaceFormat().format;

        auto colorAttachment =
            vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        std::println("Color Format: {}", vk::to_string(colorAttachment.format));

        if (mSamples != vk::SampleCountFlagBits::e1)
            colorAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto colorAttachmentRef = vk::AttachmentReference()
                                      .setAttachment(ForwardColorAttachment)
                                      .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto colorAttachmentResolve =
            vk::AttachmentDescription()
                .setFormat(format)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        auto colorAttachmentResolveRef =
            vk::AttachmentReference()
                .setAttachment(ForwardColorResolveAttachment)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        auto depthAttachment =
            vk::AttachmentDescription()
                .setFormat(mPhysicalDevice->GetDepthFormat())
                .setSamples(mSamples)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

        if (mSamples != vk::SampleCountFlagBits::e1)
            depthAttachment.setFinalLayout(vk::ImageLayout::eDepthAttachmentStencilReadOnlyOptimal);

        std::println("Depth Format: {}", vk::to_string(depthAttachment.format));

        auto depthAttachmentRef =
            vk::AttachmentReference()
                .setAttachment(ForwardDepthAttachment)
                .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorAttachmentRef)
                .setPDepthStencilAttachment(&depthAttachmentRef);

        if (mSamples != vk::SampleCountFlagBits::e1)
        {
            subpass.setPResolveAttachments(&colorAttachmentResolveRef);
        }

        auto dependencies = std::vector {
            vk::SubpassDependency()
                .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(VK_SUBPASS_EXTERNAL)
                .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDependencyFlags(vk::DependencyFlagBits::eByRegion)
        };

        auto attachments =
            mSamples == vk::SampleCountFlagBits::e1
                ? std::vector { colorAttachment,
                                depthAttachment }
                : std::vector {
                      colorAttachment,
                      depthAttachment,
                      colorAttachmentResolve
                  };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(dependencies);

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
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                .setAlphaBlendOp(vk::BlendOp::eAdd);

        auto colorBlending =
            vk::PipelineColorBlendStateCreateInfo()
                .setAttachmentCount(1)
                .setPAttachments(&colorBlendAttachment);

        auto dynamicStates = std::vector {
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
                .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr);

        auto descriptorSetBindings = std::array {
            uboLayoutBinding,
        };

        auto descriptorSetCreateInfo = vk::DescriptorSetLayoutCreateInfo()
                                           .setBindings(descriptorSetBindings);

        auto layouts = std::vector<vk::DescriptorSetLayout> {};
        for (auto i = 0; i < mFrameCount; i++)
        {
            layouts.push_back(
                mDevice->Get().createDescriptorSetLayout(descriptorSetCreateInfo));
        }

        auto descriptorSetAllocInfo = vk::DescriptorSetAllocateInfo()
                                          .setSetLayouts(layouts)
                                          .setDescriptorPool(descriptorPool);

        auto descriptorSets =
            mDevice->Get().allocateDescriptorSets(descriptorSetAllocInfo);

        auto samplerPoolSize = vk::DescriptorPoolSize()
                                   .setType(vk::DescriptorType::eCombinedImageSampler)
                                   .setDescriptorCount(2 << 16);

        auto samplerPoolInfo = vk::DescriptorPoolCreateInfo()
                                   .setPoolSizeCount(1)
                                   .setPPoolSizes(&samplerPoolSize)
                                   .setMaxSets(2 << 16);

        auto samplerDescriptorPool = mDevice->Get().createDescriptorPool(samplerPoolInfo);

        auto samplerDescriptorSetBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setPImmutableSamplers(nullptr)
        };

        auto samplerDescriptorSetCreateInfo = vk::DescriptorSetLayoutCreateInfo()
                                                  .setBindings(samplerDescriptorSetBindings);

        auto samplerLayout = mDevice->Get().createDescriptorSetLayout(samplerDescriptorSetCreateInfo);

        layouts.push_back(samplerLayout);

        const auto pipelineLayouts = std::array { layouts[0], samplerLayout };

        auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
                                      .setSetLayouts(pipelineLayouts);

        auto uniformBuffers = std::vector<Ref<Buffer>>();

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

        auto stencilOpState = vk::StencilOpState()
                                  .setFailOp(vk::StencilOp::eKeep)
                                  .setPassOp(vk::StencilOp::eKeep)
                                  .setCompareOp(vk::CompareOp::eAlways);

        auto depthStencilInfo =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(false)
                .setMinDepthBounds(0.0f)
                .setMaxDepthBounds(1.0f)
                .setStencilTestEnable(false)
                .setBack(stencilOpState)
                .setFront(stencilOpState);

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

        constexpr auto samplerCreateInfo = vk::SamplerCreateInfo()
                                               .setMagFilter(vk::Filter::eLinear)
                                               .setMinFilter(vk::Filter::eLinear)
                                               .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                                               .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                                               .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                                               .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
                                               .setUnnormalizedCoordinates(false)
                                               .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                                               .setMipLodBias(0.0f)
                                               .setMinLod(0.0f)
                                               .setMaxLod(0.0f)
                                               .setAnisotropyEnable(true)
                                               .setMaxAnisotropy(16);

        const auto sampler = mDevice->Get().createSampler(samplerCreateInfo);
        return MakeRef<ForwardPass>(
            mDevice,
            mSurface,
            renderPass,
            pipelineLayout,
            graphicsPipeline,
            uniformBuffers,
            layouts,
            descriptorSets,
            descriptorPool,
            samplerLayout,
            samplerDescriptorPool,
            sampler);
    }

} // namespace FREYA_NAMESPACE
