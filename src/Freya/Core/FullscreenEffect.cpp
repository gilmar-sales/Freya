#include "Freya/Core/FullscreenEffect.hpp"
#include "Freya/Core/FullscreenEffectStage.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/DebugLabels.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/RenderFrameContext.hpp"
#include "Freya/Core/TaaPass.hpp"
#include "Freya/Core/TranslucentPass.hpp"

#include "Freya/Internal/FullscreenEffectImpl.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

namespace FREYA_NAMESPACE
{
    FullscreenEffect::FullscreenEffect(std::unique_ptr<Impl> impl) :
        mImpl(std::move(impl))
    {
        if (mImpl && mImpl->maskBuffer)
            mImpl->uploadMaterialMask();
    }

    const char* FullscreenEffect::Name() const
    {
        return mImpl ? mImpl->name.c_str() : "FullscreenEffect";
    }

    void FullscreenEffect::SetEnabled(const bool enabled)
    {
        mImpl->enabled = enabled;
    }

    bool FullscreenEffect::Enabled() const
    {
        return mImpl->enabled;
    }

    FullscreenEffect::~FullscreenEffect()
    {
        mImpl->destroyGpu();
    }

    void FullscreenEffect::BindMaterial(const std::uint32_t materialId)
    {
        const auto id   = materialId & 0xFFu;
        const auto word = id >> 5u;
        const auto bit  = 1u << (id & 31u);
        if ((mImpl->materialBits[word] & bit) != 0)
            return;
        mImpl->materialBits[word] |= bit;
        mImpl->maskDirty = true;
    }

    void FullscreenEffect::UnbindMaterial(const std::uint32_t materialId)
    {
        const auto id   = materialId & 0xFFu;
        const auto word = id >> 5u;
        const auto bit  = 1u << (id & 31u);
        if ((mImpl->materialBits[word] & bit) == 0)
            return;
        mImpl->materialBits[word] &= ~bit;
        mImpl->maskDirty = true;
    }

    void FullscreenEffect::ClearMaterials()
    {
        mImpl->materialBits.fill(0);
        mImpl->maskDirty = true;
    }

    void FullscreenEffect::Impl::uploadMaterialMask()
    {
        if (!maskBuffer)
            return;

        FullscreenMaterialMask mask {};
        mask.bits[0] = { materialBits[0], materialBits[1], materialBits[2],
                         materialBits[3] };
        mask.bits[1] = { materialBits[4], materialBits[5], materialBits[6],
                         materialBits[7] };
        for (const auto word : materialBits)
            mask.count += std::popcount(word);

        maskBuffer->Copy(&mask, sizeof(mask));
        maskDirty = false;
    }

    void FullscreenEffect::SetPushConstants(const void*   data,
                                            std::uint32_t size)
    {
        if (mImpl->pushConstantSize == 0 || data == nullptr)
            return;
        const auto copy = std::min(size, mImpl->pushConstantSize);
        if (mImpl->pushData.size() < mImpl->pushConstantSize)
            mImpl->pushData.resize(mImpl->pushConstantSize);
        std::memcpy(mImpl->pushData.data(), data, copy);
        if (copy < mImpl->pushConstantSize)
            std::memset(mImpl->pushData.data() + copy,
                        0,
                        mImpl->pushConstantSize - copy);
    }

    FrameStagePtr FullscreenEffect::MakeStage()
    {
        return std::make_shared<FullscreenEffectStage>(shared_from_this());
    }

    void FullscreenEffect::Impl::destroyGpu()
    {
        if (!device)
            return;

        auto& vkDevice = device->Get();
        if (framebuffer)
        {
            vkDevice.destroyFramebuffer(framebuffer);
            framebuffer = nullptr;
        }
        output.reset();
        if (pipeline)
        {
            vkDevice.destroyPipeline(pipeline);
            pipeline = nullptr;
        }
        if (pipelineLayout)
        {
            vkDevice.destroyPipelineLayout(pipelineLayout);
            pipelineLayout = nullptr;
        }
        if (descriptorPool)
        {
            vkDevice.destroyDescriptorPool(descriptorPool);
            descriptorPool = nullptr;
            descriptorSets.clear();
            maskDescriptorSets.clear();
        }
        if (setLayout)
        {
            vkDevice.destroyDescriptorSetLayout(setLayout);
            setLayout = nullptr;
        }
        if (maskSetLayout)
        {
            vkDevice.destroyDescriptorSetLayout(maskSetLayout);
            maskSetLayout = nullptr;
        }
        if (renderPass)
        {
            vkDevice.destroyRenderPass(renderPass);
            renderPass = nullptr;
        }
        if (sampler)
        {
            vkDevice.destroySampler(sampler);
            sampler = nullptr;
        }
        extent = vk::Extent2D {};
    }

