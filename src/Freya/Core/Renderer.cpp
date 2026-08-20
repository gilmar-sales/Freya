#include "Freya/Internal/RendererImpl.hpp"

#include "Freya/Internal/VulkanCompat.hpp"

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/GpuAnimPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/IndirectDrawSystemBuilder.hpp"
#include "Freya/Builders/PickPassBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Builders/ShadowPassBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/DebugLabels.hpp"
#include "Freya/Core/FrameStages.hpp"
#include "Freya/Core/IndirectDrawSystem.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        float Halton(std::uint32_t index, const std::uint32_t base)
        {
            float f      = 1.0f;
            float result = 0.0f;
            while (index > 0)
            {
                f /= static_cast<float>(base);
                result += f * static_cast<float>(index % base);
                index /= base;
            }
            return result;
        }

        void ApplyHaltonJitter(glm::mat4&          projection,
                               const std::uint32_t frameIndex,
                               const vk::Extent2D  extent,
                               const std::uint32_t haltonPeriod)
        {
            if (extent.width == 0 || extent.height == 0)
                return;

            // Halton ∈ [0,1] → pixel offset ∈ [-0.5, 0.5]. Map to NDC
            // (clip xy ∈ [-1,1]) via *2/resolution. Y is negated: Vulkan
            // NDC Y points down and MakeProjection flips proj[1][1].
            const auto  period = std::max(1u, haltonPeriod);
            const auto  sample = (frameIndex % period) + 1;
            const float jx     = (Halton(sample, 2) - 0.5f) * 2.0f /
                                 static_cast<float>(extent.width);
            const float jy     = -(Halton(sample, 3) - 0.5f) * 2.0f /
                                 static_cast<float>(extent.height);
            projection[2][0] += jx;
            projection[2][1] += jy;
        }
    } // namespace

    Renderer::Impl::Impl(
        const skr::Arc<Instance>&               instance,
        const skr::Arc<Surface>&                surface,
        const skr::Arc<PhysicalDevice>&         physicalDevice,
        const skr::Arc<Device>&                 device,
        const skr::Arc<SwapChain>&              swapChain,
        const skr::Arc<DeferredCompressedPass>& deferredPass,
        const skr::Arc<BloomPass>&              bloomPass,
        const skr::Arc<TaaPass>&                taaPass,
        const skr::Arc<SsaoPass>&               ssaoPass,
        const skr::Arc<CompositePass>&          compositePass,
        const skr::Arc<DebugDrawPass>&          debugDrawPass,
        const skr::Arc<GpuAnimPass>&            gpuAnimPass,
        const skr::Arc<CommandPool>&            commandPool,
        const skr::Arc<LightService>&           lightService,
        const skr::Arc<ShadowPass>&             shadowPass,
        const skr::Arc<PickPass>&               pickPass,
        const skr::Arc<skr::ServiceProvider>&   serviceProvider,
        const skr::Arc<FreyaOptions>&           freyaOptions,
        const skr::Arc<EventManager>&           eventManager) :
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mSwapChain(swapChain), mDeferredPass(deferredPass),
        mBloomPass(bloomPass), mTaaPass(taaPass), mSsaoPass(ssaoPass),
        mCompositePass(compositePass), mDebugDrawPass(debugDrawPass),
        mGpuAnimPass(gpuAnimPass), mCommandPool(commandPool),
        mLightService(lightService), mShadowPass(shadowPass),
        mPickPass(pickPass), mServiceProvider(serviceProvider),
        mFreyaOptions(freyaOptions), mEventManager(eventManager),
        mCurrentProjection({}),
        mMeshPool(serviceProvider->GetService<MeshPool>()),
        mIndirectDraw(serviceProvider->GetService<IndirectDrawSystem>())
    {
        if (!freyaOptions->enableShadows)
            mShadowQuality = ShadowQuality::Off;
        else if (freyaOptions->shadowMapResolution >= 4096)
            mShadowQuality = ShadowQuality::Ultra;
        else if (freyaOptions->shadowSampleCount <= 4 &&
                 freyaOptions->shadowMapResolution <= 512)
            mShadowQuality = ShadowQuality::Low;
        else if (freyaOptions->shadowSampleCount <= 8 &&
                 freyaOptions->shadowMapResolution <= 1024)
            mShadowQuality = ShadowQuality::Medium;
        else
            mShadowQuality = ShadowQuality::High;

        if (!freyaOptions->enableSsao)
            mSsaoQuality = SsaoQuality::Off;
        if (!freyaOptions->enableTaa)
            mTaaQuality = TaaQuality::Off;
        if (!freyaOptions->enableBloom)
            mBloomQuality = BloomQuality::Off;

        if (mLightService)
            mLightService->SetShadowsEnabled(freyaOptions->enableShadows);

        ClearProjections();

        mEventManager->Subscribe<WindowResizeEvent>(
            [this](WindowResizeEvent event) {
                if (!event.handled)
                {
                    mResizeEvent = event;
                }
            });

        // Create bloom result images (full-res blit target, one per frame)
        const auto extent = getRenderExtent();
        mBloomResultImages.resize(mFreyaOptions->frameCount);
        for (std::uint32_t i = 0; i < mFreyaOptions->frameCount; ++i)
        {
            mBloomResultImages[i] =
                mServiceProvider->GetService<ImageBuilder>()
                    ->SetUsage(ImageUsage::Color)
                    .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                    .SetWidth(extent.width)
                    .SetHeight(extent.height)
                    .SetSamples(vk::SampleCountFlagBits::e1)
                    .Build();
        }

        // Create a linear sampler for bloom result
        mBloomResultSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // Initialize composite after first stage rebuild (needs translucent).
        // Descriptor sets are filled in CompositeFrameStage::Rebuild.

        if (!mFreyaOptions->enableSsao)
            mSsaoFallbackImage = createSsaoFallbackImage();

        registerDefaultFrameStages();
        rebuildSceneResources();
    }

    Renderer::Impl::~Impl()
    {
        mDevice->Get().waitIdle();

        mDevice->Get().destroySampler(mBloomResultSampler);
        mBloomResultImages.clear();
        mTaaPass.reset();
        mSsaoPass.reset();
        mTranslucentPass.reset();
        mBillboardPass.reset();
        mBloomPass.reset();
        mCompositePass.reset();
        mSwapChain.reset();
        mSurface.reset();
        mDeferredPass.reset();
        mCommandPool.reset();
        mDevice.reset();
        mInstance.reset();
    }

    void Renderer::Impl::RebuildSwapChain()
    {
        mDevice->Get().waitIdle();

        mSwapChain.reset();
        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();

        rebuildSceneResources();
    }

    vk::Extent2D Renderer::Impl::getRenderExtent() const
    {
        if (mOutputTarget)
            return mOutputTarget->GetExtent();
        return mSwapChain->GetExtent();
    }

    void Renderer::Impl::rebuildSceneResources()
    {
        const auto extent = getRenderExtent();

        mBloomResultImages.clear();
        mBloomResultImages.resize(mFreyaOptions->frameCount);
        for (std::uint32_t i = 0; i < mFreyaOptions->frameCount; ++i)
        {
            mBloomResultImages[i] =
                mServiceProvider->GetService<ImageBuilder>()
                    ->SetUsage(ImageUsage::Color)
                    .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                    .SetWidth(extent.width)
                    .SetHeight(extent.height)
                    .SetSamples(vk::SampleCountFlagBits::e1)
                    .Build();
        }

        mPrevViewProjection = glm::mat4(1.0f);
        mTaaFrameIndex      = 0;

        auto ctx = makeFrameContext();
        for (auto& stage : mFrameStages)
            stage->Rebuild(ctx, *mServiceProvider);

        if (!mFreyaOptions->enableSsao)
            mSsaoFallbackImage = createSsaoFallbackImage();
        else
            mSsaoFallbackImage.reset();

        resizePickPass(extent);
        if (mIndirectDraw)
            mIndirectDraw->ResizeHiZ(extent);
    }

    void Renderer::Impl::registerDefaultFrameStages()
    {
        mFrameStages = {
            std::make_shared<GpuAnimFrameStage>(),
            std::make_shared<PickFrameStage>(),
            std::make_shared<ShadowFrameStage>(),
            std::make_shared<DeferredGeometryFrameStage>(),
            std::make_shared<SsaoLightingFrameStage>(),
            std::make_shared<TaaFrameStage>(),
            std::make_shared<TranslucentFrameStage>(),
            std::make_shared<BillboardVfxFrameStage>(),
            std::make_shared<BloomFrameStage>(),
            std::make_shared<CompositeFrameStage>(),
            std::make_shared<BillboardUiFrameStage>(),
            std::make_shared<DebugDrawFrameStage>(),
        };
    }

    RenderFrameContext Renderer::Impl::makeFrameContext()
    {
        RenderFrameContext ctx;
        ctx.commandPool                = mCommandPool;
        ctx.swapChain                  = mSwapChain;
        ctx.options                    = mFreyaOptions;
        ctx.renderExtent               = getRenderExtent();
        ctx.frameIndex                 = mSwapChain->GetCurrentFrameIndex();
        ctx.cameraNear                 = mCameraNear;
        ctx.projection                 = &mCurrentProjection;
        ctx.deferred                   = &mDeferredPass;
        ctx.ssao                       = &mSsaoPass;
        ctx.taa                        = &mTaaPass;
        ctx.translucent                = &mTranslucentPass;
        ctx.bloom                      = &mBloomPass;
        ctx.composite                  = &mCompositePass;
        ctx.debugDrawPass              = &mDebugDrawPass;
        ctx.billboardPass              = &mBillboardPass;
        ctx.billboardDraw              = &mBillboardDraw;
        ctx.gpuAnim                    = &mGpuAnimPass;
        ctx.shadow                     = &mShadowPass;
        ctx.pick                       = &mPickPass;
        ctx.lights                     = &mLightService;
        ctx.bloomResultImages          = &mBloomResultImages;
        ctx.ssaoFallbackImage          = &mSsaoFallbackImage;
        ctx.bloomResultSampler         = &mBloomResultSampler;
        ctx.outputTarget               = &mOutputTarget;
        ctx.pickRequested              = &mPickRequested;
        ctx.pickX                      = &mPickX;
        ctx.pickY                      = &mPickY;
        ctx.pickAwaitingReadback       = &mPickAwaitingReadback;
        ctx.drawPipelineLayoutOverride = &mDrawPipelineLayoutOverride;

        ctx.executeDraws = [this](bool bindMaterials) {
            ExecuteDrawCommands(bindMaterials);
        };
        ctx.dispatchCull = [this](const glm::mat4& viewProj, CullMode mode) {
            DispatchCull(viewProj, mode);
        };
        ctx.executePickDraws = [this]() { ExecutePickDrawCommands(); };
        ctx.buildHiZ         = [this]() {
            if (!mIndirectDraw || !mDeferredPass)
                return;
            mIndirectDraw->BuildHiZ(mDeferredPass->GetDepthImage(),
                                    mFreyaOptions->ReverseZ);
        };
        ctx.blitBloomToFullRes = [this]() {
            blitBloomToFullRes(mCommandPool,
                               mSwapChain->GetCurrentFrameIndex());
        };
        ctx.beginComposite =
            [this](std::uint32_t frameIndex, const skr::Arc<Image>& scene,
                   bool tonemapHdr) {
                beginComposite(frameIndex, scene, tonemapHdr);
            };
        ctx.commitTaaHistory = [this]() { commitTaaHistory(); };
        ctx.resizePickPass   = [this](vk::Extent2D extent) {
            resizePickPass(extent);
        };
        ctx.createSsaoFallback = [this]() { return createSsaoFallbackImage(); };
        ctx.drawDebugOverlay   = [this]() {
            if (!mDebugDrawEnabled || !mDebugDrawPass || mDebugDraw.Empty())
                return;
            // ImGui/output-target path owns the swapchain; skip overlay.
            if (mOutputTarget)
                return;
            const glm::mat4 viewProj = mCurrentProjection.unjitteredProjection *
                                       mCurrentProjection.view;
            mDebugDrawPass->Draw(mSwapChain, mCommandPool,
                                 mDebugDraw.Vertices(), viewProj);
        };
        return ctx;
    }

    skr::Arc<Image> Renderer::Impl::createSsaoFallbackImage() const
    {
        constexpr std::uint8_t kWhite[] = { 255, 255, 255, 255 };
        auto                   staging =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Staging)
                .SetSize(sizeof(kWhite))
                .SetData(const_cast<std::uint8_t*>(kWhite))
                .Build();

        return mServiceProvider->GetService<ImageBuilder>()
            ->SetUsage(ImageUsage::Texture)
            .SetFormat(vk::Format::eR8G8B8A8Unorm)
            .SetWidth(1)
            .SetHeight(1)
            .SetChannels(4)
            .SetStagingBuffer(staging)
            .SetData(const_cast<std::uint8_t*>(kWhite))
            .Build();
    }

    bool Renderer::Impl::InsertFrameStage(const char*   beforeName,
                                          FrameStagePtr stage)
    {
        if (!stage || !beforeName)
            return false;

        const auto it = std::ranges::find_if(
            mFrameStages, [&](const std::shared_ptr<IFrameStage>& s) {
                return std::strcmp(s->Name(), beforeName) == 0;
            });
        if (it == mFrameStages.end())
            return false;

        auto ctx = makeFrameContext();
        stage->Rebuild(ctx, *mServiceProvider);
        mFrameStages.insert(it, std::move(stage));
        return true;
    }

    bool Renderer::Impl::ReplaceFrameStage(const char*   name,
                                           FrameStagePtr stage)
    {
        if (!stage || !name)
            return false;

        const auto it = std::ranges::find_if(
            mFrameStages, [&](const std::shared_ptr<IFrameStage>& s) {
                return std::strcmp(s->Name(), name) == 0;
            });
        if (it == mFrameStages.end())
            return false;

        *it = std::move(stage);
        return true;
    }

    void Renderer::Impl::resizePickPass(const vk::Extent2D extent)
    {
        if (!mPickPass || extent.width == 0 || extent.height == 0)
        {
            return;
        }

        if (mPickPass->GetExtent().width == extent.width &&
            mPickPass->GetExtent().height == extent.height)
        {
            return;
        }

        auto colorImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR32Uint)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        auto depthImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Depth)
                .SetFormat(mPhysicalDevice->GetDepthFormat())
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        mPickPass->Resize(extent, colorImage, depthImage);
    }

    void Renderer::Impl::SetShadowQuality(const ShadowQuality quality)
    {
        if (mShadowQuality == quality)
            return;

        ApplyShadowQuality(*mFreyaOptions, quality);
        mShadowQuality = quality;

        if (mLightService)
            mLightService->SetShadowsEnabled(mFreyaOptions->enableShadows);

        // Off only flips the runtime gate; keep existing atlas resources.
        if (quality == ShadowQuality::Off)
            return;

        mDevice->Get().waitIdle();

        if (mShadowPass)
        {
            auto rebuilt =
                mServiceProvider->GetService<ShadowPassBuilder>()->Build();
            mShadowPass->StealResourcesFrom(*rebuilt);
        }

        rebuildSceneResources();

        for (std::uint32_t frameIndex = 0;
             frameIndex < mFreyaOptions->frameCount;
             ++frameIndex)
        {
            mDeferredPass->UpdateProjection(
                prepareDeferredProjection(mCurrentProjection), frameIndex);
        }
    }

    void Renderer::Impl::SetSsaoQuality(const SsaoQuality quality)
    {
        if (mSsaoQuality == quality)
            return;

        const bool wasEnabled      = mFreyaOptions->enableSsao;
        const auto previousDivisor = mFreyaOptions->ssaoResolutionDivisor;
        ApplySsaoQuality(*mFreyaOptions, quality);
        mSsaoQuality = quality;

        const bool enabledChanged = wasEnabled != mFreyaOptions->enableSsao;
        const bool divisorChanged =
            previousDivisor != mFreyaOptions->ssaoResolutionDivisor;
        if (!enabledChanged && !divisorChanged)
            return;

        mDevice->Get().waitIdle();
        rebuildSceneResources();
    }

    void Renderer::Impl::SetSsaoDebugView(const SsaoDebugView view)
    {
        mFreyaOptions->ssaoDebugView = view;
    }

    void Renderer::Impl::SetSsaoRadius(const float radius)
    {
        mFreyaOptions->ssaoRadius = std::max(0.0f, radius);
    }

    void Renderer::Impl::SetSsaoBias(const float bias)
    {
        mFreyaOptions->ssaoBias = std::max(0.0f, bias);
    }

    void Renderer::Impl::SetSsaoPower(const float power)
    {
        mFreyaOptions->ssaoPower = std::max(0.0f, power);
    }

    void Renderer::Impl::SetSsaoIntensity(const float intensity)
    {
        mFreyaOptions->ssaoIntensity = std::max(0.0f, intensity);
    }

    void Renderer::Impl::SetTaaQuality(const TaaQuality quality)
    {
        if (mTaaQuality == quality)
            return;

        const bool wasEnabled = mFreyaOptions->enableTaa;
        ApplyTaaQuality(*mFreyaOptions, quality);
        mTaaQuality = quality;

        if (wasEnabled != mFreyaOptions->enableTaa)
        {
            mDevice->Get().waitIdle();
            rebuildSceneResources();
            return;
        }

        if (mTaaPass)
            mTaaPass->ResetHistory();
    }

    void Renderer::Impl::SetBloomQuality(const BloomQuality quality)
    {
        if (mBloomQuality == quality)
            return;

        const bool wasEnabled      = mFreyaOptions->enableBloom;
        const auto previousDivisor = mFreyaOptions->bloomResolutionDivisor;
        ApplyBloomQuality(*mFreyaOptions, quality);
        mBloomQuality = quality;

        const bool enabledChanged = wasEnabled != mFreyaOptions->enableBloom;
        const bool divisorChanged =
            previousDivisor != mFreyaOptions->bloomResolutionDivisor;
        if (!enabledChanged && !divisorChanged)
            return;

        mDevice->Get().waitIdle();
        rebuildSceneResources();
    }

    void Renderer::Impl::SetOutputTarget(const skr::Arc<RenderTarget>& target)
    {
        mDevice->Get().waitIdle();
        mOutputTarget = target;
        rebuildSceneResources();
    }

    void Renderer::Impl::ClearOutputTarget()
    {
        mDevice->Get().waitIdle();
        mOutputTarget.reset();
        rebuildSceneResources();
    }

    void Renderer::Impl::beginUIPass()
    {
        mCompositePass->Begin(
            mSwapChain, mCommandPool, ToVkClearValue(mFreyaOptions->clearColor),
            DebugLabel::UI);

        const auto extent        = mSwapChain->GetExtent();
        const auto commandBuffer = mCommandPool->GetCommandBuffer();

        auto viewport =
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

        mUIPassOpen = true;
    }

    void Renderer::Impl::beginComposite(const std::uint32_t    frameIndex,
                                        const skr::Arc<Image>& sceneImage,
                                        const bool             tonemapHdr)
    {
        mCompositePass->UpdateDescriptorSet(
            frameIndex, sceneImage, mBloomResultImages[frameIndex],
            mBloomResultSampler);

        if (mOutputTarget)
        {
            mCompositePass->Begin(mOutputTarget->GetRenderPass(),
                                  mOutputTarget->GetFramebuffer(),
                                  mOutputTarget->GetExtent(),
                                  mCommandPool,
                                  ToVkClearValue(mFreyaOptions->clearColor));
        }
        else
        {
            mCompositePass->Begin(mSwapChain, mCommandPool,
                                  ToVkClearValue(mFreyaOptions->clearColor));
        }

        mCompositePass->BindPipeline(mCommandPool, frameIndex);
        mCompositePass->DrawFullscreenTriangle(
            mCommandPool, tonemapHdr ? 1.0f : 0.0f);
        mCompositePass->End(mCommandPool);

        if (mOutputTarget)
            beginUIPass();
    }

    vk::RenderPass Renderer::Impl::GetUIRenderPass()
    {
        return mCompositePass->GetRenderPass();
    }

    vk::CommandBuffer Renderer::Impl::GetCommandBuffer()
    {
        return mCommandPool->GetCommandBuffer();
    }

    void Renderer::Impl::SetVSync(const bool vSync)
    {
        mFreyaOptions->vSync = vSync;
        RebuildSwapChain();
    }

    void Renderer::Impl::SetSamples(const std::uint32_t samples)
    {
        mFreyaOptions->sampleCount = samples;
        RebuildSwapChain();
    }

    void Renderer::Impl::SetDrawDistance(const float drawDistance)
    {
        mFreyaOptions->drawDistance = drawDistance;
        ClearProjections();
    }

    glm::mat4 Renderer::Impl::MakeProjection(const float fovRadians,
                                             const float aspect,
                                             const float near,
                                             const float far) const
    {
        auto projection = mFreyaOptions->ReverseZ
                              ? glm::perspective(fovRadians, aspect, far, near)
                              : glm::perspective(fovRadians, aspect, near, far);
        projection[1][1] *= -1.f;

        return projection;
    }

    void Renderer::Impl::ClearProjections()
    {
        const auto extent = mSurface->QueryExtent();

        constexpr auto cameraPosition = glm::vec3(0.0f, 0.0f, -10.1f);
        constexpr auto cameraMatrix   = glm::mat4(1.0f);
        const auto     cameraForward =
            glm::vec3(glm::vec4(0.0f, 0.0f, 1.0f, 0.0) * cameraMatrix);
        const auto cameraRight = glm::normalize(
            glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraForward));
        const auto cameraUp =
            glm::normalize(glm::cross(cameraForward, cameraRight));

        const auto near = 1.0f;
        const auto far  = mFreyaOptions->drawDistance;

        auto projectionUniformBuffer = ProjectionUniformBuffer {
            .view       = glm::lookAt(cameraPosition,
                                      cameraPosition + cameraForward,
                                      cameraUp),
            .projection = MakeProjection(glm::radians(45.0f),
                                         static_cast<float>(extent.width) /
                                             static_cast<float>(extent.height),
                                         near,
                                         far),
            .ambientLight = glm::vec4(mFreyaOptions->ambientColor,
                                      mFreyaOptions->ambientIntensity)
        };

        for (auto frameIndex = 0; frameIndex < mFreyaOptions->frameCount;
             frameIndex++)
        {
            mDeferredPass->UpdateProjection(
                prepareDeferredProjection(projectionUniformBuffer), frameIndex);
        }

        mCurrentProjection = projectionUniformBuffer;
        mCurrentProjection.unjitteredProjection =
            projectionUniformBuffer.projection;
    }

    glm::mat4 Renderer::Impl::CalculateProjectionMatrix(const float near,
                                                        const float far) const
    {
        const auto extent = mSurface->QueryExtent();
        return MakeProjection(glm::radians(75.0f),
                              static_cast<float>(extent.width) /
                                  static_cast<float>(extent.height),
                              near,
                              far);
    }

    ProjectionUniformBuffer Renderer::Impl::prepareDeferredProjection(
        const ProjectionUniformBuffer& unjittered) const
    {
        auto upload                 = unjittered;
        upload.prevViewProjection   = mPrevViewProjection;
        upload.unjitteredProjection = unjittered.projection;
        if (mFreyaOptions->enableTaa && mTaaPass)
            ApplyHaltonJitter(upload.projection, mTaaFrameIndex,
                              getRenderExtent(),
                              mFreyaOptions->taaHaltonPeriod);
        return upload;
    }

    void Renderer::Impl::commitTaaHistory()
    {
        mPrevViewProjection =
            mCurrentProjection.projection * mCurrentProjection.view;
        if (mFreyaOptions->enableTaa)
            ++mTaaFrameIndex;
    }

    void Renderer::Impl::UpdateProjection(
        ProjectionUniformBuffer& projectionUniformBuffer)
    {
        const auto frameIndex = mSwapChain->GetCurrentFrameIndex();
        mDeferredPass->UpdateProjection(
            prepareDeferredProjection(projectionUniformBuffer), frameIndex);
        mCurrentProjection = projectionUniformBuffer;
        // SSAO (and CPU consumers) read unjitteredProjection from this struct.
        // prepareDeferredProjection only stamps it on the GPU upload copy.
        mCurrentProjection.unjitteredProjection =
            projectionUniformBuffer.projection;
    }

    void Renderer::Impl::UpdateCamera(
        const glm::vec3& position, const glm::vec3& target, const glm::vec3& up)
    {
        auto projectionUniformBuffer = mCurrentProjection;
        projectionUniformBuffer.view = glm::lookAt(position, target, up);
        UpdateProjection(projectionUniformBuffer);

        if (mLightService)
        {
            const auto toTarget = target - position;
            const auto forward  = glm::length(toTarget) > 1e-6f
                                      ? glm::normalize(toTarget)
                                      : glm::vec3(0.0f, 0.0f, 1.0f);
            mLightService->Update(mSwapChain->GetCurrentFrameIndex(), position,
                                  forward);
        }
    }

    void Renderer::Impl::SetAmbient(const glm::vec3& color, float intensity)
    {
        mCurrentProjection.ambientLight = glm::vec4(color, intensity);
        for (std::uint32_t frameIndex = 0;
             frameIndex < mFreyaOptions->frameCount;
             ++frameIndex)
        {
            mDeferredPass->UpdateProjection(
                prepareDeferredProjection(mCurrentProjection), frameIndex);
        }
    }

    void Renderer::Impl::blitBloomToFullRes(
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::BloomBlit);

        auto bloomUpImage = mBloomPass->GetBloomUpImage(frameIndex);
        auto bloomResult  = mBloomResultImages[frameIndex];
        if (!bloomUpImage || !bloomResult)
            return;

        const auto extent = getRenderExtent();
        const auto bloomExtent =
            ScaledExtent(extent, mFreyaOptions->bloomResolutionDivisor);
        const auto srcW = static_cast<int32_t>(bloomExtent.width);
        const auto srcH = static_cast<int32_t>(bloomExtent.height);

        // Transition bloom up to transfer source
        auto srcBarrier =
            vk::ImageMemoryBarrier()
                .setImage(bloomUpImage->GetImage())
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
            nullptr, nullptr, srcBarrier);

        // Transition bloom result to transfer destination
        auto dstBarrier =
            vk::ImageMemoryBarrier()
                .setImage(bloomResult->GetImage())
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(),
            nullptr, nullptr, dstBarrier);

        // Blit: bloom resolution → full-res bloom result
        auto blitRegion =
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

        auto srcOffsets = std::array { vk::Offset3D { 0, 0, 0 },
                                       vk::Offset3D { srcW, srcH, 1 } };
        auto dstOffsets = std::array {
            vk::Offset3D { 0, 0, 0 },
            vk::Offset3D { static_cast<int32_t>(extent.width),
                           static_cast<int32_t>(extent.height), 1 }
        };

        blitRegion.setSrcOffsets(srcOffsets);
        blitRegion.setDstOffsets(dstOffsets);

        auto blitRegions = std::array { blitRegion };

        commandBuffer.blitImage(
            bloomUpImage->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
            bloomResult->GetImage(), vk::ImageLayout::eTransferDstOptimal,
            blitRegions, vk::Filter::eLinear);

        // Transition bloom result to shader read-only
        auto finalBarrier =
            vk::ImageMemoryBarrier()
                .setImage(bloomResult->GetImage())
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(),
            nullptr, nullptr, finalBarrier);

        mDevice->EndDebugLabel(commandBuffer);
    }

    vk::PipelineLayout Renderer::Impl::GetActivePipelineLayout() const
    {
        if (mDrawPipelineLayoutOverride)
            return mDrawPipelineLayoutOverride;
        return mDeferredPass->GetVertexPipelineLayout();
    }

    vk::RenderPass Renderer::Impl::GetActiveRenderPass() const
    {
        return mDeferredPass->GetRenderPass();
    }

    void Renderer::Impl::UpdateModel(const glm::mat4& model) const
    {
        auto layout = GetActivePipelineLayout();
        mCommandPool->GetCommandBuffer().pushConstants(
            layout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(model),
            &model);
    }

    void Renderer::Impl::UploadSceneInstances(
        const std::span<const SceneInstanceUpload> uploads)
    {
        mUsedUploadApi = true;
        if (mIndirectDraw)
        {
            mIndirectDraw->UploadSceneInstances(
                uploads, mSwapChain->GetCurrentFrameIndex());
        }
    }

    void Renderer::Impl::SetInstanceModels(const glm::mat4*  models,
                                           const std::size_t count)
    {
        mLegacyModels.clear();
        if (count == 0 || models == nullptr)
            return;
        mLegacyModels.assign(models, models + count);
    }

    void Renderer::Impl::UploadBoneMatrices(
        const std::span<const glm::mat4> bones)
    {
        if (auto boneResources =
                mServiceProvider->GetService<BoneMatrixResources>())
        {
            boneResources->Upload(mSwapChain->GetCurrentFrameIndex(), bones);
        }
    }

    void Renderer::Impl::RebuildGpuAnimPass()
    {
        if (!mDevice || !mServiceProvider)
            return;
        const bool wasEnabled = mGpuAnimPass && mGpuAnimPass->IsEnabled();
        mDevice->Get().waitIdle();
        mGpuAnimPass.reset();
        mGpuAnimPass =
            mServiceProvider->GetService<GpuAnimPassBuilder>()->Build();
        if (mGpuAnimPass)
            mGpuAnimPass->SetEnabled(wasEnabled);
    }

    bool Renderer::Impl::ReadbackGpuAnimBones(const std::uint32_t frameIndex,
                                              const std::uint32_t boneOffset,
                                              const std::span<glm::mat4>
                                                  out)
    {
        if (!mGpuAnimPass)
            return false;
        return mGpuAnimPass->ReadbackBones(
            mCommandPool, frameIndex, boneOffset,
            static_cast<std::uint32_t>(out.size()), out);
    }

    bool Renderer::Impl::DispatchGpuAnimImmediate(
        const std::span<const GpuAnimInstance> instances,
        const std::uint32_t                    frameIndex)
    {
        if (!mGpuAnimPass || instances.empty())
            return false;
        mGpuAnimPass->SetCopyPrevBones(false);
        mGpuAnimPass->UploadInstances(instances);
        mGpuAnimPass->SetEnabled(true);
        mGpuAnimPass->DispatchImmediate(mCommandPool, frameIndex);
        return true;
    }

    void Renderer::Impl::SetGpuAnimJointExtract(
        const std::span<const GpuJointExtractRequest> requests)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->SetJointExtractList(requests);
    }

    bool Renderer::Impl::PollGpuAnimJointExtract(
        const std::span<GpuJointExtractSample> out, std::uint32_t* outCount)
    {
        if (!mGpuAnimPass)
            return false;
        return mGpuAnimPass->PollJointExtract(
            mSwapChain->GetCurrentFrameIndex(), out, outCount);
    }

    bool Renderer::Impl::PollGpuAnimTiming(GpuAnimTimingSample& out)
    {
        out = {};
        if (!mGpuAnimPass)
            return false;
        return mGpuAnimPass->PollTiming(mSwapChain->GetCurrentFrameIndex(),
                                        out);
    }

    void Renderer::Impl::SetGpuAnimCopyPrevBones(const bool enabled)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->SetCopyPrevBones(enabled);
    }

    void Renderer::Impl::UploadGpuAnimInstances(
        const std::span<const GpuAnimInstance> instances)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->UploadInstances(instances);
    }

    void Renderer::Impl::CaptureGpuAnimDebugSnapshot(
        GpuAnimDebugSnapshot& out) const
    {
        if (mGpuAnimPass)
            mGpuAnimPass->CaptureDebugSnapshot(out);
    }

    std::uint32_t Renderer::Impl::FindGpuAnimClipSlot(
        const std::uint64_t key) const
    {
        if (!mGpuAnimPass)
            return 0xffffffffu;
        return mGpuAnimPass->FindClipSlot(key);
    }

    std::uint32_t Renderer::Impl::EnsureGpuAnimClipResident(
        const std::uint64_t key, const BakedClip& clip)
    {
        if (!mGpuAnimPass)
            return 0xffffffffu;
        return mGpuAnimPass->EnsureClipResident(key, clip);
    }

    std::uint32_t Renderer::Impl::GetGpuAnimResidentClipCount() const
    {
        if (!mGpuAnimPass)
            return 0;
        return mGpuAnimPass->ResidentClipCount();
    }

    std::uint32_t Renderer::Impl::GetGpuAnimJointsPerClipSlot() const
    {
        if (!mGpuAnimPass)
            return 0;
        return mGpuAnimPass->JointsPerClipSlot();
    }

    void Renderer::Impl::UploadGpuAnimSkeleton(const GpuSkeletonPack& skeleton)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->UploadSkeleton(skeleton);
    }

    void Renderer::Impl::ResetGpuAnimClipCache()
    {
        if (mGpuAnimPass)
            mGpuAnimPass->ResetClipCache();
    }

    bool Renderer::Impl::UploadGpuAnimClipSlot(const std::uint32_t slot,
                                               const std::uint64_t key,
                                               const BakedClip&    clip)
    {
        if (!mGpuAnimPass)
            return false;
        return mGpuAnimPass->UploadClipSlot(slot, key, clip);
    }

    void Renderer::Impl::PinGpuAnimClipSlot(const std::uint32_t slot,
                                            const bool          pinned)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->PinClipSlot(slot, pinned);
    }

    void Renderer::Impl::UploadGpuAnimBoneMask(
        const std::span<const float> weights)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->UploadBoneMask(weights);
    }

    void Renderer::Impl::UploadGpuAnimRestJoints(
        const std::span<const GpuFloatJoint> joints)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->UploadRestJoints(joints);
    }

    void Renderer::Impl::UploadGpuAnimRestJoints(
        const std::span<const GpuQuantJoint> joints)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->UploadRestJoints(joints);
    }

    void Renderer::Impl::SetGpuAnimRigIndices(
        const std::uint32_t lookJoint, const std::uint32_t ikRoot,
        const std::uint32_t ikMid, const std::uint32_t ikTip,
        const std::uint32_t rootJoint, const glm::vec3 lookLocalForward,
        const float lookMaxYawRad, const float lookMaxPitchRad)
    {
        if (mGpuAnimPass)
            mGpuAnimPass->SetRigIndices(
                lookJoint, ikRoot, ikMid, ikTip, rootJoint, lookLocalForward,
                lookMaxYawRad, lookMaxPitchRad);
    }

    BufferBuilder Renderer::Impl::GetBufferBuilder() const
    {
        return BufferBuilder(mDevice);
    }

    RenderTargetBuilder Renderer::Impl::GetRenderTargetBuilder() const
    {
        return RenderTargetBuilder(mDevice, mSurface, mServiceProvider);
    }

    void Renderer::Impl::BindBuffer(const skr::Arc<Buffer>& buffer) const
    {
        buffer->Bind(mCommandPool);
    }

    void Renderer::Impl::Draw(const std::uint32_t meshId,
                              const std::uint32_t materialId,
                              const std::uint32_t entityId,
                              const bool          castShadows)
    {
        mDrawCommands.push_back(
            { meshId, materialId, 1, 0, entityId, castShadows });
    }

    void Renderer::Impl::DrawInstanced(const std::uint32_t meshId,
                                       const std::uint32_t materialId,
                                       const size_t        instanceCount,
                                       const size_t        firstInstance,
                                       const bool          castShadows,
                                       const std::uint32_t entityId)
    {
        mDrawCommands.push_back(
            { meshId, materialId, static_cast<std::uint32_t>(instanceCount),
              static_cast<std::uint32_t>(firstInstance), entityId,
              castShadows });
    }

    void Renderer::Impl::ClearDrawCommands()
    {
        mDrawCommands.clear();
        mLegacyUploads.clear();
        mUsedUploadApi = false;
    }

    void Renderer::Impl::flushLegacyDrawCommands()
    {
        if (mUsedUploadApi || !mIndirectDraw || mDrawCommands.empty())
            return;

        mLegacyUploads.clear();
        for (const auto& cmd : mDrawCommands)
        {
            for (std::uint32_t i = 0; i < cmd.instanceCount; ++i)
            {
                const auto          instanceIndex = cmd.firstInstance + i;
                SceneInstanceUpload upload {};
                upload.meshId      = cmd.meshId;
                upload.materialId  = cmd.materialId;
                upload.entityId    = cmd.entityId;
                upload.castShadows = cmd.castShadows;
                if (instanceIndex < mLegacyModels.size())
                    upload.model = mLegacyModels[instanceIndex];
                mLegacyUploads.push_back(upload);
            }
        }

        mIndirectDraw->UploadSceneInstances(
            mLegacyUploads, mSwapChain->GetCurrentFrameIndex());
    }

    void Renderer::Impl::DispatchCull(const glm::mat4& viewProj,
                                      const CullMode   mode)
    {
        flushLegacyDrawCommands();
        if (!mIndirectDraw)
            return;

        const auto cameraPos =
            glm::vec3(glm::inverse(mCurrentProjection.view)[3]);
        mIndirectDraw->SetCullView(cameraPos, getRenderExtent());
        mIndirectDraw->DispatchCull(viewProj, mode, mFreyaOptions->ReverseZ);
    }

    void Renderer::Impl::ExecuteDrawCommands(const bool bindMaterials)
    {
        flushLegacyDrawCommands();
        if (!mIndirectDraw)
            return;

        mIndirectDraw->ExecuteDraws(bindMaterials, GetActivePipelineLayout());
    }

    void Renderer::Impl::ExecutePickDrawCommands()
    {
        flushLegacyDrawCommands();
        if (!mIndirectDraw)
            return;

        mIndirectDraw->ExecuteDraws(false, {});
    }

    void Renderer::Impl::RequestPick(const std::uint32_t x,
                                     const std::uint32_t y)
    {
        mPickRequested = true;
        mPickX         = x;
        mPickY         = y;
    }

    bool Renderer::Impl::TryConsumePickResult(std::uint32_t& outEntityId)
    {
        if (!mPickAwaitingReadback || !mPickPass)
        {
            return false;
        }

        // Editor MVP: ensure the submit that recorded CopyPixel finished.
        mDevice->Get().waitIdle();
        outEntityId           = mPickPass->ReadPixel();
        mPickAwaitingReadback = false;
        return true;
    }

    void Renderer::Impl::BeginFrame()
    {
        mSwapChain->WaitNextFrame();
        mDrawCommands.clear();
        mDebugDraw.Clear();
        mBillboardDraw.Clear();

        if (mResizeEvent.has_value())
        {
            mFreyaOptions->width  = mResizeEvent->width;
            mFreyaOptions->height = mResizeEvent->height;
            RebuildSwapChain();
            mResizeEvent.reset();
        }

        auto swapChainFrame = mSwapChain->GetNextFrame();

        while (!swapChainFrame)
        {
            RebuildSwapChain();
            swapChainFrame = mSwapChain->GetNextFrame();
        }

        mSwapChain->BeginNextFrame();
        mCommandPool->SetCommandBufferIndex(mSwapChain->GetCurrentFrameIndex());

        const auto commandBuffer = mCommandPool->GetCommandBuffer();
        commandBuffer.reset();

        constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        commandBuffer.begin(beginInfo);
    }

    void Renderer::Impl::EndScene()
    {
        auto       ctx           = makeFrameContext();
        const auto commandBuffer = mCommandPool->GetCommandBuffer();

        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Frame);
        for (auto& stage : mFrameStages)
        {
            mDevice->BeginDebugLabel(commandBuffer,
                                     DebugLabel::ForStage(stage->Name()));
            stage->Execute(ctx);
            mDevice->EndDebugLabel(commandBuffer);
        }
        mDevice->EndDebugLabel(commandBuffer);
    }

    void Renderer::Impl::Present()
    {
        if (mUIPassOpen)
        {
            mCompositePass->End(mCommandPool);
            mUIPassOpen = false;
        }

        auto commandBuffer = mCommandPool->GetCommandBuffer();
        commandBuffer.end();

        std::vector<vk::CommandBuffer> commandBuffers = { commandBuffer };

        auto presentResult = mSwapChain->Present(commandBuffers);

        if (presentResult == vk::Result::eErrorOutOfDateKHR ||
            presentResult == vk::Result::eSuboptimalKHR)
        {
            RebuildSwapChain();
        }
        else if (presentResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }

    void Renderer::Impl::EndFrame()
    {
        EndScene();
        Present();
    }

    void Renderer::Impl::NextSubpass()
    {
        mDeferredPass->NextSubpass(mCommandPool);
    }

    void Renderer::Impl::BindSubpass(const std::uint32_t subpass)
    {
        mDeferredPass->BindPipeline(
            subpass, mCommandPool, mSwapChain->GetCurrentFrameIndex());
    }

    void Renderer::Impl::AdvanceSubpass(const std::uint32_t subpass)
    {
        mDeferredPass->AdvanceSubpass(
            subpass, mCommandPool, mSwapChain->GetCurrentFrameIndex());
    }

} // namespace FREYA_NAMESPACE
