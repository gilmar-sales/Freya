#include "Freya/Builders/TranslucentPassBuilder.hpp"

#include "Freya/Internal/LightServiceGpu.hpp"
#include "Freya/Internal/VertexInput.hpp"

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <array>

namespace FREYA_NAMESPACE
{
    TranslucentPassBuilder::TranslucentPassBuilder(
        const skr::Arc<Device>&                      device,
        const skr::Arc<PhysicalDevice>&              physicalDevice,
        const skr::Arc<Surface>&                     surface,
        const skr::Arc<FreyaOptions>&                freyaOptions,
        const skr::Arc<MaterialDescriptorResources>& materialResources,
        const skr::Arc<BoneMatrixResources>&         boneResources,
        const skr::Arc<LightService>&                lightService,
        const skr::Arc<IBLService>&                  iblService,
        const skr::Arc<skr::ServiceProvider>&        serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mMaterialResources(materialResources),
        mBoneResources(boneResources), mLightService(lightService),
        mIblService(iblService), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<TranslucentPass> TranslucentPassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain, const skr::Arc<Image>& depthImage,
        vk::Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
            extent = swapChain->GetExtent();

        const auto& root       = mFreyaOptions->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto accumVert = loadShader("DeferredCompressed/oit_accum.vert.spv");
        auto accumFrag = loadShader("DeferredCompressed/oit_accum.frag.spv");
        auto resVert   = loadShader("DeferredCompressed/oit_resolve.vert.spv");
        auto resFrag   = loadShader("DeferredCompressed/oit_resolve.frag.spv");

        const auto depthFormat = mPhysicalDevice->GetDepthFormat();

        auto createImage =
            [&](ImageUsage usage, std::optional<vk::Format> format = {}) {
                auto builder =
                    mServiceProvider->GetService<ImageBuilder>()
                        ->SetUsage(usage)
                        .SetWidth(extent.width)
                        .SetHeight(extent.height)
                        .SetSamples(vk::SampleCountFlagBits::e1);
                if (format)
                    builder.SetFormat(*format);
                return builder.Build();
            };

        const auto frameCount = mFreyaOptions->frameCount;

        std::vector<skr::Arc<Image>> oitAccum(frameCount);
        std::vector<skr::Arc<Image>> oitReveal(frameCount);
        std::vector<skr::Arc<Image>> sceneWithTranslucency(frameCount);
        for (std::uint32_t i = 0; i < frameCount; ++i)
        {
            oitAccum[i]  = createImage(ImageUsage::GBufferSceneColor);
            oitReveal[i] = createImage(ImageUsage::Color, vk::Format::eR8Unorm);
            sceneWithTranslucency[i] =
                createImage(ImageUsage::GBufferSceneColor);
        }

        auto sampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // --- Accumulate render pass: accum + reveal + depth (load) -----------
        auto accumAttachments = std::array {
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR8Unorm)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
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

        auto accumColorRefs = std::array {
            vk::AttachmentReference().setAttachment(0).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal),
            vk::AttachmentReference().setAttachment(1).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal),
        };
        auto accumDepthRef =
            vk::AttachmentReference().setAttachment(2).setLayout(
                vk::ImageLayout::eDepthStencilReadOnlyOptimal);

        auto accumSubpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(accumColorRefs)
                .setPDepthStencilAttachment(&accumDepthRef);

        auto accumDeps = std::array {
            vk::SubpassDependency()
                .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests |
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                    vk::PipelineStageFlagBits::eLateFragmentTests |
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                    vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(
                    vk::AccessFlagBits::eDepthStencilAttachmentRead |
                    vk::AccessFlagBits::eColorAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(VK_SUBPASS_EXTERNAL)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead),
        };

        auto accumulateRenderPass = mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(accumAttachments)
                .setSubpasses(accumSubpass)
                .setDependencies(accumDeps));

        // --- Resolve render pass ---------------------------------------------
        auto resolveAttachment =
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto resolveColorRef =
            vk::AttachmentReference().setAttachment(0).setLayout(
                vk::ImageLayout::eColorAttachmentOptimal);

        auto resolveSubpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(resolveColorRef);

        auto resolveDeps = std::array {
            vk::SubpassDependency()
                .setSrcSubpass(VK_SUBPASS_EXTERNAL)
                .setDstSubpass(0)
                .setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(VK_SUBPASS_EXTERNAL)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eFragmentShader |
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eColorAttachmentRead),
        };

        auto resolveRenderPass = mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(resolveAttachment)
                .setSubpasses(resolveSubpass)
                .setDependencies(resolveDeps));

        // --- Camera set 0 + bindless set 1 -----------------------------------
        auto uboBinding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                               vk::ShaderStageFlagBits::eFragment);

        auto cameraSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(uboBinding));

        auto cameraPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(mFreyaOptions->frameCount),
        };
        auto cameraPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setMaxSets(mFreyaOptions->frameCount)
                .setPoolSizes(cameraPoolSizes));

        auto cameraLayouts = std::vector<vk::DescriptorSetLayout>(
            mFreyaOptions->frameCount, cameraSetLayout);
        auto cameraSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(cameraPool)
                .setSetLayouts(cameraLayouts));

        auto uniformBuffer =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(ProjectionUniformBuffer) *
                         mFreyaOptions->frameCount)
                .Build();

        for (std::uint32_t i = 0; i < mFreyaOptions->frameCount; ++i)
        {
            auto bufInfo = vk::DescriptorBufferInfo()
                               .setBuffer(uniformBuffer->Get())
                               .setOffset(sizeof(ProjectionUniformBuffer) * i)
                               .setRange(sizeof(ProjectionUniformBuffer));
            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(cameraSets[i])
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufInfo);
            mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
        }

        auto& bindlessLayout  = mMaterialResources->GetBindlessLayout();
        auto  lightLayout     = LightServiceGpu::Layout(*mLightService);

        // Set 4: opaque HDR (refraction) + IBL (parity with deferred).
        auto glassBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
        };
        auto glassSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(glassBindings));

        auto glassPoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(4 * mFreyaOptions->frameCount),
        };
        auto glassPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setMaxSets(mFreyaOptions->frameCount)
                .setPoolSizes(glassPoolSizes));

        auto glassLayouts = std::vector<vk::DescriptorSetLayout>(
            mFreyaOptions->frameCount, glassSetLayout);
        auto glassSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(glassPool)
                .setSetLayouts(glassLayouts));

        auto irradianceInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetIrradianceSampler())
                .setImageView(mIblService->GetIrradianceMap()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto prefilterInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetEnvironmentSampler())
                .setImageView(mIblService->GetEnvironmentMap()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        auto brdfInfo =
            vk::DescriptorImageInfo()
                .setSampler(mIblService->GetBrdfSampler())
                .setImageView(mIblService->GetBrdfLut()->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        // Placeholder opaque until BeginAccumulate rebinds the real HDR.
        auto placeholderOpaque =
            vk::DescriptorImageInfo()
                .setSampler(sampler)
                .setImageView(sceneWithTranslucency[0]->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        for (std::uint32_t i = 0; i < mFreyaOptions->frameCount; ++i)
        {
            auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(glassSets[i])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(placeholderOpaque),
                vk::WriteDescriptorSet()
                    .setDstSet(glassSets[i])
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(irradianceInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(glassSets[i])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(prefilterInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(glassSets[i])
                    .setDstBinding(3)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(brdfInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                nullptr);
        }

        auto accumSetLayouts = std::array {
            cameraSetLayout,
            bindlessLayout,
            lightLayout,
            mBoneResources->GetLayout(),
            glassSetLayout,
        };
        auto accumulateLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setSetLayouts(accumSetLayouts));

        // --- Resolve descriptors ---------------------------------------------
        auto resolveBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
        };
        auto resolveSetLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(resolveBindings));

        auto resolvePoolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(3 * mFreyaOptions->frameCount),
        };
        auto resolvePool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setMaxSets(mFreyaOptions->frameCount)
                .setPoolSizes(resolvePoolSizes));

        auto resolveLayouts = std::vector<vk::DescriptorSetLayout>(
            mFreyaOptions->frameCount, resolveSetLayout);
        auto resolveSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(resolvePool)
                .setSetLayouts(resolveLayouts));

        auto resolveLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo().setSetLayouts(resolveSetLayout));

        // Pre-bind per-frame accum/reveal (opaque updated in Resolve).
        for (std::uint32_t i = 0; i < mFreyaOptions->frameCount; ++i)
        {
            auto accumInfo =
                vk::DescriptorImageInfo()
                    .setSampler(sampler)
                    .setImageView(oitAccum[i]->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            auto revealInfo =
                vk::DescriptorImageInfo()
                    .setSampler(sampler)
                    .setImageView(oitReveal[i]->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            auto opaqueInfo =
                vk::DescriptorImageInfo()
                    .setSampler(sampler)
                    .setImageView(sceneWithTranslucency[i]->GetImageView())
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(resolveSets[i])
                    .setDstBinding(0)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(opaqueInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(resolveSets[i])
                    .setDstBinding(1)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(accumInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(resolveSets[i])
                    .setDstBinding(2)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(revealInfo),
            };
            mDevice->Get().updateDescriptorSets(
                static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                nullptr);
        }

        // --- Accumulate pipeline ---------------------------------------------
        auto makeStage = [](vk::ShaderModule        module,
                            vk::ShaderStageFlagBits stage) {
            return vk::PipelineShaderStageCreateInfo()
                .setStage(stage)
                .setModule(module)
                .setPName("main");
        };

        auto accumStages = std::array {
            makeStage(accumVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(accumFrag->Get(), vk::ShaderStageFlagBits::eFragment),
        };

        auto vertexBinding = GetVertexBindingDescription();
        auto vertexAttrs   = GetVertexAttributesDescription();
        auto vertexInput   = vk::PipelineVertexInputStateCreateInfo()
                                 .setVertexBindingDescriptions(vertexBinding)
                                 .setVertexAttributeDescriptions(vertexAttrs);

        auto inputAssembly =
            vk::PipelineInputAssemblyStateCreateInfo().setTopology(
                vk::PrimitiveTopology::eTriangleList);
        auto viewportState = vk::PipelineViewportStateCreateInfo()
                                 .setViewportCount(1)
                                 .setScissorCount(1);
        auto rasterizer =
            vk::PipelineRasterizationStateCreateInfo()
                .setCullMode(vk::CullModeFlagBits::eNone)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f);
        auto multisample =
            vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(
                vk::SampleCountFlagBits::e1);

        auto accumBlend0 =
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eOne)
                .setDstColorBlendFactor(vk::BlendFactor::eOne)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                .setDstAlphaBlendFactor(vk::BlendFactor::eOne)
                .setAlphaBlendOp(vk::BlendOp::eAdd)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA);
        // Reveal R8: dst *= (1 - src.r) via OneMinusSrcColor.
        auto accumBlend1 =
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eZero)
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcColor)
                .setColorBlendOp(vk::BlendOp::eAdd)
                .setSrcAlphaBlendFactor(vk::BlendFactor::eZero)
                .setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                .setAlphaBlendOp(vk::BlendOp::eAdd)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR);

        auto accumBlendAttachments = std::array { accumBlend0, accumBlend1 };
        auto accumBlendState =
            vk::PipelineColorBlendStateCreateInfo().setAttachments(
                accumBlendAttachments);

        auto depthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreaterOrEqual
                                       : vk::CompareOp::eLessOrEqual);

        auto dynamicStates = std::array { vk::DynamicState::eViewport,
                                          vk::DynamicState::eScissor };
        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        auto accumulatePipeline =
            mDevice->Get()
                .createGraphicsPipeline(
                    nullptr,
                    vk::GraphicsPipelineCreateInfo()
                        .setStages(accumStages)
                        .setPVertexInputState(&vertexInput)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPMultisampleState(&multisample)
                        .setPDepthStencilState(&depthStencil)
                        .setPColorBlendState(&accumBlendState)
                        .setPDynamicState(&dynamicState)
                        .setLayout(accumulateLayout)
                        .setRenderPass(accumulateRenderPass)
                        .setSubpass(0))
                .value;

        // --- Resolve pipeline ------------------------------------------------
        auto resolveStages = std::array {
            makeStage(resVert->Get(), vk::ShaderStageFlagBits::eVertex),
            makeStage(resFrag->Get(), vk::ShaderStageFlagBits::eFragment),
        };

        auto emptyVertexInput = vk::PipelineVertexInputStateCreateInfo();
        auto resolveBlend =
            vk::PipelineColorBlendAttachmentState()
                .setBlendEnable(false)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA);
        auto resolveBlendState =
            vk::PipelineColorBlendStateCreateInfo().setAttachments(
                resolveBlend);
        auto noDepth = vk::PipelineDepthStencilStateCreateInfo()
                           .setDepthTestEnable(false)
                           .setDepthWriteEnable(false);

        auto resolvePipeline =
            mDevice->Get()
                .createGraphicsPipeline(
                    nullptr,
                    vk::GraphicsPipelineCreateInfo()
                        .setStages(resolveStages)
                        .setPVertexInputState(&emptyVertexInput)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPMultisampleState(&multisample)
                        .setPDepthStencilState(&noDepth)
                        .setPColorBlendState(&resolveBlendState)
                        .setPDynamicState(&dynamicState)
                        .setLayout(resolveLayout)
                        .setRenderPass(resolveRenderPass)
                        .setSubpass(0))
                .value;

        std::vector<vk::Framebuffer> accumulateFramebuffers(frameCount);
        std::vector<vk::Framebuffer> resolveFramebuffers(frameCount);
        for (std::uint32_t i = 0; i < frameCount; ++i)
        {
            auto accumFbAttachments = std::array {
                oitAccum[i]->GetImageView(),
                oitReveal[i]->GetImageView(),
                depthImage->GetImageView(),
            };
            accumulateFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(accumulateRenderPass)
                    .setAttachments(accumFbAttachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));

            resolveFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(resolveRenderPass)
                    .setAttachments(sceneWithTranslucency[i]->GetImageView())
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }

        return skr::MakeArc<TranslucentPass>(
            mDevice, mFreyaOptions, mMaterialResources, mBoneResources,
            mLightService, mIblService, accumulateRenderPass, resolveRenderPass,
            accumulateLayout, resolveLayout, accumulatePipeline,
            resolvePipeline, uniformBuffer, cameraSetLayout, cameraPool,
            cameraSets, glassSetLayout, glassPool, glassSets, resolveSetLayout,
            resolvePool, resolveSets, std::move(oitAccum), std::move(oitReveal),
            std::move(sceneWithTranslucency), std::move(accumulateFramebuffers),
            std::move(resolveFramebuffers), sampler, depthFormat, extent);
    }

} // namespace FREYA_NAMESPACE
