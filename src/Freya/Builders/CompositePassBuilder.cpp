#include "CompositePassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    CompositePassBuilder::CompositePassBuilder(
        const Ref<Device>&               device,
        const Ref<PhysicalDevice>&       physicalDevice,
        const Ref<Surface>&              surface,
        const Ref<FreyaOptions>&         freyaOptions,
        const Ref<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    Ref<CompositePass> CompositePassBuilder::Build(
        const Ref<SwapChain>& swapChain)
    {
        auto renderPass = createRenderPass();

        // ------------------------------------------------------------------
        // Shaders
        // ------------------------------------------------------------------
        auto loadShader = [&](const std::string& path) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(path)
                .Build();
        };

        auto vertShader = loadShader(
            "./Resources/Shaders/DeferredCompressed/composing.vert.spv");
        auto fragShader = loadShader(
            "./Resources/Shaders/DeferredCompressed/composing.frag.spv");

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

        // Bloom result image (full res, used as blit target in Renderer)
        const auto extent = swapChain->GetExtent();
        auto       bloomResultImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        // ------------------------------------------------------------------
        // Descriptor set layout: 3 combined image samplers (opaque,
        // translucent, bloom)
        // ------------------------------------------------------------------
        auto bindings = std::array {
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
        };

        auto layoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings);

        auto descriptorSetLayout =
            mDevice->Get().createDescriptorSetLayout(layoutInfo);

        // ------------------------------------------------------------------
        // Descriptor pool
        // ------------------------------------------------------------------
        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eCombinedImageSampler)
                            .setDescriptorCount(3 * mFreyaOptions->frameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(mFreyaOptions->frameCount);

        auto descriptorPool = mDevice->Get().createDescriptorPool(poolInfo);

        // Allocate descriptor sets (one per frame)
        auto setLayouts =
            std::vector<vk::DescriptorSetLayout>(mFreyaOptions->frameCount,
                                                 descriptorSetLayout);

        auto allocInfo = vk::DescriptorSetAllocateInfo()
                             .setDescriptorPool(descriptorPool)
                             .setSetLayouts(setLayouts);

        auto descriptorSets = mDevice->Get().allocateDescriptorSets(allocInfo);

        // ------------------------------------------------------------------
        // Pipeline layout
        // ------------------------------------------------------------------
        auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo().setSetLayouts(descriptorSetLayout);

        auto pipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        // ------------------------------------------------------------------
        // Pipeline state
        // ------------------------------------------------------------------
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
                .setCullMode(vk::CullModeFlagBits::eNone)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f)
                .setDepthBiasEnable(false);

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

        auto blendState =
            vk::PipelineColorBlendStateCreateInfo()
                .setLogicOpEnable(false)
                .setLogicOp(vk::LogicOp::eCopy)
                .setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f })
                .setAttachmentCount(1)
                .setPAttachments(&noBlendAttachment);

        auto noDepthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        auto emptyVertexInput = vk::PipelineVertexInputStateCreateInfo()
                                    .setVertexBindingDescriptions({})
                                    .setVertexAttributeDescriptions({});

        // ------------------------------------------------------------------
        // Create composite pipeline
        // ------------------------------------------------------------------
        auto compositePipeline =
            mDevice->Get()
                .createGraphicsPipeline(
                    nullptr,
                    vk::GraphicsPipelineCreateInfo()
                        .setStages(stages)
                        .setPVertexInputState(&emptyVertexInput)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPDepthStencilState(&noDepthStencil)
                        .setPMultisampleState(&multisampling)
                        .setPColorBlendState(&blendState)
                        .setPDynamicState(&dynamicState)
                        .setLayout(pipelineLayout)
                        .setRenderPass(renderPass)
                        .setSubpass(0)
                        .setBasePipelineHandle(nullptr))
                .value;

        // Cleanup shaders
        mDevice->Get().destroyShaderModule(vertShader->Get());
        mDevice->Get().destroyShaderModule(fragShader->Get());

        // ------------------------------------------------------------------
        // Framebuffers (one per swapchain image)
        // ------------------------------------------------------------------
        auto frames       = swapChain->GetFrames();
        auto framebuffers = std::vector<vk::Framebuffer>(frames.size());

        for (std::size_t i = 0; i < frames.size(); i++)
        {
            auto fbInfo =
                vk::FramebufferCreateInfo()
                    .setRenderPass(renderPass)
                    .setAttachments(frames[i].imageView)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1);

            framebuffers[i] = mDevice->Get().createFramebuffer(fbInfo);
        }

        return skr::MakeRef<CompositePass>(
            mDevice,
            mFreyaOptions,
            mSurface,
            renderPass,
            pipelineLayout,
            compositePipeline,
            framebuffers,
            descriptorPool,
            descriptorSetLayout,
            descriptorSets);
    }

    vk::RenderPass CompositePassBuilder::createRenderPass() const
    {
        const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
        };

        auto colorRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto subpasses = std::vector<vk::SubpassDescription> {
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef),
        };

        auto dependencies = std::vector<vk::SubpassDependency> {
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
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eNone),
        };

        auto renderPassInfo =
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies);

        return mDevice->Get().createRenderPass(renderPassInfo);
    }
} // namespace FREYA_NAMESPACE