    skr::Arc<Image> FullscreenEffect::Impl::resolveHdr(
        const RenderFrameContext& ctx) const
    {
        if (ctx.translucent && *ctx.translucent)
        {
            if (auto img = (*ctx.translucent)
                               ->GetSceneWithTranslucency(ctx.frameIndex))
                return img;
        }
        if (ctx.taa && *ctx.taa)
            return (*ctx.taa)->GetOutputImage();
        if (ctx.deferred && *ctx.deferred)
            return (*ctx.deferred)->GetSceneColorImage();
        return {};
    }

    skr::Arc<Image> FullscreenEffect::Impl::resolveInput(
        const EffectInput         input,
        const RenderFrameContext& ctx,
        const skr::Arc<Image>&    hdr) const
    {
        if (!ctx.deferred || !*ctx.deferred)
            return {};

        switch (input)
        {
            case EffectInput::SceneColor:
                return hdr;
            case EffectInput::Depth:
                return (*ctx.deferred)->GetDepthImage();
            case EffectInput::Albedo:
                return (*ctx.deferred)->GetAlbedoImage();
            case EffectInput::Normal:
                return (*ctx.deferred)->GetNormalImage();
            case EffectInput::Pbr:
                return (*ctx.deferred)->GetPbrImage();
            case EffectInput::Velocity:
                return (*ctx.deferred)->GetVelocityImage();
        }
        return {};
    }

    vk::ImageLayout FullscreenEffect::Impl::inputLayout(EffectInput input) const
    {
        if (input == EffectInput::Depth)
            return vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        return vk::ImageLayout::eShaderReadOnlyOptimal;
    }

    void FullscreenEffect::Rebuild(RenderFrameContext&   ctx,
                                   skr::ServiceProvider& sp)
    {
        mImpl->destroyGpu();

        if (!mImpl->device || mImpl->fragmentRelative.empty() ||
            mImpl->inputs.empty())
            return;

        mImpl->extent = ctx.renderExtent;
        if (mImpl->extent.width == 0 || mImpl->extent.height == 0)
            return;

        const auto& root       = mImpl->options->shaderRoot;
        auto        loadShader = [&](const std::string& relative) {
            return sp.GetService<ShaderModuleBuilder>()
                ->SetFilePath(root + "/" + relative)
                .Build();
        };

        auto vertShader = loadShader(mImpl->vertexRelative);
        auto fragShader = loadShader(mImpl->fragmentRelative);
        if (!vertShader || !fragShader)
            return;

        auto& vkDevice = mImpl->device->Get();

        auto attachment =
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eTransferSrcOptimal);

