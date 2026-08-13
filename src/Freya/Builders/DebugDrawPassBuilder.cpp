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

    skr::Arc<DebugDrawPass> DebugDrawPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain)
    {
        auto renderPass = createRenderPass();

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto vertShader = loadShader("Debug/line.vert.spv");
        auto fragShader = loadShader("Debug/line.frag.spv");

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

        auto pushRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                             .setOffset(0)
                             .setSize(sizeof(glm::mat4));

        auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setPushConstantRanges(pushRange));

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

        auto pipeline =
            mDevice->Get()
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
                        .setLayout(pipelineLayout)
                        .setRenderPass(renderPass)
                        .setSubpass(0))
                .value;

        mDevice->Get().destroyShaderModule(vertShader->Get());
        mDevice->Get().destroyShaderModule(fragShader->Get());

        auto       frames       = swapChain->GetFrames();
        auto       framebuffers = std::vector<vk::Framebuffer>(frames.size());
        const auto extent       = swapChain->GetExtent();
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            auto views      = std::array { frames[i].imageView };
            framebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(renderPass)
                    .setAttachments(views)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }

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

        return skr::MakeArc<DebugDrawPass>(
            mDevice, mFreyaOptions, renderPass, pipelineLayout, pipeline,
            std::move(framebuffers), std::move(vertexBuffers), maxVerts);
    }

    vk::RenderPass DebugDrawPassBuilder::createRenderPass() const
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

} // namespace FREYA_NAMESPACE
