#include "Renderer.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/Vertex.hpp"
#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/PickPassBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/ShadowDenoisePassBuilder.hpp"
#include "Freya/Builders/ShadowPassBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/ShaderModule.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <cmath>

namespace FREYA_NAMESPACE
{
    Renderer::Renderer(
        const skr::Arc<Instance>&               instance,
        const skr::Arc<Surface>&                surface,
        const skr::Arc<PhysicalDevice>&         physicalDevice,
        const skr::Arc<Device>&                 device,
        const skr::Arc<SwapChain>&              swapChain,
        const skr::Arc<RenderPass>&             forwardPass,
        const skr::Arc<DeferredCompressedPass>& deferredPass,
        const skr::Arc<BloomPass>&              bloomPass,
        const skr::Arc<CompositePass>&          compositePass,
        const skr::Arc<CommandPool>&            commandPool,
        const skr::Arc<LightService>&           lightService,
        const skr::Arc<ShadowPass>&             shadowPass,
        const skr::Arc<PickPass>&               pickPass,
        const skr::Arc<skr::ServiceProvider>&   serviceProvider,
        const skr::Arc<FreyaOptions>&           freyaOptions,
        const skr::Arc<EventManager>&           eventManager,
        const skr::Arc<Image>&                  forwardColorImage,
        const skr::Arc<Image>&                  forwardResolveImage) :
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mSwapChain(swapChain), mForwardPass(forwardPass),
        mDeferredPass(deferredPass), mBloomPass(bloomPass),
        mCompositePass(compositePass), mCommandPool(commandPool),
        mLightService(lightService), mShadowPass(shadowPass),
        mPickPass(pickPass), mServiceProvider(serviceProvider),
        mFreyaOptions(freyaOptions), mEventManager(eventManager),
        mCurrentProjection({}), mForwardColorImage(forwardColorImage),
        mForwardResolveImage(forwardResolveImage),
        mForwardOffscreenRenderPass(VK_NULL_HANDLE),
        mMeshPool(serviceProvider->GetService<MeshPool>()),
        mMaterialPool(serviceProvider->GetService<MaterialPool>())
    {
        if (freyaOptions->shadowMapResolution >= 4096)
            mShadowQuality = ShadowQuality::Ultra;
        else if (freyaOptions->shadowSampleCount <= 4 &&
                 freyaOptions->shadowMapResolution <= 512)
            mShadowQuality = ShadowQuality::Low;
        else if (freyaOptions->shadowSampleCount <= 8 &&
                 freyaOptions->shadowMapResolution <= 1024)
            mShadowQuality = ShadowQuality::Medium;
        else
            mShadowQuality = ShadowQuality::High;

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

        if (IsDeferred())
        {
            // Initialize composite descriptor sets for deferred
            for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
            {
                mCompositePass->UpdateDescriptorSet(
                    frame, mDeferredPass->GetOpaqueImage(),
                    mDeferredPass->GetTranslucentImage(), mBloomResultImage,
                    mBloomResultSampler);
            }
            rebuildShadowDenoisePass();
        }
        else
        {
            // Create offscreen resources (depth image, render pass,
            // framebuffers)
            createForwardOffscreenResources();
            createForwardPrepassResources();
            rebuildShadowDenoisePass();

            // Determine the bloom/composite input image:
            //   MSAA → use the resolve image (single sample)
            //   no MSAA → use the color image directly
            const auto compositeInput =
                mForwardResolveImage ? mForwardResolveImage
                                     : mForwardColorImage;

            // Initialize composite descriptor sets for forward
            for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
            {
                mCompositePass->UpdateDescriptorSet(
                    frame, compositeInput, compositeInput, mBloomResultImage,
                    mBloomResultSampler);
            }
        }
    }

    Renderer::~Renderer()
    {
        mDevice->Get().waitIdle();

        destroyForwardOffscreenResources();
        destroyForwardPrepassResources();
        mShadowDenoisePass.reset();

        mDevice->Get().destroySampler(mBloomResultSampler);
        mBloomResultImage.reset();
        mBloomPass.reset();
        mCompositePass.reset();
        mSwapChain.reset();
        mSurface.reset();
        mDeferredPass.reset();
        mForwardPass.reset();
        mCommandPool.reset();
        mDevice.reset();
        mInstance.reset();
    }

