#include "Freya/Core/FrameStages.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DebugDrawPassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Builders/TranslucentPassBuilder.hpp"
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

        if (ctx.dispatchCull && ctx.projection)
        {
            const auto viewProj =
                ctx.projection->unjitteredProjection * ctx.projection->view;
            ctx.dispatchCull(viewProj, CullMode::Camera);
        }

        (*ctx.pick)->Render(ctx.commandPool, *ctx.projection,
                            ctx.executePickDraws, ctx.frameIndex);
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
                     // Stable frustum fit — never the Halton-jittered matrix.
                     ctx.projection->unjitteredProjection,
                     glm::vec3(glm::inverse(ctx.projection->view)[3]),
                     ctx.cameraNear,
                     ctx.options->drawDistance,
                     ctx.frameIndex);
        (*ctx.shadow)
            ->Render(
                ctx.commandPool,
                [&](const glm::mat4& lightVP) {
                    if (ctx.dispatchCull)
                        ctx.dispatchCull(lightVP, CullMode::Shadow);
                },
                [&]() { ctx.executeDraws(false); });
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

        if (ctx.dispatchCull && ctx.projection)
        {
            const auto viewProj =
                ctx.projection->unjitteredProjection * ctx.projection->view;
            ctx.dispatchCull(viewProj, CullMode::Camera);
        }

        (*ctx.deferred)->Begin(ctx.swapChain, ctx.commandPool);
        SetFullViewport(ctx.commandPool, ctx.renderExtent);

        auto currentSubpass = (*ctx.deferred)->GetCurrentSubpass();
        if (currentSubpass == DefDepthPrePass)
        {
            ctx.executeDraws(false);
            (*ctx.deferred)
                ->AdvanceSubpass(DefGBufferPass, ctx.commandPool,
                                 ctx.frameIndex);
            ctx.executeDraws(true);
        }
        else if (currentSubpass == DefGBufferPass)
        {
            ctx.executeDraws(true);
        }

        (*ctx.deferred)->End(ctx.commandPool);

        // Previous-frame Hi-Z for next camera cull (depth is ReadOnly).
        if (ctx.buildHiZ)
            ctx.buildHiZ();
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

        const bool ssaoDebug =
            ctx.options->ssaoDebugView != SsaoDebugView::None;

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
            ssaoImage = ctx.options->ssaoDebugView == SsaoDebugView::Raw
                            ? (*ctx.ssao)->GetRawImage()
                            : (*ctx.ssao)->GetOutputImage();
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
        (*ctx.deferred)
            ->DrawLighting(ctx.commandPool, ctx.frameIndex, ssaoDebug);
        (*ctx.deferred)->EndLighting(ctx.commandPool);
    }

    void TaaFrameStage::Rebuild(RenderFrameContext&   ctx,
                                skr::ServiceProvider& sp)
    {
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
        if (!ctx.taa || !*ctx.taa || !ctx.deferred || !*ctx.deferred)
            return;

        (*ctx.taa)->Dispatch(ctx.commandPool,
                             (*ctx.deferred)->GetSceneColorImage(),
                             (*ctx.deferred)->GetVelocityImage(),
                             (*ctx.deferred)->GetDepthImage());
    }

    void TranslucentFrameStage::Rebuild(RenderFrameContext&   ctx,
                                        skr::ServiceProvider& sp)
    {
        if (!ctx.translucent || !ctx.deferred || !*ctx.deferred)
            return;
        ctx.translucent->reset();
        *ctx.translucent = sp.GetService<TranslucentPassBuilder>()->Build(
            ctx.swapChain, (*ctx.deferred)->GetDepthImage(), ctx.renderExtent);
    }

    void TranslucentFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.translucent || !*ctx.translucent || !ctx.deferred ||
            !*ctx.deferred || !ctx.projection)
            return;

        (*ctx.translucent)->UpdateProjection(*ctx.projection, ctx.frameIndex);

        if (ctx.dispatchCull)
        {
            const auto viewProj =
                ctx.projection->unjitteredProjection * ctx.projection->view;
            ctx.dispatchCull(viewProj, CullMode::Translucent);
        }

        auto layout = (*ctx.translucent)->GetAccumulatePipelineLayout();
        if (ctx.drawPipelineLayoutOverride)
            *ctx.drawPipelineLayoutOverride = layout;

        (*ctx.translucent)->BeginAccumulate(ctx.commandPool, ctx.frameIndex);
        SetFullViewport(ctx.commandPool, ctx.renderExtent);
        if (ctx.executeDraws)
            ctx.executeDraws(true);
        (*ctx.translucent)->EndAccumulate(ctx.commandPool);

        if (ctx.drawPipelineLayoutOverride)
            *ctx.drawPipelineLayoutOverride = vk::PipelineLayout {};

        const auto opaque =
            (ctx.taa && *ctx.taa) ? (*ctx.taa)->GetOutputImage()
                                  : (*ctx.deferred)->GetSceneColorImage();
        (*ctx.translucent)->Resolve(ctx.commandPool, opaque, ctx.frameIndex);
    }

    void BloomFrameStage::Rebuild(RenderFrameContext&   ctx,
                                  skr::ServiceProvider& sp)
    {
        if (!ctx.bloom)
            return;
        ctx.bloom->reset();
        if (!ctx.options->enableBloom)
            return;

        // Builder needs a valid initial HDR view; wire each frame afterwards.
        skr::Arc<Image> bloomSource;
        if (ctx.translucent && *ctx.translucent)
            bloomSource = (*ctx.translucent)->GetSceneWithTranslucency(0);
        else if (ctx.deferred && *ctx.deferred)
            bloomSource = (*ctx.deferred)->GetSceneColorImage();
        if (!bloomSource)
            return;

        *ctx.bloom = sp.GetService<BloomPassBuilder>()->Build(
            ctx.swapChain, bloomSource, ctx.renderExtent);

        if (ctx.translucent && *ctx.translucent)
        {
            for (std::uint32_t frame = 0; frame < ctx.options->frameCount;
                 ++frame)
            {
                if (auto src =
                        (*ctx.translucent)->GetSceneWithTranslucency(frame))
                    (*ctx.bloom)->SetThresholdInput(frame, src);
            }
        }
    }

    void BloomFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.bloom || !*ctx.bloom)
        {
            // Keep composite bloom tap black when bloom is disabled.
            if (ctx.bloomResultImages &&
                ctx.frameIndex < ctx.bloomResultImages->size() &&
                (*ctx.bloomResultImages)[ctx.frameIndex])
            {
                const auto& bloomResult =
                    (*ctx.bloomResultImages)[ctx.frameIndex];
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
                        .setImage(bloomResult->GetImage())
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
                    bloomResult->GetImage(),
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

        (*ctx.bloom)->Begin(ctx.commandPool, ctx.frameIndex);
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

        if (!ctx.bloomResultImages || !ctx.bloomResultSampler ||
            ctx.bloomResultImages->empty())
            return;

        for (auto frame = 0; frame < ctx.options->frameCount; frame++)
        {
            skr::Arc<Image> scene;
            if (ctx.translucent && *ctx.translucent)
                scene = (*ctx.translucent)->GetSceneWithTranslucency(frame);
            else if (ctx.deferred && *ctx.deferred)
                scene = (*ctx.deferred)->GetSceneColorImage();
            if (!scene ||
                static_cast<std::size_t>(frame) >=
                    ctx.bloomResultImages->size() ||
                !(*ctx.bloomResultImages)[frame])
                continue;
            (*ctx.composite)
                ->UpdateDescriptorSet(
                    frame, scene, (*ctx.bloomResultImages)[frame],
                    *ctx.bloomResultSampler);
        }
    }

    void CompositeFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.beginComposite)
            return;

        SetFullViewport(ctx.commandPool, ctx.renderExtent);

        skr::Arc<Image> scene;
        if (ctx.translucent && *ctx.translucent)
            scene =
                (*ctx.translucent)->GetSceneWithTranslucency(ctx.frameIndex);
        else if (ctx.taa && *ctx.taa)
            scene = (*ctx.taa)->GetOutputImage();
        else if (ctx.deferred && *ctx.deferred)
            scene = (*ctx.deferred)->GetSceneColorImage();
        if (!scene)
            return;

        ctx.beginComposite(ctx.frameIndex, scene,
                           ctx.options->ssaoDebugView == SsaoDebugView::None);
        if (ctx.commitTaaHistory)
            ctx.commitTaaHistory();
    }

    void DebugDrawFrameStage::Rebuild(RenderFrameContext&   ctx,
                                      skr::ServiceProvider& sp)
    {
        if (!ctx.debugDrawPass)
            return;
        ctx.debugDrawPass->reset();
        *ctx.debugDrawPass =
            sp.GetService<DebugDrawPassBuilder>()->Build(ctx.swapChain);
    }

    void DebugDrawFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (ctx.drawDebugOverlay)
            ctx.drawDebugOverlay();
    }

    void GpuAnimFrameStage::Execute(RenderFrameContext& ctx)
    {
        if (!ctx.gpuAnim || !*ctx.gpuAnim)
            return;
        (*ctx.gpuAnim)->Dispatch(ctx.commandPool, ctx.frameIndex);
    }

} // namespace FREYA_NAMESPACE
