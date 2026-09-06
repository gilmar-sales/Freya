#include "Freya/Builders/DebugDrawPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    DebugDrawPassBuilder::DebugDrawPassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    vk::Pipeline DebugDrawPassBuilder::createPipeline(
        const vk::ShaderModule vert, const vk::ShaderModule frag,
        const vk::PipelineLayout layout, const vk::RenderPass renderPass) const
    {
        auto stages = std::array {
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(vert)
                .setPName("main"),
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(frag)
                .setPName("main"),
        };

        auto binding = vk::VertexInputBindingDescription()
                           .setBinding(0)
                           .setStride(sizeof(DebugDrawVertex))
                           .setInputRate(vk::VertexInputRate::eVertex);

        auto attrs = std::array {
            vk::VertexInputAttributeDescription()
                .setLocation(0)
                .setBinding(0)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(DebugDrawVertex, position)),
            vk::VertexInputAttributeDescription()
                .setLocation(1)
                .setBinding(0)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(offsetof(DebugDrawVertex, color)),
        };

        auto vertexInput = vk::PipelineVertexInputStateCreateInfo()
                               .setVertexBindingDescriptions(binding)
                               .setVertexAttributeDescriptions(attrs);

        auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo()
                                 .setTopology(vk::PrimitiveTopology::eLineList)
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

        auto blendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setAlphaBlendOp(vk::BlendOp::eAdd)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA);

        auto blendState = vk::PipelineColorBlendStateCreateInfo()
                              .setLogicOpEnable(false)
                              .setAttachments(blendAttachment);

        auto noDepthStencil = vk::PipelineDepthStencilStateCreateInfo()
                                  .setDepthTestEnable(false)
                                  .setDepthWriteEnable(false);

        return mDevice->Get()
            .createGraphicsPipeline(
                nullptr,
                vk::GraphicsPipelineCreateInfo()
                    .setStages(stages)
                    .setPVertexInputState(&vertexInput)
                    .setPInputAssemblyState(&inputAssembly)
                    .setPViewportState(&viewportState)
                    .setPRasterizationState(&rasterizer)
                    .setPDepthStencilState(&noDepthStencil)
                    .setPMultisampleState(&multisampling)
                    .setPColorBlendState(&blendState)
                    .setPDynamicState(&dynamicState)
                    .setLayout(layout)
                    .setRenderPass(renderPass)
                    .setSubpass(0))
            .value;
    }

    skr::Arc<DebugDrawPass> DebugDrawPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain)
    {
        auto swapchainPass = createSwapchainRenderPass();
        auto offscreenPass = createOffscreenRenderPass();

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto vertShader = loadShader("Debug/line.vert.spv");
        auto fragShader = loadShader("Debug/line.frag.spv");

        auto pushRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                             .setOffset(0)
                             .setSize(sizeof(glm::mat4));

        auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setPushConstantRanges(pushRange));

        auto v = vertShader->Get();
        auto f = fragShader->Get();
        auto swapchainPipeline =
            createPipeline(v, f, pipelineLayout, swapchainPass);
        auto offscreenPipeline =
            createPipeline(v, f, pipelineLayout, offscreenPass);

        mDevice->Get().destroyShaderModule(v);
        mDevice->Get().destroyShaderModule(f);

        constexpr auto maxVerts = DebugDraw::kDefaultMaxVertices;
        const auto     byteSize =
            static_cast<std::uint64_t>(maxVerts) * sizeof(DebugDrawVertex);

        std::vector<skr::Arc<Buffer>> vertexBuffers;
        vertexBuffers.reserve(mFreyaOptions->frameCount);
        for (std::uint32_t i = 0; i < mFreyaOptions->frameCount; ++i)
        {
            vertexBuffers.push_back(BufferBuilder(mDevice)
                                        .SetUsage(BufferUsage::Vertex)
                                        .SetSize(byteSize)
                                        .Build());
        }

        auto pass = skr::MakeArc<DebugDrawPass>(
            mDevice, mFreyaOptions, swapchainPass, offscreenPass,
            pipelineLayout, swapchainPipeline, offscreenPipeline,
            std::vector<vk::Framebuffer> {}, std::move(vertexBuffers),
            maxVerts);
        if (swapChain)
            pass->UpdateSwapchain(swapChain);
        return pass;
    }

    vk::RenderPass DebugDrawPassBuilder::createSwapchainRenderPass() const
    {
        const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::ePresentSrcKHR)
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
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                                  vk::AccessFlagBits::eColorAttachmentRead),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eNone),
        };

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies));
    }

    vk::RenderPass DebugDrawPassBuilder::createOffscreenRenderPass() const
    {
        const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;

        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
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
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eFragmentShader)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                                  vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                                  vk::AccessFlagBits::eColorAttachmentRead),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
        };

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpasses)
                .setDependencies(dependencies));
    }

} // namespace FREYA_NAMESPACE
