#include "Freya/Core/FrameStages.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Builders/XessPassBuilder.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/DebugLabels.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderFrameContext.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>

namespace FREYA_NAMESPACE
{
    namespace
    {
        void SetFullViewport(const skr::Arc<CommandPool>& commandPool,
                             const vk::Extent2D           extent)
        {
            const auto commandBuffer = commandPool->GetCommandBuffer();
            auto       viewport =
                vk::Viewport()
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<float>(extent.width))
                    .setHeight(static_cast<float>(extent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);
            auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);
        }
    } // namespace

    void PickFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.pick || !*ctx.pick || !ctx.pickRequested ||
            !*ctx.pickRequested)
            return;

        if (ctx.resizePickPass)
            ctx.resizePickPass(ctx.renderExtent);

        (*ctx.pick)->Render(ctx.commandPool, *ctx.projection,
                            ctx.executePickDraws);
        (*ctx.pick)->CopyPixel(ctx.commandPool, *ctx.pickX, *ctx.pickY);
        *ctx.pickRequested        = false;
        *ctx.pickAwaitingReadback = true;
    }

    void ShadowFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.options->enableShadows)
            return;

        if (!ctx.shadow || !*ctx.shadow || !ctx.lights || !*ctx.lights)
            return;

        (*ctx.shadow)
            ->Update(**ctx.lights,
                     ctx.projection->view,
                     ctx.projection->projection,
                     glm::vec3(glm::inverse(ctx.projection->view)[3]),
                     ctx.cameraNear,
                     ctx.options->drawDistance,
                     ctx.frameIndex);
        (*ctx.shadow)->Render(ctx.commandPool, [&]() {
            ctx.executeDraws(false, true);
        });
    }

    void DeferredGeometryFrameStage::Rebuild(RenderFrameContext&   ctx,
                                             skr::ServiceProvider& sp)
    {
        if (!ctx.deferred)
            return;
        ctx.deferred->reset();
        *ctx.deferred = sp.GetService<DeferredCompressedPassBuilder>()->Build(
            ctx.swapChain, ctx.renderExtent);
    }

    void DeferredGeometryFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.deferred || !*ctx.deferred)
            return;

        (*ctx.deferred)->Begin(ctx.swapChain, ctx.commandPool);
        SetFullViewport(ctx.commandPool, ctx.renderExtent);

        auto currentSubpass = (*ctx.deferred)->GetCurrentSubpass();
        if (currentSubpass == DefDepthPrePass)
        {
            ctx.executeDraws(false, false);
            (*ctx.deferred)
                ->AdvanceSubpass(DefGBufferPass, ctx.commandPool,
                                 ctx.frameIndex);
            ctx.executeDraws(true, false);
        }
        else if (currentSubpass == DefGBufferPass)
        {
            ctx.executeDraws(true, false);
        }

        (*ctx.deferred)->End(ctx.commandPool);
    }

    void SsaoLightingFrameStage::Rebuild(RenderFrameContext&   ctx,
                                         skr::ServiceProvider& sp)
    {
        if (!ctx.ssao)
            return;
        ctx.ssao->reset();
        if (ctx.options->enableSsao)
        {
            *ctx.ssao = sp.GetService<SsaoPassBuilder>()->Build(
                ctx.swapChain, ctx.renderExtent);
        }
        else if (ctx.createSsaoFallback && ctx.ssaoFallbackImage)
        {
            *ctx.ssaoFallbackImage = ctx.createSsaoFallback();
        }
    }

    void SsaoLightingFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.deferred || !*ctx.deferred)
            return;

        skr::Arc<Image> ssaoImage;
        if (ctx.ssao && *ctx.ssao)
        {
            (*ctx.ssao)->Dispatch(
                ctx.commandPool,
                (*ctx.deferred)->GetDepthImage(),
                (*ctx.deferred)->GetNormalImage(),
                ctx.projection->view,
                ctx.projection->unjitteredProjection,
                ctx.options->ReverseZ,
                ctx.options->ssaoRadius,
                ctx.options->ssaoBias,
                ctx.options->ssaoPower,
                ctx.options->ssaoIntensity);
            ssaoImage = (*ctx.ssao)->GetOutputImage();
        }
        else if (ctx.ssaoFallbackImage && *ctx.ssaoFallbackImage)
        {
            ssaoImage = *ctx.ssaoFallbackImage;
        }

        if (!ssaoImage)
            return;

        (*ctx.deferred)
            ->BeginLighting(ctx.commandPool, ssaoImage, ctx.frameIndex);
        SetFullViewport(ctx.commandPool, ctx.renderExtent);
        (*ctx.deferred)->DrawLighting(ctx.commandPool, ctx.frameIndex);
        (*ctx.deferred)->EndLighting(ctx.commandPool);
    }

    void TaaFrameStage::Rebuild(RenderFrameContext&   ctx,
                                skr::ServiceProvider& sp)
    {
        if (ctx.options->enableXess)
        {
            if (ctx.taa)
                ctx.taa->reset();
            if (!ctx.xess)
                return;
            // Prefer an already-built pass (Renderer::rebuildSceneResources
            // creates XeSS before other stages so renderExtent is known).
            if (!*ctx.xess)
            {
                *ctx.xess = sp.GetService<XessPassBuilder>()->Build(
                    ctx.swapChain, ctx.presentExtent.width > 0
                                       ? ctx.presentExtent
                                       : ctx.renderExtent);
            }
            if (*ctx.xess)
            {
                (*ctx.xess)->ResetHistory();
                ctx.renderExtent = (*ctx.xess)->GetInputExtent();
            }
            return;
        }

        if (ctx.xess)
            ctx.xess->reset();
        if (!ctx.taa)
            return;
        ctx.taa->reset();
        if (!ctx.options->enableTaa)
            return;
        *ctx.taa = sp.GetService<TaaPassBuilder>()->Build(ctx.swapChain,
                                                          ctx.renderExtent);
        (*ctx.taa)->ResetHistory();
    }

    void TaaFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.deferred || !*ctx.deferred)
            return;

        if (ctx.xess && *ctx.xess)
        {
            (*ctx.xess)->Dispatch(ctx.commandPool,
                                  (*ctx.deferred)->GetSceneColorImage(),
                                  (*ctx.deferred)->GetVelocityImage(),
                                  (*ctx.deferred)->GetDepthImage());
            return;
        }

        if (!ctx.taa || !*ctx.taa)
            return;

        (*ctx.taa)->Dispatch(ctx.commandPool,
                             (*ctx.deferred)->GetSceneColorImage(),
                             (*ctx.deferred)->GetVelocityImage(),
                             (*ctx.deferred)->GetDepthImage());
    }

    void BloomFrameStage::Rebuild(RenderFrameContext&   ctx,
                                  skr::ServiceProvider& sp)
    {
        if (!ctx.bloom)
            return;
        ctx.bloom->reset();
        if (!ctx.options->enableBloom || !ctx.deferred || !*ctx.deferred)
            return;
        *ctx.bloom = sp.GetService<BloomPassBuilder>()->Build(
            ctx.swapChain,
            (*ctx.deferred)->GetSceneColorImage(),
            ctx.renderExtent);
    }

    void BloomFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.bloom || !*ctx.bloom)
        {
            // Keep composite bloom tap black when bloom is disabled.
            if (ctx.bloomResultImage && *ctx.bloomResultImage)
            {
                const auto commandBuffer = ctx.commandPool->GetCommandBuffer();
                ctx.commandPool->GetDevice()->BeginDebugLabel(
                    commandBuffer, DebugLabel::BloomClear);

                const auto range =
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1);

                auto barrier =
                    vk::ImageMemoryBarrier()
                        .setOldLayout(vk::ImageLayout::eUndefined)
                        .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                        .setSrcAccessMask({})
                        .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                        .setImage((*ctx.bloomResultImage)->GetImage())
                        .setSubresourceRange(range);
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    vk::PipelineStageFlagBits::eTransfer,
                    {},
                    nullptr,
                    nullptr,
                    barrier);

                constexpr vk::ClearColorValue clearBlack { 0.f, 0.f, 0.f, 0.f };
                commandBuffer.clearColorImage(
                    (*ctx.bloomResultImage)->GetImage(),
                    vk::ImageLayout::eTransferDstOptimal,
                    clearBlack,
                    range);

                barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    {},
                    nullptr,
                    nullptr,
                    barrier);

                ctx.commandPool->GetDevice()->EndDebugLabel(commandBuffer);
            }
            return;
        }

        const auto commandBuffer = ctx.commandPool->GetCommandBuffer();
        const auto bloomExtent =
            ScaledExtent(ctx.renderExtent, ctx.options->bloomResolutionDivisor);
        const auto halfW = bloomExtent.width;
        const auto halfH = bloomExtent.height;

        auto bloomViewport =
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(halfW))
                .setHeight(static_cast<float>(halfH))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        auto bloomScissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(
            vk::Extent2D { halfW, halfH });

        commandBuffer.setViewport(0, 1, &bloomViewport);
        commandBuffer.setScissor(0, 1, &bloomScissor);

        (*ctx.bloom)->Begin(ctx.swapChain, ctx.commandPool);
        (*ctx.bloom)->DrawFullscreenTriangle(ctx.commandPool);
        (*ctx.bloom)
            ->AdvanceSubpass(BloomDownsampleSubpass, ctx.commandPool,
                             ctx.frameIndex);
        (*ctx.bloom)->DrawFullscreenTriangle(ctx.commandPool);
        (*ctx.bloom)
            ->AdvanceSubpass(BloomUpsampleSubpass, ctx.commandPool,
                             ctx.frameIndex);
        (*ctx.bloom)->DrawFullscreenTriangle(ctx.commandPool);
        (*ctx.bloom)->End(ctx.commandPool);

        if (ctx.blitBloomToFullRes)
            ctx.blitBloomToFullRes();
    }

    void CompositeFrameStage::Rebuild(RenderFrameContext&   ctx,
                                      skr::ServiceProvider& sp)
    {
        if (!ctx.composite)
            return;
        ctx.composite->reset();
        *ctx.composite =
            sp.GetService<CompositePassBuilder>()->Build(ctx.swapChain);

        if (!ctx.deferred || !*ctx.deferred || !ctx.bloomResultImage ||
            !*ctx.bloomResultImage || !ctx.bloomResultSampler)
            return;

        for (auto frame = 0; frame < ctx.options->frameCount; frame++)
        {
            (*ctx.composite)
                ->UpdateDescriptorSet(
                    frame, (*ctx.deferred)->GetSceneColorImage(),
                    (*ctx.deferred)->GetTranslucentImage(),
                    *ctx.bloomResultImage, *ctx.bloomResultSampler);
        }
    }

    void CompositeFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.deferred || !*ctx.deferred || !ctx.beginComposite)
            return;

        SetFullViewport(
            ctx.commandPool,
            ctx.presentExtent.width > 0 ? ctx.presentExtent : ctx.renderExtent);

        skr::Arc<Image> compositeOpaque = (*ctx.deferred)->GetSceneColorImage();
        if (ctx.xess && *ctx.xess)
            compositeOpaque = (*ctx.xess)->GetOutputImage();
        else if (ctx.taa && *ctx.taa)
            compositeOpaque = (*ctx.taa)->GetOutputImage();
        ctx.beginComposite(ctx.frameIndex,
                           compositeOpaque,
                           (*ctx.deferred)->GetTranslucentImage(),
                           true);
        if (ctx.commitTaaHistory)
            ctx.commitTaaHistory();
    }

} // namespace FREYA_NAMESPACE
