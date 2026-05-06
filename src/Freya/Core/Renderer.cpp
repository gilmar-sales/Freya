#include "Renderer.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <cmath>

namespace FREYA_NAMESPACE
{
    Renderer::Renderer(
        const Ref<Instance>&               instance,
        const Ref<Surface>&                surface,
        const Ref<PhysicalDevice>&         physicalDevice,
        const Ref<Device>&                 device,
        const Ref<SwapChain>&              swapChain,
        const Ref<RenderPass>&             forwardPass,
        const Ref<DeferredCompressedPass>& deferredPass,
        const Ref<BloomPass>&              bloomPass,
        const Ref<CompositePass>&          compositePass,
        const Ref<CommandPool>&            commandPool,
        const Ref<LightService>&           lightService,
        const Ref<skr::ServiceProvider>&   serviceProvider,
        const Ref<FreyaOptions>&           freyaOptions,
        const Ref<EventManager>&           eventManager) :
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mSwapChain(swapChain), mForwardPass(forwardPass),
        mDeferredPass(deferredPass), mBloomPass(bloomPass),
        mCompositePass(compositePass), mCommandPool(commandPool),
        mLightService(lightService), mServiceProvider(serviceProvider),
        mFreyaOptions(freyaOptions), mEventManager(eventManager),
        mCurrentProjection({})
    {
        ClearProjections();

        mEventManager->Subscribe<WindowResizeEvent>(
            [this](WindowResizeEvent event) {
                if (!event.handled)
                {
                    mResizeEvent = event;
                }
            });

        // Create bloom result image (full-res blit target)
        const auto extent = mSwapChain->GetExtent();
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

        // Initialize composite descriptor sets for all frames
        for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
        {
            mCompositePass->UpdateDescriptorSet(
                frame, mDeferredPass->GetOpaqueImage(),
                mDeferredPass->GetTranslucentImage(), mBloomResultImage,
                mBloomResultSampler);
        }
    }

    Renderer::~Renderer()
    {
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

    void Renderer::RebuildSwapChain()
    {
        mDevice->Get().waitIdle();

        mSwapChain.reset();
        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();

        if (IsDeferred())
        {
            mDeferredPass.reset();
            mDeferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain);

            mBloomPass.reset();
            mBloomPass =
                mServiceProvider->GetService<BloomPassBuilder>()->Build(
                    mSwapChain, mDeferredPass->GetEmissiveImage());

            mCompositePass.reset();
            mCompositePass =
                mServiceProvider->GetService<CompositePassBuilder>()->Build(
                    mSwapChain);

            // Reinitialize composite descriptor sets with new images
            for (auto frame = 0; frame < mFreyaOptions->frameCount; frame++)
            {
                mCompositePass->UpdateDescriptorSet(
                    frame, mDeferredPass->GetOpaqueImage(),
                    mDeferredPass->GetTranslucentImage(), mBloomResultImage,
                    mBloomResultSampler);
            }
        }
    }

    void Renderer::SetVSync(const bool vSync)
    {
        mFreyaOptions->vSync = vSync;
        RebuildSwapChain();
    }

    void Renderer::SetSamples(const std::uint32_t samples)
    {
        mFreyaOptions->sampleCount = samples;
        mSwapChain.reset();
        mForwardPass.reset();

        mForwardPass =
            mServiceProvider->GetService<RenderPassBuilder>()->Build();

        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();

        if (IsDeferred())
        {
            mDeferredPass.reset();
            mDeferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain);

            mBloomPass.reset();
            mBloomPass =
                mServiceProvider->GetService<BloomPassBuilder>()->Build(
                    mSwapChain, mDeferredPass->GetEmissiveImage());

            mCompositePass.reset();
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
        }
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
            .ambientLight =
                glm::vec4(glm::normalize(glm::vec3(0.0f, 3.0f, 0.0f)), 0.5f)
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
            mDeferredPass->UpdateProjection(projectionUniformBuffer,
                                            frameIndex);
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

