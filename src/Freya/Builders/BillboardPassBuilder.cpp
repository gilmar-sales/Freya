#include "Freya/Builders/BillboardPassBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/ShaderModule.hpp"

#include <array>
#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        struct BillboardPush
        {
            glm::mat4 view { 1.f };
            glm::mat4 proj { 1.f };
        };
    } // namespace

    BillboardPassBuilder::BillboardPassBuilder(
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

    vk::RenderPass BillboardPassBuilder::createHdrRenderPass(
        const vk::Format depthFormat) const
    {
        auto attachments = std::array {
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(depthFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
        };

        auto colorRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);
        auto depthRef = vk::AttachmentReference().setAttachment(1).setLayout(
            vk::ImageLayout::eDepthStencilReadOnlyOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef)
                .setPDepthStencilAttachment(&depthRef);

        auto deps = std::array {
            vk::SubpassDependency()
                .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests |
                    vk::PipelineStageFlagBits::eFragmentShader)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eShaderRead |
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentRead)
                .setDstAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eColorAttachmentRead |
                    vk::AccessFlagBits::eDepthStencilAttachmentRead),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(VK_SUBPASS_EXTERNAL)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
        };

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(deps));
    }

    vk::RenderPass BillboardPassBuilder::createLdrRenderPass(
        const vk::Format depthFormat) const
    {
        const auto surfaceFormat = mSurface->QuerySurfaceFormat().format;
        auto       attachments   = std::array {
            vk::AttachmentDescription()
                .setFormat(surfaceFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::ePresentSrcKHR)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
            vk::AttachmentDescription()
                .setFormat(depthFormat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eLoad)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal)
                .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
        };

        auto colorRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);
        auto depthRef = vk::AttachmentReference().setAttachment(1).setLayout(
            vk::ImageLayout::eDepthStencilReadOnlyOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef)
                .setPDepthStencilAttachment(&depthRef);

        auto deps = std::array {
            vk::SubpassDependency()
                .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentRead)
                .setDstAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eColorAttachmentRead |
                    vk::AccessFlagBits::eDepthStencilAttachmentRead),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(VK_SUBPASS_EXTERNAL)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eNone),
        };

        return mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(deps));
    }

    vk::Pipeline BillboardPassBuilder::createPipeline(
        const vk::ShaderModule vert, const vk::ShaderModule frag,
        const vk::PipelineLayout layout, const vk::RenderPass renderPass,
        const bool additive, const bool depthTest) const
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
                .setSrcColorBlendFactor(additive ? vk::BlendFactor::eSrcAlpha
                                                 : vk::BlendFactor::eSrcAlpha)
                .setDstColorBlendFactor(
                    additive ? vk::BlendFactor::eOne
                             : vk::BlendFactor::eOneMinusSrcAlpha)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(
                    additive ? vk::BlendFactor::eOne
                             : vk::BlendFactor::eOneMinusSrcAlpha)
                .setAlphaBlendOp(vk::BlendOp::eAdd)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA);
        auto blendState = vk::PipelineColorBlendStateCreateInfo()
                              .setLogicOpEnable(false)
                              .setAttachments(blendAttachment);

        const auto compare = mFreyaOptions->ReverseZ ? vk::CompareOp::eGreater
                                                     : vk::CompareOp::eLess;
        auto       depthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(depthTest)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(compare)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

        return mDevice->Get()
            .createGraphicsPipeline(
                nullptr,
                vk::GraphicsPipelineCreateInfo()
                    .setStages(stages)
                    .setPVertexInputState(&vertexInput)
                    .setPInputAssemblyState(&inputAssembly)
                    .setPViewportState(&viewportState)
                    .setPRasterizationState(&rasterizer)
                    .setPDepthStencilState(&depthStencil)
                    .setPMultisampleState(&multisampling)
                    .setPColorBlendState(&blendState)
                    .setPDynamicState(&dynamicState)
                    .setLayout(layout)
                    .setRenderPass(renderPass)
                    .setSubpass(0))
            .value;
    }

    skr::Arc<BillboardPass> BillboardPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain, const skr::Arc<Image>& depthImage)
    {
        const auto depthFormat = mPhysicalDevice->GetDepthFormat();
        auto       hdrPass     = createHdrRenderPass(depthFormat);
        auto       ldrPass     = createLdrRenderPass(depthFormat);

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };
        auto vertShader = loadShader("Billboard/billboard.vert.spv");
        auto fragShader = loadShader("Billboard/billboard.frag.spv");

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
                             .setSize(sizeof(BillboardPush));

        auto setLayouts =
            std::array { setLayout, mMaterials->GetBindlessLayout() };
        auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayouts)
                .setPushConstantRanges(pushRange));

        BillboardPass::Pipelines hdr {};
        BillboardPass::Pipelines ldr {};
        auto                     v = vertShader->Get();
        auto                     f = fragShader->Get();
        hdr.alphaDepth =
            createPipeline(v, f, pipelineLayout, hdrPass, false, true);
        hdr.alphaNoDepth =
            createPipeline(v, f, pipelineLayout, hdrPass, false, false);
        hdr.addDepth =
            createPipeline(v, f, pipelineLayout, hdrPass, true, true);
        hdr.addNoDepth =
            createPipeline(v, f, pipelineLayout, hdrPass, true, false);
        ldr.alphaDepth =
            createPipeline(v, f, pipelineLayout, ldrPass, false, true);
        ldr.alphaNoDepth =
            createPipeline(v, f, pipelineLayout, ldrPass, false, false);
        ldr.addDepth =
            createPipeline(v, f, pipelineLayout, ldrPass, true, true);
        ldr.addNoDepth =
            createPipeline(v, f, pipelineLayout, ldrPass, true, false);

        mDevice->Get().destroyShaderModule(v);
        mDevice->Get().destroyShaderModule(f);

        constexpr auto maxQuads = BillboardDraw::kDefaultMaxQuads;
        const auto     byteSize =
            static_cast<std::uint64_t>(maxQuads) * sizeof(BillboardGpuInstance);

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

        const auto extent = swapChain->GetExtent();
        auto       pass   = skr::MakeArc<BillboardPass>(
            mDevice, mFreyaOptions, mMaterials, hdrPass, ldrPass,
            pipelineLayout, setLayout, pool, sets, std::move(buffers), hdr, ldr,
            std::vector<vk::Framebuffer> {}, extent, maxQuads);

        if (depthImage)
        {
            pass->UpdateLdrDepth(depthImage, swapChain);
        }
        return pass;
    }

} // namespace FREYA_NAMESPACE