    void Renderer::createForwardOffscreenResources()
    {
        if (!IsDeferred())
        {
            const auto extent    = getRenderExtent();
            const auto format    = mSurface->QuerySurfaceFormat().format;
            const auto depthFmt  = mPhysicalDevice->GetDepthFormat();
            const auto vkSamples = static_cast<vk::SampleCountFlagBits>(
                mFreyaOptions->sampleCount);
            const bool msaa = vkSamples != vk::SampleCountFlagBits::e1;

            // Create forward depth image (matching MSAA setting for pipeline
            // compatibility)
            mForwardDepthImage =
                mServiceProvider->GetService<ImageBuilder>()
                    ->SetUsage(ImageUsage::Depth)
                    .SetFormat(depthFmt)
                    .SetWidth(extent.width)
                    .SetHeight(extent.height)
                    .SetSamples(vkSamples)
                    .Build();

            // Build compatible offscreen render pass.
            // Same attachment formats & sample counts as the forward render
            // pass, but color/resolve final layout is eShaderReadOnlyOptimal
            // so bloom can sample it.
            auto attachments = std::vector<vk::AttachmentDescription> {};

            if (msaa)
            {
                attachments = {
                    // 0: MSAA color (written by pipeline)
                    vk::AttachmentDescription()
                        .setFormat(format)
                        .setSamples(vkSamples)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setInitialLayout(vk::ImageLayout::eUndefined)
                        .setFinalLayout(
                            vk::ImageLayout::eColorAttachmentOptimal),
                    // 1: MSAA depth
                    vk::AttachmentDescription()
                        .setFormat(depthFmt)
                        .setSamples(vkSamples)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setInitialLayout(vk::ImageLayout::eUndefined)
                        .setFinalLayout(
                            vk::ImageLayout::eDepthStencilAttachmentOptimal),
                    // 2: Single-sample resolve (readable by bloom)
                    vk::AttachmentDescription()
                        .setFormat(format)
                        .setSamples(vk::SampleCountFlagBits::e1)
                        .setLoadOp(vk::AttachmentLoadOp::eDontCare)
                        .setStoreOp(vk::AttachmentStoreOp::eStore)
                        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setInitialLayout(vk::ImageLayout::eUndefined)
                        .setFinalLayout(
                            vk::ImageLayout::eShaderReadOnlyOptimal),
                };
            }
            else
            {
                attachments = {
                    // 0: Single-sample color (readable by bloom)
                    vk::AttachmentDescription()
                        .setFormat(format)
                        .setSamples(vk::SampleCountFlagBits::e1)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eStore)
                        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setInitialLayout(vk::ImageLayout::eUndefined)
                        .setFinalLayout(
                            vk::ImageLayout::eShaderReadOnlyOptimal),
                    // 1: Depth
                    vk::AttachmentDescription()
                        .setFormat(depthFmt)
                        .setSamples(vk::SampleCountFlagBits::e1)
                        .setLoadOp(vk::AttachmentLoadOp::eClear)
                        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                        .setInitialLayout(vk::ImageLayout::eUndefined)
                        .setFinalLayout(
                            vk::ImageLayout::eDepthStencilAttachmentOptimal),
                };
            }

            auto colorRef =
                vk::AttachmentReference().setAttachment(0).setLayout(
                    vk::ImageLayout::eColorAttachmentOptimal);

            auto depthRef =
                vk::AttachmentReference().setAttachment(1).setLayout(
                    vk::ImageLayout::eDepthStencilAttachmentOptimal);

            // Must live at this scope — subpassDesc stores a pointer to it
            auto resolveRef = vk::AttachmentReference();

            auto subpassDesc =
                vk::SubpassDescription()
                    .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                    .setColorAttachments(colorRef)
                    .setPDepthStencilAttachment(&depthRef);

            if (msaa)
            {
                resolveRef.setAttachment(2).setLayout(
                    vk::ImageLayout::eColorAttachmentOptimal);
                subpassDesc.setPResolveAttachments(&resolveRef);
            }

            auto subpasses = std::vector { subpassDesc };

            // Must match the forward render pass exactly (1 dependency) for
            // pipeline compatibility, even though the spec says dependency
            // count should not affect compatibility — validation layers check
            // it anyway.
            auto dependencies = std::vector<vk::SubpassDependency> {
                vk::SubpassDependency()
                    .setSrcSubpass(vk::SubpassExternal)
                    .setDstSubpass(0)
                    .setSrcStageMask(
                        vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eEarlyFragmentTests)
                    .setDstStageMask(
                        vk::PipelineStageFlagBits::eColorAttachmentOutput |
                        vk::PipelineStageFlagBits::eEarlyFragmentTests)
                    .setSrcAccessMask(vk::AccessFlagBits::eNone)
                    .setDstAccessMask(
                        vk::AccessFlagBits::eColorAttachmentWrite |
                        vk::AccessFlagBits::eDepthStencilAttachmentWrite),
            };

            auto renderPassInfo =
                vk::RenderPassCreateInfo()
                    .setAttachments(attachments)
                    .setSubpasses(subpasses)
                    .setDependencies(dependencies);

            mForwardOffscreenRenderPass =
                mDevice->Get().createRenderPass(renderPassInfo);

            // Create framebuffers (one per swapchain image index,
            // though they're all identical since the images are
            // fixed-size, not per-swapchain-image)
            const auto frames = mSwapChain->GetFrames();
            mForwardOffscreenFramebuffers.resize(frames.size());

            for (std::size_t i = 0; i < frames.size(); i++)
            {
                std::vector<vk::ImageView> fbViews;
                if (msaa)
                {
                    fbViews = {
                        mForwardColorImage->GetImageView(),
                        mForwardDepthImage->GetImageView(),
                        mForwardResolveImage->GetImageView(),
                    };
                }
                else
                {
                    fbViews = {
                        mForwardColorImage->GetImageView(),
                        mForwardDepthImage->GetImageView(),
                    };
                }

                auto fbInfo =
                    vk::FramebufferCreateInfo()
                        .setRenderPass(mForwardOffscreenRenderPass)
                        .setAttachments(fbViews)
                        .setWidth(extent.width)
                        .setHeight(extent.height)
                        .setLayers(1);

                mForwardOffscreenFramebuffers[i] =
                    mDevice->Get().createFramebuffer(fbInfo);
            }
        }
    }

    void Renderer::destroyForwardOffscreenResources()
    {
        if (mForwardOffscreenRenderPass)
        {
            mDevice->Get().destroyRenderPass(mForwardOffscreenRenderPass);
            mForwardOffscreenRenderPass = VK_NULL_HANDLE;
        }

        for (auto& fb : mForwardOffscreenFramebuffers)
        {
            mDevice->Get().destroyFramebuffer(fb);
        }
        mForwardOffscreenFramebuffers.clear();

        mForwardColorImage.reset();
        mForwardResolveImage.reset();
        mForwardDepthImage.reset();

        destroyForwardPrepassResources();
    }

    void Renderer::RebuildSwapChain()
    {
        mDevice->Get().waitIdle();

        destroyForwardOffscreenResources();
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

        // Tear down path-specific resources so strategy switches are clean.
        destroyForwardOffscreenResources();
        destroyForwardPrepassResources();
        mForwardColorImage.reset();
        mForwardResolveImage.reset();
        mDeferredPass.reset();
        mShadowDenoisePass.reset();
        mBloomPass.reset();
        mCompositePass.reset();

        mBloomResultImage.reset();
        mBloomResultImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        if (IsDeferred())
        {
            mDeferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain, extent);

            mBloomPass =
                mServiceProvider->GetService<BloomPassBuilder>()->Build(
                    mSwapChain, mDeferredPass->GetEmissiveImage(), extent);

            mCompositePass =
                mServiceProvider->GetService<CompositePassBuilder>()->Build(
                    mSwapChain);

            for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
            {
                mCompositePass->UpdateDescriptorSet(
                    frame, mDeferredPass->GetOpaqueImage(),
                    mDeferredPass->GetTranslucentImage(), mBloomResultImage,
                    mBloomResultSampler);
            }

            rebuildShadowDenoisePass();
        }
        else
        {
            const auto format    = mSurface->QuerySurfaceFormat().format;
            const auto vkSamples = static_cast<vk::SampleCountFlagBits>(
                mFreyaOptions->sampleCount);
            const bool msaa = vkSamples != vk::SampleCountFlagBits::e1;

            mForwardColorImage =
                mServiceProvider->GetService<ImageBuilder>()
                    ->SetUsage(ImageUsage::Color)
                    .SetFormat(format)
                    .SetWidth(extent.width)
                    .SetHeight(extent.height)
                    .SetSamples(vkSamples)
                    .Build();

            if (msaa)
            {
                mForwardResolveImage =
                    mServiceProvider->GetService<ImageBuilder>()
                        ->SetUsage(ImageUsage::Color)
                        .SetFormat(format)
                        .SetWidth(extent.width)
                        .SetHeight(extent.height)
                        .SetSamples(vk::SampleCountFlagBits::e1)
                        .Build();
            }

            const auto bloomInput =
                msaa ? mForwardResolveImage : mForwardColorImage;
            mBloomPass =
                mServiceProvider->GetService<BloomPassBuilder>()->Build(
                    mSwapChain, bloomInput, extent);

            mCompositePass =
                mServiceProvider->GetService<CompositePassBuilder>()->Build(
                    mSwapChain);

            createForwardOffscreenResources();
            createForwardPrepassResources();
            rebuildShadowDenoisePass();

            const auto compositeInput =
                mForwardResolveImage ? mForwardResolveImage
                                     : mForwardColorImage;
            for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
            {
                mCompositePass->UpdateDescriptorSet(
                    frame, compositeInput, compositeInput, mBloomResultImage,
                    mBloomResultSampler);
            }
        }

        resizePickPass(extent);
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

    void Renderer::SetRenderingStrategy(const RenderingStrategy strategy)
    {
        if (mFreyaOptions->renderingStrategy == strategy)
        {
            return;
        }

        mDevice->Get().waitIdle();
        mFreyaOptions->renderingStrategy = strategy;
        rebuildSceneResources();

        // Refresh projection/ambient UBOs into the newly built passes.
        for (std::uint32_t frameIndex = 0;
             frameIndex < mFreyaOptions->frameCount;
             ++frameIndex)
        {
            mForwardPass->UpdateProjection(mCurrentProjection, frameIndex);
            if (mDeferredPass)
            {
                mDeferredPass->UpdateProjection(mCurrentProjection, frameIndex);
            }
        }
    }

    void Renderer::SetShadowQuality(const ShadowQuality quality)
    {
        if (mShadowQuality == quality)
            return;

        ApplyShadowQuality(*mFreyaOptions, quality);
        mShadowQuality = quality;

        mDevice->Get().waitIdle();

        if (mShadowPass)
        {
            auto rebuilt =
                mServiceProvider->GetService<ShadowPassBuilder>()->Build();
            mShadowPass->StealResourcesFrom(*rebuilt);
        }

        mForwardPass.reset();
        mForwardPass =
            mServiceProvider->GetService<RenderPassBuilder>()->Build();

        rebuildSceneResources();

        for (std::uint32_t frameIndex = 0;
             frameIndex < mFreyaOptions->frameCount;
             ++frameIndex)
        {
            mForwardPass->UpdateProjection(mCurrentProjection, frameIndex);
            if (mDeferredPass)
            {
                mDeferredPass->UpdateProjection(mCurrentProjection, frameIndex);
            }
        }
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
                              mFreyaOptions->clearColor);

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
                                  const skr::Arc<Image>& translucentImage)
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
        mCompositePass->DrawFullscreenTriangle(mCommandPool);
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
        mDevice->Get().waitIdle();

        destroyForwardOffscreenResources();
        mSwapChain.reset();
        mForwardPass.reset();

        mForwardPass =
            mServiceProvider->GetService<RenderPassBuilder>()->Build();

        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();

        rebuildSceneResources();
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
            mForwardPass->UpdateProjection(projectionUniformBuffer, frameIndex);
            if (mDeferredPass)
            {
                mDeferredPass->UpdateProjection(projectionUniformBuffer,
                                                frameIndex);
            }
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

    void Renderer::UpdateProjection(
        ProjectionUniformBuffer& projectionUniformBuffer)
    {
        const auto frameIndex = mSwapChain->GetCurrentFrameIndex();
        mForwardPass->UpdateProjection(projectionUniformBuffer, frameIndex);
        if (mDeferredPass)
        {
            mDeferredPass->UpdateProjection(
                projectionUniformBuffer, frameIndex);
        }
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
            mForwardPass->UpdateProjection(mCurrentProjection, frameIndex);
            if (mDeferredPass)
            {
                mDeferredPass->UpdateProjection(mCurrentProjection, frameIndex);
            }
        }
    }

    void Renderer::blitBloomToFullRes(
        const skr::Arc<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

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
    }

    vk::PipelineLayout Renderer::GetActivePipelineLayout() const
    {
        if (IsDeferred() && mDeferredPass)
        {
            return mDeferredPass->GetVertexPipelineLayout();
        }
        return mForwardPass->GetPipelineLayout();
    }

    vk::RenderPass Renderer::GetActiveRenderPass() const
    {
        if (IsDeferred() && mDeferredPass)
        {
            return mDeferredPass->GetRenderPass();
        }
        return mForwardPass->Get();
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
        if (mPickRequested && mPickPass)
        {
            resizePickPass(getRenderExtent());
            mPickPass->Render(mCommandPool, mCurrentProjection,
                              [this]() { ExecutePickDrawCommands(); });
            mPickPass->CopyPixel(mCommandPool, mPickX, mPickY);
            mPickRequested        = false;
            mPickAwaitingReadback = true;
        }

        if (mShadowPass && mLightService)
        {
            const auto frameIndex = mSwapChain->GetCurrentFrameIndex();
            mShadowPass->Update(
                *mLightService,
                mCurrentProjection.view,
                mCurrentProjection.projection,
                glm::vec3(glm::inverse(mCurrentProjection.view)[3]),
                mCameraNear,
                mFreyaOptions->drawDistance,
                frameIndex);
            mShadowPass->Render(mCommandPool, [this]() {
                ExecuteDrawCommands(false, true);
            });
        }

        const auto commandBuffer = mCommandPool->GetCommandBuffer();
        const auto renderExtent  = getRenderExtent();
        const auto frameIndex    = mSwapChain->GetCurrentFrameIndex();

        auto setFullViewport = [&]() {
            auto viewport =
                vk::Viewport()
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<float>(renderExtent.width))
                    .setHeight(static_cast<float>(renderExtent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);
            auto scissor =
                vk::Rect2D().setOffset({ 0, 0 }).setExtent(renderExtent);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);
        };

        auto runShadowDenoise = [&]() {
            if (!mFreyaOptions->shadowDenoise || !mShadowDenoisePass)
                return;

            const auto viewPos =
                glm::vec3(glm::inverse(mCurrentProjection.view)[3]);
            const auto cameraForward =
                -glm::normalize(glm::vec3(mCurrentProjection.view[0][2],
                                          mCurrentProjection.view[1][2],
                                          mCurrentProjection.view[2][2]));
            const auto invViewProj = glm::inverse(
                mCurrentProjection.projection * mCurrentProjection.view);

            mShadowDenoisePass->Render(
                mCommandPool, invViewProj, viewPos, cameraForward,
                findDirectionalLightDirection(), frameIndex);
            bindDirectionalShadowMask();
        };

        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->Begin(mSwapChain, mCommandPool);
            setFullViewport();

            auto currentSubpass = mDeferredPass->GetCurrentSubpass();
            if (currentSubpass == DefDepthPrePass)
            {
                ExecuteDrawCommands(false);
                mDeferredPass->AdvanceSubpass(
                    DefGBufferPass, mCommandPool, frameIndex);
                ExecuteDrawCommands(true);
            }
            else if (currentSubpass == DefGBufferPass)
            {
                ExecuteDrawCommands(true);
            }

            mDeferredPass->End(mCommandPool);

            runShadowDenoise();

            setFullViewport();
            mDeferredPass->BeginLighting(mSwapChain, mCommandPool);
            setFullViewport();
            mDeferredPass->DrawLighting(mCommandPool, frameIndex);
            mDeferredPass->EndLighting(mCommandPool);

            // --- Bloom pass (half resolution) ---
            const auto extent = getRenderExtent();
            const auto halfW  = std::max(1u, extent.width / 2);
            const auto halfH  = std::max(1u, extent.height / 2);

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

            mBloomPass->Begin(mSwapChain, mCommandPool);
            mBloomPass->DrawFullscreenTriangle(mCommandPool);
            mBloomPass->AdvanceSubpass(
                BloomDownsampleSubpass, mCommandPool, frameIndex);
            mBloomPass->DrawFullscreenTriangle(mCommandPool);
            mBloomPass->AdvanceSubpass(
                BloomUpsampleSubpass, mCommandPool, frameIndex);
            mBloomPass->DrawFullscreenTriangle(mCommandPool);
            mBloomPass->End(mCommandPool);

            blitBloomToFullRes(mCommandPool);

            setFullViewport();
            beginComposite(frameIndex,
                           mDeferredPass->GetOpaqueImage(),
                           mDeferredPass->GetTranslucentImage());
        }
        else
        {
            // Forward prepass (single-sample depth + normal)
            if (mForwardPrepassRenderPass && mForwardPrepassPipeline)
            {
                auto clearValues = std::vector<vk::ClearValue> {
                    vk::ClearValue().setDepthStencil(
                        vk::ClearDepthStencilValue().setDepth(
                            mFreyaOptions->ReverseZ ? 0.0f : 1.0f)),
                    vk::ClearValue().setColor({ 0.0f, 0.0f, 0.0f, 0.0f }),
                };

                commandBuffer.beginRenderPass(
                    vk::RenderPassBeginInfo()
                        .setRenderPass(mForwardPrepassRenderPass)
                        .setFramebuffer(
                            mForwardPrepassFramebuffers
                                [mSwapChain->GetCurrentImageIndex()])
                        .setRenderArea(
                            vk::Rect2D().setOffset({ 0, 0 }).setExtent(
                                renderExtent))
                        .setClearValues(clearValues),
                    vk::SubpassContents::eInline);

                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                           mForwardPrepassPipeline);
                mForwardPass->BindDescriptorSet(mCommandPool, frameIndex);
                setFullViewport();
                ExecuteDrawCommands(false);
                commandBuffer.endRenderPass();
            }

            runShadowDenoise();

            mForwardPass->Begin(
                mForwardOffscreenRenderPass,
                mForwardOffscreenFramebuffers[mSwapChain
                                                  ->GetCurrentImageIndex()],
                getRenderExtent(),
                frameIndex,
                mCommandPool);
            setFullViewport();
            ExecuteDrawCommands(true);
            mForwardPass->End(mCommandPool);

            // --- Bloom pass (half resolution) ---
            const auto extent        = getRenderExtent();
            const auto halfW         = std::max(1u, extent.width / 2);
            const auto halfH         = std::max(1u, extent.height / 2);
            const auto commandBuffer = mCommandPool->GetCommandBuffer();

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

            mBloomPass->Begin(mSwapChain, mCommandPool);

            mBloomPass->DrawFullscreenTriangle(mCommandPool); // threshold

            mBloomPass->AdvanceSubpass(
                BloomDownsampleSubpass, mCommandPool, frameIndex);
            mBloomPass->DrawFullscreenTriangle(mCommandPool); // downsample

            mBloomPass->AdvanceSubpass(
                BloomUpsampleSubpass, mCommandPool, frameIndex);
            mBloomPass->DrawFullscreenTriangle(mCommandPool); // upsample

            mBloomPass->End(mCommandPool);

            // --- Blit bloom up (half res) -> bloom result (full res) ---
            blitBloomToFullRes(mCommandPool);

            // --- Composite pass (full resolution) ---
            auto fullViewport =
                vk::Viewport()
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<float>(extent.width))
                    .setHeight(static_cast<float>(extent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);

            auto fullScissor =
                vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent);

            commandBuffer.setViewport(0, 1, &fullViewport);
            commandBuffer.setScissor(0, 1, &fullScissor);

            const auto compositeInput =
                mForwardResolveImage ? mForwardResolveImage
                                     : mForwardColorImage;

            beginComposite(frameIndex, compositeInput, compositeInput);
        }
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
        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->NextSubpass(mCommandPool);
        }
    }

    void Renderer::BindSubpass(const std::uint32_t subpass)
    {
        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->BindPipeline(
                subpass, mCommandPool, mSwapChain->GetCurrentFrameIndex());
        }
    }

    void Renderer::AdvanceSubpass(const std::uint32_t subpass)
    {
        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->AdvanceSubpass(
                subpass, mCommandPool, mSwapChain->GetCurrentFrameIndex());
        }
    }

    glm::vec3 Renderer::findDirectionalLightDirection() const
    {
        if (!mLightService)
            return glm::vec3(0.0f, -1.0f, 0.0f);

        for (std::uint32_t i = 0; i < mLightService->GetLightCount(); ++i)
        {
            const auto* light = mLightService->GetLight(i);
            if (light && light->type >= 0.5f && light->type < 1.5f)
            {
                return light->direction;
            }
        }
        return glm::vec3(0.0f, -1.0f, 0.0f);
    }

    void Renderer::bindDirectionalShadowMask()
    {
        if (!mShadowDenoisePass)
            return;

        const auto mask    = mShadowDenoisePass->GetResult();
        const auto sampler = mShadowDenoisePass->GetSampler();

        if (mDeferredPass)
            mDeferredPass->UpdateDirectionalShadowMask(mask, sampler);
        if (mForwardPass)
            mForwardPass->UpdateDirectionalShadowMask(mask, sampler);
    }

    void Renderer::rebuildShadowDenoisePass()
    {
        mShadowDenoisePass.reset();

        if (!mFreyaOptions->shadowDenoise || !mShadowPass)
            return;

        skr::Arc<Image> depth;
        skr::Arc<Image> normal;

        if (IsDeferred() && mDeferredPass)
        {
            depth  = mDeferredPass->GetDepthImage();
            normal = mDeferredPass->GetNormalImage();
        }
        else if (mForwardPrepassDepthImage && mForwardNormalImage)
        {
            depth  = mForwardPrepassDepthImage;
            normal = mForwardNormalImage;
        }
        else
        {
            return;
        }

        mShadowDenoisePass =
            mServiceProvider->GetService<ShadowDenoisePassBuilder>()->Build(
                mSwapChain, depth, normal, getRenderExtent());
        bindDirectionalShadowMask();
    }

    void Renderer::destroyForwardPrepassResources()
    {
        if (mForwardPrepassPipeline)
        {
            mDevice->Get().destroyPipeline(mForwardPrepassPipeline);
            mForwardPrepassPipeline = VK_NULL_HANDLE;
        }
        if (mForwardPrepassRenderPass)
        {
            mDevice->Get().destroyRenderPass(mForwardPrepassRenderPass);
            mForwardPrepassRenderPass = VK_NULL_HANDLE;
        }
        for (auto& fb : mForwardPrepassFramebuffers)
            mDevice->Get().destroyFramebuffer(fb);
        mForwardPrepassFramebuffers.clear();
        mForwardPrepassDepthImage.reset();
        mForwardNormalImage.reset();
    }

    void Renderer::createForwardPrepassResources()
    {
        destroyForwardPrepassResources();
        if (IsDeferred())
            return;

        const auto extent   = getRenderExtent();
        const auto depthFmt = mPhysicalDevice->GetDepthFormat();

        mForwardPrepassDepthImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Depth)
                .SetFormat(depthFmt)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        mForwardNormalImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Color)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        auto attachments = std::vector<vk::AttachmentDescription> {
            vk::AttachmentDescription()
                .setFormat(depthFmt)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal),
            vk::AttachmentDescription()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
        };

        auto depthRef = vk::AttachmentReference().setAttachment(0).setLayout(
            vk::ImageLayout::eDepthStencilAttachmentOptimal);
        auto colorRef = vk::AttachmentReference().setAttachment(1).setLayout(
            vk::ImageLayout::eColorAttachmentOptimal);

        auto subpass =
            vk::SubpassDescription()
                .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
                .setColorAttachments(colorRef)
                .setPDepthStencilAttachment(&depthRef);

        auto dependency =
            vk::SubpassDependency()
                .setSrcSubpass(vk::SubpassExternal)
                .setDstSubpass(0)
                .setSrcStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setDstStageMask(
                    vk::PipelineStageFlagBits::eColorAttachmentOutput |
                    vk::PipelineStageFlagBits::eEarlyFragmentTests)
                .setSrcAccessMask(vk::AccessFlagBits::eNone)
                .setDstAccessMask(
                    vk::AccessFlagBits::eColorAttachmentWrite |
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite);

        mForwardPrepassRenderPass = mDevice->Get().createRenderPass(
            vk::RenderPassCreateInfo()
                .setAttachments(attachments)
                .setSubpasses(subpass)
                .setDependencies(dependency));

        auto loadShader = [&](const std::string& path) {
            return mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath(path)
                .Build();
        };
        auto vert = loadShader("./Resources/Shaders/Forward/prepass.vert.spv");
        auto frag = loadShader("./Resources/Shaders/Forward/prepass.frag.spv");

        auto stages = std::array {
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eVertex)
                .setModule(vert->Get())
                .setPName("main"),
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eFragment)
                .setModule(frag->Get())
                .setPName("main"),
        };

        auto vertexBinding    = Vertex::GetBindingDescription();
        auto vertexAttributes = Vertex::GetAttributesDescription();
        auto vertexInput =
            vk::PipelineVertexInputStateCreateInfo()
                .setVertexBindingDescriptions(vertexBinding)
                .setVertexAttributeDescriptions(vertexAttributes);

        auto inputAssembly =
            vk::PipelineInputAssemblyStateCreateInfo().setTopology(
                vk::PrimitiveTopology::eTriangleList);

        auto viewportState = vk::PipelineViewportStateCreateInfo()
                                 .setViewportCount(1)
                                 .setScissorCount(1);

        auto rasterizer =
            vk::PipelineRasterizationStateCreateInfo()
                .setPolygonMode(vk::PolygonMode::eFill)
                .setCullMode(vk::CullModeFlagBits::eBack)
                .setFrontFace(vk::FrontFace::eCounterClockwise)
                .setLineWidth(1.0f);

        auto multisampling =
            vk::PipelineMultisampleStateCreateInfo().setRasterizationSamples(
                vk::SampleCountFlagBits::e1);

        auto depthStencil =
            vk::PipelineDepthStencilStateCreateInfo()
                .setDepthTestEnable(true)
                .setDepthWriteEnable(true)
                .setDepthCompareOp(mFreyaOptions->ReverseZ
                                       ? vk::CompareOp::eGreater
                                       : vk::CompareOp::eLess);

        auto colorBlendAttachment =
            vk::PipelineColorBlendAttachmentState()
                .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                   vk::ColorComponentFlagBits::eG |
                                   vk::ColorComponentFlagBits::eB |
                                   vk::ColorComponentFlagBits::eA)
                .setBlendEnable(false);
        auto colorBlending = vk::PipelineColorBlendStateCreateInfo()
                                 .setAttachmentCount(1)
                                 .setPAttachments(&colorBlendAttachment);

        auto dynamicStates = std::vector { vk::DynamicState::eViewport,
                                           vk::DynamicState::eScissor };
        auto dynamicState =
            vk::PipelineDynamicStateCreateInfo().setDynamicStates(
                dynamicStates);

        mForwardPrepassPipeline =
            mDevice->Get()
                .createGraphicsPipeline(
                    nullptr,
                    vk::GraphicsPipelineCreateInfo()
                        .setStages(stages)
                        .setPVertexInputState(&vertexInput)
                        .setPInputAssemblyState(&inputAssembly)
                        .setPViewportState(&viewportState)
                        .setPRasterizationState(&rasterizer)
                        .setPMultisampleState(&multisampling)
                        .setPDepthStencilState(&depthStencil)
                        .setPColorBlendState(&colorBlending)
                        .setPDynamicState(&dynamicState)
                        .setLayout(mForwardPass->GetPipelineLayout())
                        .setRenderPass(mForwardPrepassRenderPass)
                        .setSubpass(0))
                .value;

        mDevice->Get().destroyShaderModule(vert->Get());
        mDevice->Get().destroyShaderModule(frag->Get());

        const auto frames = mSwapChain->GetFrames();
        mForwardPrepassFramebuffers.resize(frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            auto views = std::vector {
                mForwardPrepassDepthImage->GetImageView(),
                mForwardNormalImage->GetImageView(),
            };
            mForwardPrepassFramebuffers[i] = mDevice->Get().createFramebuffer(
                vk::FramebufferCreateInfo()
                    .setRenderPass(mForwardPrepassRenderPass)
                    .setAttachments(views)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1));
        }
    }

} // namespace FREYA_NAMESPACE