        if (mLightService && mLightService->HasLights())
        {
            mLightService->Update(mSwapChain->GetCurrentFrameIndex(), position);
        }
    }

    void Renderer::blitBloomToFullRes(const Ref<CommandPool>& commandPool) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

        auto       bloomUpImage = mBloomPass->GetBloomUpImage();
        const auto extent       = mSwapChain->GetExtent();
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

    void Renderer::BindBuffer(const Ref<Buffer>& buffer) const
    {
        buffer->Bind(mCommandPool);
    }

    void Renderer::BeginFrame()
    {
        mSwapChain->WaitNextFrame();

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

        // Begin the Gbuffer+Lighting pass (deferred) or forward pass
        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->Begin(mSwapChain, mCommandPool);
        }
        else
        {
            mForwardPass->Begin(mSwapChain, mCommandPool);
        }

        // Viewport and scissor
        const auto viewport =
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(static_cast<float>(mSwapChain->GetExtent().width))
                .setHeight(static_cast<float>(mSwapChain->GetExtent().height))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        commandBuffer.setViewport(0, 1, &viewport);

        const auto scissor =
            vk::Rect2D().setOffset({ 0, 0 }).setExtent(mSwapChain->GetExtent());

        commandBuffer.setScissor(0, 1, &scissor);
    }

    void Renderer::EndFrame()
    {
        if (IsDeferred() && mDeferredPass)
        {
            auto frameIndex = mSwapChain->GetCurrentFrameIndex();

            // Subpass 2: lighting (fullscreen triangle)
            mDeferredPass->AdvanceSubpass(DefLightingPass,
                                          mCommandPool,
                                          frameIndex);
            mDeferredPass->DrawFullscreenTriangle(mCommandPool);

            // Subpass 3: translucent (skip — no translucent geometry)
            mDeferredPass->AdvanceSubpass(DefTranslucentPass,
                                          mCommandPool,
                                          frameIndex);

            // End deferred Gbuffer+Lighting pass
            mDeferredPass->End(mCommandPool);

            // --- Bloom pass (half resolution) ---
            const auto extent        = mSwapChain->GetExtent();
            const auto halfW         = std::max(1u, extent.width / 2);
            const auto halfH         = std::max(1u, extent.height / 2);
            const auto commandBuffer = mCommandPool->GetCommandBuffer();

            // Viewport for half-res bloom
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

            // Subpass 0: threshold
            // Already bound in Begin()
            mBloomPass->DrawFullscreenTriangle(mCommandPool);

            // Subpass 1: downsample
            mBloomPass->AdvanceSubpass(
                BloomDownsampleSubpass, mCommandPool, frameIndex);
            mBloomPass->DrawFullscreenTriangle(mCommandPool);

            // Subpass 2: upsample
            mBloomPass->AdvanceSubpass(
                BloomUpsampleSubpass, mCommandPool, frameIndex);
            mBloomPass->DrawFullscreenTriangle(mCommandPool);

            mBloomPass->End(mCommandPool);

            // --- Blit bloom up (half res) → bloom result (full res) ---
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

            // Update descriptor set for this frame (images persist, but
            // ensure the correct frame's descriptor set points to valid
            // images)
            mCompositePass->UpdateDescriptorSet(
                frameIndex, mDeferredPass->GetOpaqueImage(),
                mDeferredPass->GetTranslucentImage(), mBloomResultImage,
                mBloomResultSampler);

            mCompositePass->Begin(mSwapChain, mCommandPool,
                                  mFreyaOptions->clearColor);
            mCompositePass->BindPipeline(mCommandPool, frameIndex);
            mCompositePass->DrawFullscreenTriangle(mCommandPool);
            mCompositePass->End(mCommandPool);
        }
        else
        {
            mForwardPass->End(mCommandPool);
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

} // namespace FREYA_NAMESPACE