        auto colorRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef);

        auto dependencies = std::array {
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eFragmentShader |
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
            vk::SubpassDependency()
                .setSrcSubpass(0)
                .setDstSubpass(vk::SubpassExternal)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput)
                .setDstStageMask(vk::PipelineStageFlagBits::eTransfer)
                .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead),
        };

        mImpl->renderPass = vkDevice.createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachment)
                .setSubpasses(subpass)
                .setDependencies(dependencies));

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        bindings.reserve(mImpl->inputs.size());
        for (std::uint32_t i = 0; i < mImpl->inputs.size(); ++i)
        {
            bindings.push_back(
                vk::DescriptorSetLayoutBinding()
                    .setBinding(i)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setStageFlags(vk::ShaderStageFlagBits::eFragment));
        }

        mImpl->setLayout = vkDevice.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));

        auto maskBindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eFragment),
        };
        mImpl->maskSetLayout = vkDevice.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(maskBindings));

        const auto frameCount = std::max(1u, mImpl->options->frameCount);
        auto       poolSizes  = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(
                    (static_cast<std::uint32_t>(mImpl->inputs.size()) + 1) *
                    frameCount),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(frameCount),
        };

        mImpl->descriptorPool = vkDevice.createDescriptorPool(
            vk::DescriptorPoolCreateInfo().setPoolSizes(poolSizes).setMaxSets(
                frameCount * 2));

        auto inputLayouts =
            std::vector<vk::DescriptorSetLayout>(frameCount, mImpl->setLayout);
        mImpl->descriptorSets = vkDevice.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mImpl->descriptorPool)
                .setSetLayouts(inputLayouts));

        auto maskLayouts =
            std::vector<vk::DescriptorSetLayout>(frameCount,
                                                 mImpl->maskSetLayout);
        mImpl->maskDescriptorSets = vkDevice.allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mImpl->descriptorPool)
                .setSetLayouts(maskLayouts));

        auto pipelineSetLayouts =
            std::array { mImpl->setLayout, mImpl->maskSetLayout };

        vk::PushConstantRange        pushRange;
        vk::PipelineLayoutCreateInfo layoutInfo;
        layoutInfo.setSetLayouts(pipelineSetLayouts);
        if (mImpl->pushConstantSize > 0)
        {
            pushRange.setStageFlags(vk::ShaderStageFlagBits::eFragment)
                .setOffset(0)
                .setSize(mImpl->pushConstantSize);
            layoutInfo.setPushConstantRanges(pushRange);
        }
        mImpl->pipelineLayout = vkDevice.createPipelineLayout(layoutInfo);

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

        auto emptyVertexInput = vk::PipelineVertexInputStateCreateInfo();
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
                .setAttachmentCount(1)
                .setPAttachments(&noBlendAttachment);
        auto noDepthStencil = vk::PipelineDepthStencilStateCreateInfo()
                                  .setDepthTestEnable(false)
                                  .setDepthWriteEnable(false);

        mImpl->pipeline =
            vkDevice
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
                        .setLayout(mImpl->pipelineLayout)
                        .setRenderPass(mImpl->renderPass)
                        .setSubpass(0))
                .value;

        vkDevice.destroyShaderModule(vertShader->Get());
        vkDevice.destroyShaderModule(fragShader->Get());

        mImpl->sampler = vkDevice.createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eNearest)
                .setMinFilter(vk::Filter::eNearest)
                .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        mImpl->output =
            sp.GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(mImpl->extent.width)
                .SetHeight(mImpl->extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        mImpl->framebuffer = vkDevice.createFramebuffer(
            vk::FramebufferCreateInfo()
                .setRenderPass(mImpl->renderPass)
                .setAttachments(mImpl->output->GetImageView())
                .setWidth(mImpl->extent.width)
                .setHeight(mImpl->extent.height)
                .setLayers(1));
    }

    void FullscreenEffect::Execute(RenderFrameContext& ctx)
    {
        if (!mImpl->enabled || !mImpl->pipeline || !mImpl->output ||
            !ctx.commandPool)
            return;

        if (ctx.renderExtent.width != mImpl->extent.width ||
            ctx.renderExtent.height != mImpl->extent.height)
        {
            if (ctx.swapChain)
                Rebuild(ctx, *mImpl->serviceProvider);
            if (!mImpl->pipeline)
                return;
        }

        const auto hdr = mImpl->resolveHdr(ctx);
        if (!hdr)
            return;

        const auto frameIndex = std::min(
            ctx.frameIndex,
            static_cast<std::uint32_t>(mImpl->descriptorSets.size() - 1));

        if (mImpl->maskDirty)
            mImpl->uploadMaterialMask();

        std::vector<vk::DescriptorImageInfo> imageInfos;
        std::vector<vk::WriteDescriptorSet>  writes;
        imageInfos.reserve(mImpl->inputs.size() + 1);
        writes.reserve(mImpl->inputs.size() + 3);

        for (std::uint32_t i = 0; i < mImpl->inputs.size(); ++i)
        {
            auto image = mImpl->resolveInput(mImpl->inputs[i], ctx, hdr);
            if (!image)
                return;

            imageInfos.push_back(
                vk::DescriptorImageInfo()
                    .setSampler(mImpl->sampler)
                    .setImageView(image->GetImageView())
                    .setImageLayout(mImpl->inputLayout(mImpl->inputs[i])));
        }

        for (std::uint32_t i = 0; i < mImpl->inputs.size(); ++i)
        {
            writes.push_back(vk::WriteDescriptorSet()
                                 .setDstSet(mImpl->descriptorSets[frameIndex])
                                 .setDstBinding(i)
                                 .setDescriptorType(
                                     vk::DescriptorType::eCombinedImageSampler)
                                 .setDescriptorCount(1)
                                 .setImageInfo(imageInfos[i]));
        }

        auto albedo = mImpl->resolveInput(EffectInput::Albedo, ctx, hdr);
        if (!albedo || mImpl->maskDescriptorSets.empty() || !mImpl->maskBuffer)
            return;

        imageInfos.push_back(
            vk::DescriptorImageInfo()
                .setSampler(mImpl->sampler)
                .setImageView(albedo->GetImageView())
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal));
        writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(mImpl->maskDescriptorSets[frameIndex])
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(imageInfos.back()));

        auto maskBufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mImpl->maskBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(FullscreenMaterialMask));
        writes.push_back(
            vk::WriteDescriptorSet()
                .setDstSet(mImpl->maskDescriptorSets[frameIndex])
                .setDstBinding(1)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(maskBufferInfo));

        mImpl->device->Get().updateDescriptorSets(writes, nullptr);

        auto commandBuffer = ctx.commandPool->GetCommandBuffer();
        mImpl->device->BeginDebugLabel(commandBuffer,
                                       DebugLabel::ForStage(Name()));

        auto viewport =
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(mImpl->extent.width))
                .setHeight(static_cast<float>(mImpl->extent.height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);
        auto scissor =
            vk::Rect2D().setOffset({ 0, 0 }).setExtent(mImpl->extent);

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mImpl->renderPass)
                .setFramebuffer(mImpl->framebuffer)
                .setRenderArea(scissor),
            vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mImpl->pipeline);
        auto boundSets = std::array { mImpl->descriptorSets[frameIndex],
                                      mImpl->maskDescriptorSets[frameIndex] };
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mImpl->pipelineLayout,
            0,
            static_cast<std::uint32_t>(boundSets.size()),
            boundSets.data(),
            0,
            nullptr);
        commandBuffer.setViewport(0, 1, &viewport);
        commandBuffer.setScissor(0, 1, &scissor);

        if (mImpl->pushConstantSize > 0 && !mImpl->pushData.empty())
        {
            commandBuffer.pushConstants(
                mImpl->pipelineLayout,
                vk::ShaderStageFlagBits::eFragment,
                0,
                mImpl->pushConstantSize,
                mImpl->pushData.data());
        }

        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();

        const auto colorRange =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto dstToTransfer =
            vk::ImageMemoryBarrier()
                .setImage(hdr->GetImage())
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eColorAttachmentWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSubresourceRange(colorRange);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eTransfer,
            {},
            nullptr,
            nullptr,
            dstToTransfer);

        const auto w = static_cast<std::int32_t>(mImpl->extent.width);
        const auto h = static_cast<std::int32_t>(mImpl->extent.height);
        auto       blit =
            vk::ImageBlit {}
                .setSrcSubresource(
                    vk::ImageSubresourceLayers {}
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setMipLevel(0)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1))
                .setDstSubresource(
                    vk::ImageSubresourceLayers {}
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setMipLevel(0)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));
        auto srcOffsets =
            std::array { vk::Offset3D { 0, 0, 0 }, vk::Offset3D { w, h, 1 } };
        blit.setSrcOffsets(srcOffsets);
        blit.setDstOffsets(srcOffsets);

        commandBuffer.blitImage(
            mImpl->output->GetImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            hdr->GetImage(),
            vk::ImageLayout::eTransferDstOptimal,
            blit,
            vk::Filter::eNearest);

        auto dstToSampled =
            vk::ImageMemoryBarrier()
                .setImage(hdr->GetImage())
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSubresourceRange(colorRange);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            {},
            nullptr,
            nullptr,
            dstToSampled);

        mImpl->device->EndDebugLabel(commandBuffer);
    }
} // namespace FREYA_NAMESPACE
