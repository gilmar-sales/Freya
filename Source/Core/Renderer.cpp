#include "Core/Renderer.hpp"

#include "Builders/BufferBuilder.hpp"
#include "Builders/ForwardPassBuilder.hpp"
#include "Builders/SwapChainBuilder.hpp"

#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include <Core/UniformBuffer.hpp>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace FREYA_NAMESPACE
{
    Renderer::~Renderer()
    {
        mSwapChain.reset();

        mSurface.reset();

        mRenderPass.reset();

        mVertexBuffer.reset();
        mIndexBuffer.reset();

        for (const auto& semaphore : mRenderFinishedSemaphores)
        {
            mDevice->Get().destroySemaphore(semaphore);
        }

        for (const auto& semaphore : mImageAvailableSemaphores)
        {
            mDevice->Get().destroySemaphore(semaphore);
        }

        for (const auto& fence : mInFlightFences)
        {
            mDevice->Get().destroyFence(fence);
        }

        mCommandPool.reset();

        mDevice.reset();

        mInstance.reset();
    }

    void Renderer::RebuildSwapChain()
    {
        mDevice->Get().waitIdle();

        const auto frameCount = mSwapChain->GetFrames().size();
        const auto extent     = mSurface->QueryExtent();

        mSwapChain.reset();

        mSwapChain =
            SwapChainBuilder()
                .SetInstance(mInstance)
                .SetSurface(mSurface)
                .SetPhysicalDevice(mPhysicalDevice)
                .SetDevice(mDevice)
                .SetRenderPass(mRenderPass)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetFrameCount(frameCount)
                .SetVSync(mVSync)
                .SetSamples(mSamples)
                .Build();
    }

    void Renderer::SetVSync(bool vSync)
    {
        mVSync = vSync;
        RebuildSwapChain();
    }

    void Renderer::SetSamples(vk::SampleCountFlagBits samples)
    {
        mSamples              = samples;
        const auto frameCount = mSwapChain->GetFrames().size();
        const auto extent     = mSurface->QueryExtent();

        mSwapChain.reset();
        mRenderPass.reset();

        mRenderPass =
            ForwardPassBuilder()
                .SetDevice(mDevice)
                .SetPhysicalDevice(mPhysicalDevice)
                .SetSurface(mSurface)
                .SetSamples(mSamples)
                .SetFrameCount(frameCount)
                .Build();

        mSwapChain =
            SwapChainBuilder()
                .SetInstance(mInstance)
                .SetSurface(mSurface)
                .SetPhysicalDevice(mPhysicalDevice)
                .SetDevice(mDevice)
                .SetRenderPass(mRenderPass)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetFrameCount(frameCount)
                .SetVSync(mVSync)
                .SetSamples(mSamples)
                .Build();
    }

    void Renderer::SetDrawDistance(const float drawDistance)
    {
        mDrawDistance = drawDistance;
        ClearProjections();
    }

    void Renderer::ClearProjections()
    {
        const auto extent = mSurface->QueryExtent();

        constexpr auto cameraPosition = glm::vec3(0.0f, 0.0f, -1000.1f);
        constexpr auto cameraMatrix   = glm::mat4(1.0f);
        constexpr auto cameraForward =
            glm::vec3(glm::vec4(0.0f, 0.0f, 1.0f, 0.0) * cameraMatrix);
        const auto cameraRight =
            glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraForward));
        const auto cameraUp = glm::normalize(glm::cross(cameraForward, cameraRight));

        auto projectionUniformBuffer = ProjectionUniformBuffer {
            .view =
                glm::lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp),
            .projection   = glm::perspectiveFov(glm::radians(45.0f),
                                                static_cast<float>(extent.width),
                                                static_cast<float>(extent.height),
                                                0.1f,
                                                mDrawDistance),
            .ambientLight = glm::vec4(glm::normalize(glm::vec3(0.0f, 3.0f, 0.0f)), 0.1f)
        };

        projectionUniformBuffer.projection[1][1] *= -1.f;

        for (auto frameIndex = 0; frameIndex < mImageAvailableSemaphores.size(); frameIndex++)
        {
            mRenderPass->UpdateProjection(projectionUniformBuffer, frameIndex);
        }

        mCurrentProjection = projectionUniformBuffer;
    }

    void Renderer::UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer)
    {
        mRenderPass->UpdateProjection(projectionUniformBuffer, mCurrentFrameIndex);
        mCurrentProjection = projectionUniformBuffer;
    }

    void Renderer::UpdateModel(const glm::mat4& model) const
    {
        mCommandPool->GetCommandBuffer().pushConstants(mRenderPass->GetPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(model), &model);
    }

    Ref<MeshPoolFactory> Renderer::GetMeshPoolFactory()
    {
        return MakeRef<MeshPoolFactory>(mDevice, mPhysicalDevice, mCommandPool);
    }

    Ref<TexturePoolFactory> Renderer::GetTexturePoolFactory()
    {
        return MakeRef<TexturePoolFactory>(mDevice, mCommandPool, mRenderPass);
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
        if (mDevice->Get().waitForFences(1,
                                         &mInFlightFences[mCurrentFrameIndex],
                                         true,
                                         UINT64_MAX) != vk::Result::eSuccess)
            throw std::runtime_error("failed to wait for fences!");

        if (mResizeEvent.has_value())
        {
            mSurface->SetWidth(mResizeEvent->width);
            mSurface->SetHeight(mResizeEvent->height);

            RebuildSwapChain();

            mResizeEvent.reset();
        }

        auto swapChainFrame =
            mSwapChain->GetNextFrame(mImageAvailableSemaphores[mCurrentFrameIndex]);

        while (!swapChainFrame)
        {
            RebuildSwapChain();
            swapChainFrame =
                mSwapChain->GetNextFrame(mImageAvailableSemaphores[mCurrentFrameIndex]);
        }

        if (mDevice->Get().resetFences(1, &mInFlightFences[mCurrentFrameIndex]) !=
            vk::Result::eSuccess)
            throw std::runtime_error("failed to reset fences!");

        mCommandPool->SetCommandBufferIndex(mCurrentFrameIndex);
        mRenderPass->SetFrameIndex(mCurrentFrameIndex);

        const auto& commandBuffer = mCommandPool->GetCommandBuffer();
        commandBuffer.reset();

        constexpr auto beginInfo = vk::CommandBufferBeginInfo();

        commandBuffer.begin(beginInfo);

        auto clearValues = {
            vk::ClearValue()
                .setColor(mClearColor),
            vk::ClearValue()
                .setDepthStencil(vk::ClearDepthStencilValue().setDepth(1.0f))
        };

        const auto renderPassBeginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass->Get())
                .setFramebuffer(swapChainFrame.frameBuffer)
                .setRenderArea(
                    vk::Rect2D()
                        .setOffset({ 0, 0 })
                        .setExtent(mSwapChain->GetExtent()))
                .setClearValues(clearValues);

        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mRenderPass->GetGraphicsPipeline());

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

        mRenderPass->BindDescriptorSet(mCommandPool, mCurrentFrameIndex);
    }

    void Renderer::EndFrame()
    {
        auto& commandBuffer = mCommandPool->GetCommandBuffer();

        commandBuffer.endRenderPass();
        commandBuffer.end();

        auto commandBuffers = { commandBuffer };

        constexpr vk::PipelineStageFlags waitStages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        auto waitSemaphores = { mImageAvailableSemaphores[mCurrentFrameIndex] };

        auto signalSemaphores = { mRenderFinishedSemaphores[mCurrentFrameIndex] };

        const auto submitInfo =
            vk::SubmitInfo()
                .setWaitSemaphores(waitSemaphores)
                .setWaitDstStageMask(waitStages)
                .setCommandBuffers(commandBuffers)
                .setSignalSemaphores(signalSemaphores);

        const auto submitResult =
            mDevice->GetGraphicsQueue()
                .submit(
                    1,
                    &submitInfo,
                    mInFlightFences[mCurrentFrameIndex]);

        if (submitResult != vk::Result::eSuccess)
            throw std::runtime_error("failed to submit draw command buffer!");

        auto swapChains = { mSwapChain->Get() };

        auto imageIndices = { mSwapChain->GetCurrentFrameIndex() };

        const auto presentInfo =
            vk::PresentInfoKHR()
                .setWaitSemaphores(signalSemaphores)
                .setSwapchains(swapChains)
                .setImageIndices(imageIndices);

        if (const auto result = mDevice->GetPresentQueue().presentKHR(presentInfo);
            result == vk::Result::eErrorOutOfDateKHR ||
            result == vk::Result::eSuboptimalKHR)
        {
            RebuildSwapChain();
        }
        else if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mImageAvailableSemaphores.size();
        mDevice->Get().waitIdle();
    }

} // namespace FREYA_NAMESPACE
