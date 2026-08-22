#include "Freya/Core/FrameStages.hpp"

#include "Freya/Internal/VulkanCompat.hpp"

#include "Freya/Builders/BillboardPassBuilder.hpp"
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
#include "Freya/Core/Image.hpp"
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

    void PickFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.pick || !*ctx.pick || !ctx.pickRequested ||
            !*ctx.pickRequested)
            return;

        if (ctx.resizePickPass)
            ctx.resizePickPass(ctx.VkExtent());

        if (ctx.DispatchCull && ctx.projection)
        {
            const auto viewProj =
                ctx.projection->unjitteredProjection * ctx.projection->view;
            ctx.DispatchCull(viewProj, CullMode::Camera, kTechniqueFilterAll);
        }

        (*ctx.pick)->Render(ctx.commandPool, *ctx.projection,
                            ctx.executePickDraws, ctx.frameIndex);
        (*ctx.pick)->CopyPixel(ctx.commandPool, *ctx.pickX, *ctx.pickY);
        *ctx.pickRequested        = false;
        *ctx.pickAwaitingReadback = true;
    }

    void ShadowFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
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
                    if (ctx.DispatchCull)
                        ctx.DispatchCull(lightVP, CullMode::Shadow,
                                         kTechniqueFilterAll);
                },
                [&]() {
                    auto layout = (*ctx.shadow)->GetPipelineLayout();
                    if (ctx.drawPipelineLayoutOverride)
                        *ctx.drawPipelineLayoutOverride = layout;
                    ctx.ExecuteDraws(true, kTechniqueFilterAll);
                    if (ctx.drawPipelineLayoutOverride)
                        *ctx.drawPipelineLayoutOverride = vk::PipelineLayout {};
                });
    }

    void DeferredGeometryFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.deferred)
            return;
        ctx.deferred->reset();
        *ctx.deferred = sp.GetService<DeferredCompressedPassBuilder>()->Build(
            ctx.swapChain, ctx.VkExtent());
    }

    void DeferredGeometryFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.deferred || !*ctx.deferred)
            return;

        const glm::mat4 viewProj =
            ctx.projection
                ? ctx.projection->unjitteredProjection * ctx.projection->view
                : glm::mat4(1.0f);

        std::uint32_t usedMask = 0x1u;
        if (ctx.usedTechniqueMask)
            usedMask = *ctx.usedTechniqueMask;

        // Depth list (all opaque) + per-technique G-buffer lists.
        if (ctx.DispatchCull && ctx.projection)
        {
            ctx.DispatchCull(viewProj, CullMode::Camera, kTechniqueFilterAll);
            for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
            {
                if ((usedMask & (1u << t)) == 0)
                    continue;
                ctx.DispatchCull(viewProj, CullMode::Camera, t);
            }
        }

        (*ctx.deferred)->Begin(ctx.swapChain, ctx.commandPool);
        SetFullViewport(ctx.commandPool, ctx.VkExtent());

        auto currentSubpass = (*ctx.deferred)->GetCurrentSubpass();
        if (currentSubpass == DefDepthPrePass)
        {
            if (ctx.ExecuteDraws)
                ctx.ExecuteDraws(false, kTechniqueFilterAll);
            (*ctx.deferred)->NextSubpass(ctx.commandPool);

            bool drew = false;
            for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
            {
                if ((usedMask & (1u << t)) == 0)
                    continue;
                (*ctx.deferred)
                    ->BindGBufferTechnique(t, ctx.commandPool, ctx.frameIndex);
                if (ctx.ExecuteDraws)
                    ctx.ExecuteDraws(true, t);
                drew = true;
            }
            if (!drew)
            {
                (*ctx.deferred)
                    ->BindGBufferTechnique(0, ctx.commandPool, ctx.frameIndex);
            }
        }
        else if (currentSubpass == DefGBufferPass)
        {
            for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
            {
                if ((usedMask & (1u << t)) == 0)
                    continue;
                (*ctx.deferred)
                    ->BindGBufferTechnique(t, ctx.commandPool, ctx.frameIndex);
                if (ctx.ExecuteDraws)
                    ctx.ExecuteDraws(true, t);
            }
        }

        (*ctx.deferred)->End(ctx.commandPool);

        // Previous-frame Hi-Z for next camera cull (depth is ReadOnly).
        if (ctx.buildHiZ)
            ctx.buildHiZ();
    }

    void SsaoLightingFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.ssaoPass)
            return;
        ctx.ssaoPass->reset();
        if (ctx.options->enableSsao)
        {
            *ctx.ssaoPass = sp.GetService<SsaoPassBuilder>()->Build(
                ctx.swapChain, ctx.VkExtent());
        }
        else if (ctx.createSsaoFallback && ctx.ssaoFallbackImage)
        {
            *ctx.ssaoFallbackImage = ctx.createSsaoFallback();
        }
    }

    void SsaoLightingFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.deferred || !*ctx.deferred)
            return;

        const bool ssaoDebug =
            ctx.options->ssaoDebugView != SsaoDebugView::None;
        std::uint32_t lightingDebug = 0;
        if (ssaoDebug)
            lightingDebug = 1;
        else if (ctx.options->shadowDebug)
            lightingDebug = 2;

        skr::Arc<Image> ssaoImage;
        if (ctx.ssaoPass && *ctx.ssaoPass)
        {
            (*ctx.ssaoPass)->Dispatch(
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
                            ? (*ctx.ssaoPass)->GetRawImage()
                            : (*ctx.ssaoPass)->GetOutputImage();
        }
        else if (ctx.ssaoFallbackImage && *ctx.ssaoFallbackImage)
        {
            ssaoImage = *ctx.ssaoFallbackImage;
        }

        if (!ssaoImage)
            return;

        (*ctx.deferred)
            ->BeginLighting(ctx.commandPool, ssaoImage, ctx.frameIndex);
        SetFullViewport(ctx.commandPool, ctx.VkExtent());
        (*ctx.deferred)
            ->DrawLighting(ctx.commandPool, ctx.frameIndex, lightingDebug);
        (*ctx.deferred)->EndLighting(ctx.commandPool);
    }

    void TaaFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.taa)
            return;
        ctx.taa->reset();
        if (!ctx.options->enableTaa)
            return;
        *ctx.taa = sp.GetService<TaaPassBuilder>()->Build(ctx.swapChain,
                                                          ctx.VkExtent());
        (*ctx.taa)->ResetHistory();
    }

    void TaaFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.taa || !*ctx.taa || !ctx.deferred || !*ctx.deferred)
            return;

        (*ctx.taa)->Dispatch(ctx.commandPool,
                             (*ctx.deferred)->GetSceneColorImage(),
                             (*ctx.deferred)->GetVelocityImage(),
                             (*ctx.deferred)->GetDepthImage());
    }

    void TranslucentFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.translucent || !ctx.deferred || !*ctx.deferred)
            return;
        ctx.translucent->reset();
        *ctx.translucent = sp.GetService<TranslucentPassBuilder>()->Build(
            ctx.swapChain, (*ctx.deferred)->GetDepthImage(), ctx.VkExtent());
    }

    void TranslucentFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.translucent || !*ctx.translucent || !ctx.deferred ||
            !*ctx.deferred || !ctx.projection)
            return;

        (*ctx.translucent)->UpdateProjection(*ctx.projection, ctx.frameIndex);

        if (ctx.DispatchCull)
        {
            const auto viewProj =
                ctx.projection->unjitteredProjection * ctx.projection->view;
            ctx.DispatchCull(viewProj, CullMode::Translucent,
                             kTechniqueFilterAll);
        }

        auto layout = (*ctx.translucent)->GetAccumulatePipelineLayout();
        if (ctx.drawPipelineLayoutOverride)
            *ctx.drawPipelineLayoutOverride = layout;

        const auto opaque =
            (ctx.taa && *ctx.taa) ? (*ctx.taa)->GetOutputImage()
                                  : (*ctx.deferred)->GetSceneColorImage();

        (*ctx.translucent)
            ->BeginAccumulate(ctx.commandPool, opaque, ctx.frameIndex);
        SetFullViewport(ctx.commandPool, ctx.VkExtent());
        if (ctx.ExecuteDraws)
            ctx.ExecuteDraws(true, kTechniqueFilterAll);
        (*ctx.translucent)->EndAccumulate(ctx.commandPool);

        if (ctx.drawPipelineLayoutOverride)
            *ctx.drawPipelineLayoutOverride = vk::PipelineLayout {};

        (*ctx.translucent)->Resolve(ctx.commandPool, opaque, ctx.frameIndex);
    }

    void BillboardVfxFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.billboardPass || !ctx.deferred || !*ctx.deferred)
            return;
        ctx.billboardPass->reset();
        const auto depth = (*ctx.deferred)->GetDepthImage();
        *ctx.billboardPass =
            sp.GetService<BillboardPassBuilder>()->Build(ctx.swapChain, depth);

        std::vector<skr::Arc<Image>> colors;
        if (ctx.translucent && *ctx.translucent)
        {
            colors.reserve(ctx.options->frameCount);
            for (std::uint32_t i = 0; i < ctx.options->frameCount; ++i)
                colors.push_back(
                    (*ctx.translucent)->GetSceneWithTranslucency(i));
        }
        else if (ctx.deferred && *ctx.deferred)
        {
            colors.assign(ctx.options->frameCount,
                          (*ctx.deferred)->GetSceneColorImage());
        }
        if (*ctx.billboardPass)
            (*ctx.billboardPass)
                ->UpdateHdrTargets(colors, depth, ctx.VkExtent());
    }

    void BillboardVfxFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.billboardPass || !*ctx.billboardPass || !ctx.billboardDraw ||
            !ctx.projection)
            return;
        const auto& proj = *ctx.projection;
        (*ctx.billboardPass)
            ->Draw(ctx.commandPool, ctx.swapChain, BillboardTarget::Hdr,
                   BillboardLayer::Vfx, *ctx.billboardDraw, proj.view,
                   proj.unjitteredProjection);
    }

    void BillboardUiFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.billboardPass || !*ctx.billboardPass || !ctx.deferred ||
            !*ctx.deferred)
            return;
        (void) sp;
        (*ctx.billboardPass)
            ->UpdateLdrDepth((*ctx.deferred)->GetDepthImage(), ctx.swapChain);
    }

    void BillboardUiFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.billboardPass || !*ctx.billboardPass || !ctx.billboardDraw ||
            !ctx.projection)
            return;
        if (ctx.outputTarget && *ctx.outputTarget)
            return;
        const auto& proj = *ctx.projection;
        (*ctx.billboardPass)
            ->Draw(ctx.commandPool, ctx.swapChain, BillboardTarget::Ldr,
                   BillboardLayer::Ui, *ctx.billboardDraw, proj.view,
                   proj.unjitteredProjection);
    }

    void BloomFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
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
            ctx.swapChain, bloomSource, ctx.VkExtent());

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

    void BloomFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
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
            ScaledExtent(ctx.VkExtent(), ctx.options->bloomResolutionDivisor);
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

    void CompositeFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
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

    void CompositeFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.beginComposite)
            return;

        SetFullViewport(ctx.commandPool, ctx.VkExtent());

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
                           ctx.options->ssaoDebugView == SsaoDebugView::None &&
                               !ctx.options->shadowDebug);
        if (ctx.commitTaaHistory)
            ctx.commitTaaHistory();
    }

    void DebugDrawFrameStage::Rebuild(StageContext& stageCtx,
                                 skr::ServiceProvider& sp)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.debugDrawPass)
            return;
        ctx.debugDrawPass->reset();
        *ctx.debugDrawPass =
            sp.GetService<DebugDrawPassBuilder>()->Build(ctx.swapChain);
    }

    void DebugDrawFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (ctx.drawDebugOverlay)
            ctx.drawDebugOverlay();
    }

    void GpuAnimFrameStage::Execute(StageContext& stageCtx)
    {
        auto& ctx = AsRenderFrameContext(stageCtx);
        if (!ctx.gpuAnim || !*ctx.gpuAnim)
            return;
        (*ctx.gpuAnim)->Dispatch(ctx.commandPool, ctx.frameIndex);
    }

} // namespace FREYA_NAMESPACE
