#include "Renderer.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
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

    Renderer::Renderer(
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
        mCompositePass(compositePass), mCommandPool(commandPool),
        mLightService(lightService), mShadowPass(shadowPass),
        mPickPass(pickPass), mServiceProvider(serviceProvider),
        mFreyaOptions(freyaOptions), mEventManager(eventManager),
        mCurrentProjection({}),
        mMeshPool(serviceProvider->GetService<MeshPool>()),
        mMaterialPool(serviceProvider->GetService<MaterialPool>())
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

        // Create bloom result image (full-res blit target)
        const auto extent = getRenderExtent();
        mBloomResultImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        // Create a linear sampler for bloom result
        mBloomResultSampler = mDevice->Get().createSampler(
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

        // Initialize composite descriptor sets for deferred rendering.
        for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
        {
            mCompositePass->UpdateDescriptorSet(
                frame, mDeferredPass->GetSceneColorImage(),
                mDeferredPass->GetTranslucentImage(), mBloomResultImage,
                mBloomResultSampler);
        }

        if (!mFreyaOptions->enableSsao)
            mSsaoFallbackImage = createSsaoFallbackImage();

        registerDefaultFrameStages();
    }

    Renderer::~Renderer()
    {
        mDevice->Get().waitIdle();

        mDevice->Get().destroySampler(mBloomResultSampler);
        mBloomResultImage.reset();
        mTaaPass.reset();
        mSsaoPass.reset();
        mBloomPass.reset();
        mCompositePass.reset();
        mSwapChain.reset();
        mSurface.reset();
        mDeferredPass.reset();
        mCommandPool.reset();
        mDevice.reset();
        mInstance.reset();
    }

    void Renderer::RebuildSwapChain()
    {
        mDevice->Get().waitIdle();

        mSwapChain.reset();
        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();

        rebuildSceneResources();
    }

    vk::Extent2D Renderer::getRenderExtent() const
    {
        if (mOutputTarget)
            return mOutputTarget->GetExtent();
        return mSwapChain->GetExtent();
    }

    void Renderer::rebuildSceneResources()
    {
        const auto extent = getRenderExtent();

        mBloomResultImage.reset();
        mBloomResultImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

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
    }

    void Renderer::registerDefaultFrameStages()
    {
        mFrameStages = {
            std::make_shared<PickFrameStage>(),
            std::make_shared<ShadowFrameStage>(),
            std::make_shared<DeferredGeometryFrameStage>(),
            std::make_shared<SsaoLightingFrameStage>(),
            std::make_shared<TaaFrameStage>(),
            std::make_shared<BloomFrameStage>(),
            std::make_shared<CompositeFrameStage>(),
        };
    }

    RenderFrameContext Renderer::makeFrameContext()
    {
        RenderFrameContext ctx;
        ctx.commandPool          = mCommandPool;
        ctx.swapChain            = mSwapChain;
        ctx.options              = mFreyaOptions;
        ctx.renderExtent         = getRenderExtent();
        ctx.frameIndex           = mSwapChain->GetCurrentFrameIndex();
        ctx.cameraNear           = mCameraNear;
        ctx.projection           = &mCurrentProjection;
        ctx.deferred             = &mDeferredPass;
        ctx.ssao                 = &mSsaoPass;
        ctx.taa                  = &mTaaPass;
        ctx.bloom                = &mBloomPass;
        ctx.composite            = &mCompositePass;
        ctx.shadow               = &mShadowPass;
        ctx.pick                 = &mPickPass;
        ctx.lights               = &mLightService;
        ctx.bloomResultImage     = &mBloomResultImage;
        ctx.ssaoFallbackImage    = &mSsaoFallbackImage;
        ctx.bloomResultSampler   = &mBloomResultSampler;
        ctx.outputTarget         = &mOutputTarget;
        ctx.pickRequested        = &mPickRequested;
        ctx.pickX                = &mPickX;
        ctx.pickY                = &mPickY;
        ctx.pickAwaitingReadback = &mPickAwaitingReadback;

        ctx.executeDraws = [this](bool bindMaterials, bool shadowCastersOnly) {
            ExecuteDrawCommands(bindMaterials, shadowCastersOnly);
        };
        ctx.executePickDraws   = [this]() { ExecutePickDrawCommands(); };
        ctx.blitBloomToFullRes = [this]() { blitBloomToFullRes(mCommandPool); };
        ctx.beginComposite =
            [this](std::uint32_t frameIndex, const skr::Arc<Image>& opaque,
                   const skr::Arc<Image>& translucent, bool tonemapHdr) {
                beginComposite(frameIndex, opaque, translucent, tonemapHdr);
            };
        ctx.commitTaaHistory = [this]() { commitTaaHistory(); };
        ctx.resizePickPass   = [this](vk::Extent2D extent) {
            resizePickPass(extent);
        };
        ctx.createSsaoFallback = [this]() { return createSsaoFallbackImage(); };
        return ctx;
    }

    skr::Arc<Image> Renderer::createSsaoFallbackImage() const
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

    bool Renderer::InsertFrameStage(const char* beforeName, FrameStagePtr stage)
    {
        if (!stage || !beforeName)
            return false;

        const auto it = std::ranges::find_if(
            mFrameStages, [&](const std::shared_ptr<IFrameStage>& s) {
                return std::strcmp(s->Name(), beforeName) == 0;
            });
        if (it == mFrameStages.end())
            return false;

        mFrameStages.insert(it, std::move(stage));
        return true;
    }

    bool Renderer::ReplaceFrameStage(const char* name, FrameStagePtr stage)
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

    void Renderer::resizePickPass(const vk::Extent2D extent)
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

    void Renderer::SetShadowQuality(const ShadowQuality quality)
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

    void Renderer::SetSsaoQuality(const SsaoQuality quality)
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

    void Renderer::SetTaaQuality(const TaaQuality quality)
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

    void Renderer::SetBloomQuality(const BloomQuality quality)
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

    void Renderer::SetOutputTarget(const skr::Arc<RenderTarget>& target)
    {
        mDevice->Get().waitIdle();
        mOutputTarget = target;
        rebuildSceneResources();
    }

    void Renderer::ClearOutputTarget()
    {
        mDevice->Get().waitIdle();
        mOutputTarget.reset();
        rebuildSceneResources();
    }

    void Renderer::beginUIPass()
    {
        mCompositePass->Begin(mSwapChain, mCommandPool,
                              mFreyaOptions->clearColor, DebugLabel::UI);

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

    void Renderer::beginComposite(const std::uint32_t    frameIndex,
                                  const skr::Arc<Image>& opaqueImage,
                                  const skr::Arc<Image>& translucentImage,
                                  const bool             tonemapHdr)
    {
        mCompositePass->UpdateDescriptorSet(
            frameIndex,
            opaqueImage,
            translucentImage,
            mBloomResultImage,
            mBloomResultSampler);

        if (mOutputTarget)
        {
            mCompositePass->Begin(mOutputTarget->GetRenderPass(),
                                  mOutputTarget->GetFramebuffer(),
                                  mOutputTarget->GetExtent(),
                                  mCommandPool,
                                  mFreyaOptions->clearColor);
        }
        else
        {
            mCompositePass->Begin(mSwapChain, mCommandPool,
                                  mFreyaOptions->clearColor);
        }

        mCompositePass->BindPipeline(mCommandPool, frameIndex);
        mCompositePass->DrawFullscreenTriangle(
            mCommandPool, tonemapHdr ? 1.0f : 0.0f);
        mCompositePass->End(mCommandPool);

        if (mOutputTarget)
            beginUIPass();
    }

    vk::RenderPass Renderer::GetUIRenderPass()
    {
        return mCompositePass->GetRenderPass();
    }

    vk::CommandBuffer Renderer::GetCommandBuffer()
    {
        return mCommandPool->GetCommandBuffer();
    }

    void Renderer::SetVSync(const bool vSync)
    {
        mFreyaOptions->vSync = vSync;
        RebuildSwapChain();
    }

    void Renderer::SetSamples(const std::uint32_t samples)
    {
        mFreyaOptions->sampleCount = samples;
        RebuildSwapChain();
    }

    void Renderer::SetDrawDistance(const float drawDistance)
    {
        mFreyaOptions->drawDistance = drawDistance;
        ClearProjections();
    }

    glm::mat4 Renderer::MakeProjection(const float fovRadians,
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

    void Renderer::ClearProjections()
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
    }

    glm::mat4 Renderer::CalculateProjectionMatrix(const float near,
                                                  const float far) const
    {
        const auto extent = mSurface->QueryExtent();
        return MakeProjection(glm::radians(75.0f),
                              static_cast<float>(extent.width) /
                                  static_cast<float>(extent.height),
                              near,
                              far);
    }

    ProjectionUniformBuffer Renderer::prepareDeferredProjection(
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

    void Renderer::commitTaaHistory()
    {
        mPrevViewProjection =
            mCurrentProjection.projection * mCurrentProjection.view;
        if (mFreyaOptions->enableTaa)
            ++mTaaFrameIndex;
    }

    void Renderer::UpdateProjection(
        ProjectionUniformBuffer& projectionUniformBuffer)
    {
        const auto frameIndex = mSwapChain->GetCurrentFrameIndex();
        mDeferredPass->UpdateProjection(
            prepareDeferredProjection(projectionUniformBuffer), frameIndex);
        mCurrentProjection = projectionUniformBuffer;
    }

    void Renderer::UpdateCamera(const glm::vec3& position,
                                const glm::vec3& target,
                                const glm::vec3& up)
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

    void Renderer::SetAmbient(const glm::vec3& color, float intensity)
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

    void Renderer::blitBloomToFullRes(
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::BloomBlit);

        auto       bloomUpImage = mBloomPass->GetBloomUpImage();
        const auto extent       = getRenderExtent();
        const auto halfW        = static_cast<int32_t>(extent.width / 2);
        const auto halfH        = static_cast<int32_t>(extent.height / 2);

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
                .setImage(mBloomResultImage->GetImage())
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

        // Blit: half-res bloom up → full-res bloom result
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
                                       vk::Offset3D { halfW, halfH, 1 } };
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
            mBloomResultImage->GetImage(), vk::ImageLayout::eTransferDstOptimal,
            blitRegions, vk::Filter::eLinear);

        // Transition bloom result to shader read-only
        auto finalBarrier =
            vk::ImageMemoryBarrier()
                .setImage(mBloomResultImage->GetImage())
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

    vk::PipelineLayout Renderer::GetActivePipelineLayout() const
    {
        return mDeferredPass->GetVertexPipelineLayout();
    }

    vk::RenderPass Renderer::GetActiveRenderPass() const
    {
        return mDeferredPass->GetRenderPass();
    }

    void Renderer::UpdateModel(const glm::mat4& model) const
    {
        auto layout = GetActivePipelineLayout();
        mCommandPool->GetCommandBuffer().pushConstants(
            layout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(model),
            &model);
    }

    void Renderer::SetInstanceModels(const glm::mat4*  models,
                                     const std::size_t count)
    {
        if (count == 0 || models == nullptr)
        {
            mInstanceTransforms.clear();
            mInstanceTransformBuffers.clear();
            mInstanceTransformBuffer.reset();
            mInstanceBufferCapacity = 0;
            mInstanceHistoryFrame   = std::numeric_limits<std::uint32_t>::max();
            return;
        }

        const auto frameIndex = GetCurrentFrameIndex();
        const bool newFrame   = frameIndex != mInstanceHistoryFrame;
        mInstanceHistoryFrame = frameIndex;

        if (mInstanceTransforms.size() != count)
        {
            mInstanceTransforms.resize(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                mInstanceTransforms[i].model     = models[i];
                mInstanceTransforms[i].prevModel = models[i];
            }
        }
        else
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if (newFrame)
                {
                    mInstanceTransforms[i].prevModel =
                        mInstanceTransforms[i].model;
                }
                mInstanceTransforms[i].model = models[i];
            }
        }

        const auto frameCount = GetFrameCount();
        const auto bytes      = sizeof(InstanceTransform) * count;
        const bool rebuildBuffers =
            mInstanceTransformBuffers.size() != frameCount ||
            mInstanceBufferCapacity != count;

        if (rebuildBuffers)
        {
            mInstanceTransformBuffers.clear();
            mInstanceTransformBuffers.reserve(frameCount);
            for (std::uint32_t i = 0; i < frameCount; ++i)
            {
                mInstanceTransformBuffers.push_back(
                    GetBufferBuilder()
                        .SetData(mInstanceTransforms.data())
                        .SetSize(bytes)
                        .SetUsage(BufferUsage::Instance)
                        .Build());
            }
            mInstanceBufferCapacity = count;
        }

        auto& frameBuffer = mInstanceTransformBuffers[frameIndex];
        frameBuffer->Copy(mInstanceTransforms.data(), bytes);
        mInstanceTransformBuffer = frameBuffer;
        BindBuffer(mInstanceTransformBuffer);
    }

    BufferBuilder Renderer::GetBufferBuilder() const
    {
        return BufferBuilder(mDevice);
    }

    RenderTargetBuilder Renderer::GetRenderTargetBuilder() const
    {
        return RenderTargetBuilder(mDevice, mSurface, mServiceProvider);
    }

    void Renderer::BindBuffer(const skr::Arc<Buffer>& buffer) const
    {
        buffer->Bind(mCommandPool);
    }

    void Renderer::BindMaterial(const std::uint32_t materialId)
    {
        auto material = mMaterialPool->GetMaterial(materialId);

        mCommandPool->GetCommandBuffer().bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            GetActivePipelineLayout(),
            1,
            material.descriptorSets,
            nullptr);
    }

    void Renderer::Draw(const std::uint32_t meshId,
                        const std::uint32_t materialId,
                        const std::uint32_t entityId,
                        const bool          castShadows)
    {
        mDrawCommands.push_back(
            { meshId, materialId, 1, 0, entityId, castShadows });
    }

    void Renderer::DrawInstanced(const std::uint32_t meshId,
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

    void Renderer::ClearDrawCommands()
    {
        mDrawCommands.clear();
    }

    void Renderer::ExecuteDrawCommands(const bool bindMaterials,
                                       const bool shadowCastersOnly)
    {
        if (mInstanceTransformBuffer)
        {
            BindBuffer(mInstanceTransformBuffer);
        }

        for (const auto& cmd : mDrawCommands)
        {
            if (shadowCastersOnly && !cmd.castShadows)
            {
                continue;
            }
            if (bindMaterials)
            {
                BindMaterial(cmd.materialId);
            }
            mMeshPool->DrawInstanced(
                cmd.meshId, cmd.instanceCount, cmd.firstInstance);
        }
    }

    void Renderer::ExecutePickDrawCommands()
    {
        if (!mPickPass)
        {
            return;
        }

        if (mInstanceTransformBuffer)
        {
            BindBuffer(mInstanceTransformBuffer);
        }

        for (const auto& cmd : mDrawCommands)
        {
            mPickPass->PushEntityId(mCommandPool, cmd.entityId);
            mMeshPool->DrawInstanced(
                cmd.meshId, cmd.instanceCount, cmd.firstInstance);
        }
    }

    void Renderer::RequestPick(const std::uint32_t x, const std::uint32_t y)
    {
        mPickRequested = true;
        mPickX         = x;
        mPickY         = y;
    }

    bool Renderer::TryConsumePickResult(std::uint32_t& outEntityId)
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

    void Renderer::BeginFrame()
    {
        mSwapChain->WaitNextFrame();
        mDrawCommands.clear();

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

    void Renderer::EndScene()
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

    void Renderer::Present()
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

    void Renderer::EndFrame()
    {
        EndScene();
        Present();
    }

    void Renderer::NextSubpass()
    {
        mDeferredPass->NextSubpass(mCommandPool);
    }

    void Renderer::BindSubpass(const std::uint32_t subpass)
    {
        mDeferredPass->BindPipeline(
            subpass, mCommandPool, mSwapChain->GetCurrentFrameIndex());
    }

    void Renderer::AdvanceSubpass(const std::uint32_t subpass)
    {
        mDeferredPass->AdvanceSubpass(
            subpass, mCommandPool, mSwapChain->GetCurrentFrameIndex());
    }

} // namespace FREYA_NAMESPACE
