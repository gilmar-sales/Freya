#include "Freya/Builders/UiPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/UiDraw.hpp"
#include "Freya/Core/UiGpu.hpp"

#include <array>
#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct UiPush
        {
            glm::vec2 framebufferSize { 1.f, 1.f };
        };
    } // namespace

    UiPassBuilder::UiPassBuilder(
        const skr::Arc<Device>&                      device,
        const skr::Arc<PhysicalDevice>&              physicalDevice,
        const skr::Arc<Surface>&                     surface,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const skr::Arc<skr::ServiceProvider>&        serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mMaterials(materials),
        mServiceProvider(serviceProvider)
    {
    }

    vk::Pipeline UiPassBuilder::createPipeline(
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

        auto vertexInput = vk::PipelineVertexInputStateCreateInfo();
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
                .setLineWidth(1.0f);

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

    skr::Arc<UiPass> UiPassBuilder::Build(const skr::Arc<SwapChain>& swapChain)
    {
        auto swapchainPass = createSwapchainRenderPass();
        auto offscreenPass = createOffscreenRenderPass();

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto vertShader = loadShader("Ui/ui.vert.spv");
        auto fragShader = loadShader("Ui/ui.frag.spv");

        auto instanceBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex);

        auto setLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(instanceBinding));

        auto pushRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eVertex)
                             .setOffset(0)
                             .setSize(sizeof(UiPush));

        auto setLayouts =
            std::array { setLayout, mMaterials->GetBindlessLayout() };
        auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayouts)
                .setPushConstantRanges(pushRange));

        auto v = vertShader->Get();
        auto f = fragShader->Get();
        auto swapchainPipeline =
            createPipeline(v, f, pipelineLayout, swapchainPass);
        auto offscreenPipeline =
            createPipeline(v, f, pipelineLayout, offscreenPass);

        mDevice->Get().destroyShaderModule(v);
        mDevice->Get().destroyShaderModule(f);

        constexpr auto maxQuads = UiDraw::kDefaultMaxQuads;
        const auto     byteSize =
            static_cast<std::uint64_t>(maxQuads) * sizeof(UiGpuInstance);

        const auto frameCount = mFreyaOptions->frameCount;
        auto       poolSize   = vk::DescriptorPoolSize()
                                    .setType(vk::DescriptorType::eStorageBuffer)
                                    .setDescriptorCount(frameCount);
        auto       pool       = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo().setPoolSizes(poolSize).setMaxSets(
                frameCount));

        std::vector<vk::DescriptorSetLayout> layouts(frameCount, setLayout);
        auto sets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(pool)
                .setSetLayouts(layouts));

        std::vector<skr::Arc<Buffer>> buffers;
        buffers.reserve(frameCount);
        std::vector<vk::WriteDescriptorSet>   writes;
        std::vector<vk::DescriptorBufferInfo> infos;
        infos.reserve(frameCount);
        writes.reserve(frameCount);
        for (std::uint32_t i = 0; i < frameCount; ++i)
        {
            buffers.push_back(BufferBuilder(mDevice)
                                  .SetUsage(BufferUsage::Storage)
                                  .SetSize(byteSize)
                                  .Build());
            infos.push_back(vk::DescriptorBufferInfo()
                                .setBuffer(buffers.back()->Get())
                                .setOffset(0)
                                .setRange(byteSize));
            writes.push_back(
                vk::WriteDescriptorSet()
                    .setDstSet(sets[i])
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(infos.back()));
        }
        mDevice->Get().updateDescriptorSets(writes, nullptr);

        auto pass = skr::MakeArc<UiPass>(
            mDevice, mFreyaOptions, mMaterials, swapchainPass, offscreenPass,
            pipelineLayout, setLayout, pool, sets, std::move(buffers),
            swapchainPipeline, offscreenPipeline,
            std::vector<vk::Framebuffer> {}, maxQuads);

        if (swapChain)
            pass->UpdateSwapchain(swapChain);
        return pass;
    }

    vk::RenderPass UiPassBuilder::createSwapchainRenderPass() const
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

    vk::RenderPass UiPassBuilder::createOffscreenRenderPass() const
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
