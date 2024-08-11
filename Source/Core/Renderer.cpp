#include "Core/Renderer.hpp"

#include "Builders/BufferBuilder.hpp"
#include "Builders/RenderPassBuilder.hpp"
#include "Builders/SwapChainBuilder.hpp"

#include "Asset/Vertex.hpp"
#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include <Core/UniformBuffer.hpp>

namespace FREYA_NAMESPACE
{
    Renderer::~Renderer()
    {
        mSwapChain.reset();

        mSurface.reset();

        mRenderPass.reset();

        mVertexBuffer.reset();
        mIndexBuffer.reset();

        for (auto& semaphore : mRenderFinishedSemaphores)
        {
            mDevice->Get().destroySemaphore(semaphore);
        }

        for (auto& semaphore : mImageAvailableSemaphores)
        {
            mDevice->Get().destroySemaphore(semaphore);
        }

        for (auto& fence : mInFlightFences)
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

        auto frameCount = mSwapChain->GetFrames().size();
        auto extent     = mSurface->QueryExtent();

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
        mSamples        = samples;
        auto frameCount = mSwapChain->GetFrames().size();
        auto extent     = mSurface->QueryExtent();

        mSwapChain.reset();
        mRenderPass.reset();

        mRenderPass =
            RenderPassBuilder()
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

    void Renderer::SetDrawDistance(float drawDistance)
    {
        mDrawDistance = drawDistance;
        ClearProjections();
    }

    void Renderer::ClearProjections()
    {
        auto extent = mSurface->QueryExtent();

        auto cameraPosition = glm::vec3(0.0f, 0.0f, -10.0f);
        auto cameraMatrix   = glm::mat4(1.0f);
        auto cameraForward =
            glm::vec3(glm::vec4(0.0f, 0.0f, 1.0f, 0.0) * cameraMatrix);
        auto cameraRight =
            glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraForward));
        auto cameraUp = glm::normalize(glm::cross(cameraForward, cameraRight));

        auto projectionUniformBuffer = fra::ProjectionUniformBuffer {
            .view =
                glm::lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp),
            .projection = glm::perspectiveFov(glm::radians(45.0f),
                                              (float) extent.width, (float) extent.height,
                                              0.1f,
                                              mDrawDistance)
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

    void Renderer::UpdateModel(glm::mat4& model)
    {
        mCommandPool->GetCommandBuffer().pushConstants(mRenderPass->GetPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(model), &model);
    }

    std::shared_ptr<MeshPoolFactory> Renderer::GetMeshPoolFactory()
    {
        return std::make_shared<MeshPoolFactory>(mDevice, mPhysicalDevice, mCommandPool);
    }

    BufferBuilder Renderer::GetBufferBuilder()
    {
        return BufferBuilder(mDevice);
    }

    void Renderer::BindBuffer(std::shared_ptr<Buffer> buffer)
    {
        buffer->Bind(mCommandPool);
    }

    void Renderer::BeginFrame()
    {
        if (mDevice->Get().waitForFences(1,
                                         &mInFlightFences[mCurrentFrameIndex],
                                         true,
                                         UINT64_MAX) != vk::Result::eSuccess)
            throw new std::runtime_error("failed to wait for fences!");

        if (mResizeEvent.has_value())
        {
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
        auto& commandBuffer = mCommandPool->GetCommandBuffer();
        commandBuffer.reset();

        auto beginInfo = vk::CommandBufferBeginInfo();

        commandBuffer.begin(beginInfo);

        auto clearValues = { vk::ClearValue().setColor(mClearColor),
                             vk::ClearValue().setDepthStencil({ 1.0f, 0 }) };

        auto renderPassInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass->Get())
                .setFramebuffer(swapChainFrame.frameBuffer)
                .setRenderArea(
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(mSwapChain->GetExtent()))
                .setClearValues(clearValues);

        commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                   mRenderPass->GetGraphicsPipeline());

        auto viewport =
            vk::Viewport()
                .setX(0)
                .setY(0)
                .setWidth(mSwapChain->GetExtent().width)
                .setHeight(mSwapChain->GetExtent().height)
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        commandBuffer.setViewport(0, 1, &viewport);

        auto scissor =
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

        vk::PipelineStageFlags waitStages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };

        auto waitSemaphores = { mImageAvailableSemaphores[mCurrentFrameIndex] };

        auto signalSemaphores = { mRenderFinishedSemaphores[mCurrentFrameIndex] };

        auto submitInfo =
            vk::SubmitInfo()
                .setWaitSemaphores(waitSemaphores)
                .setWaitDstStageMask(waitStages)
                .setCommandBuffers(commandBuffers)
                .setSignalSemaphores(signalSemaphores);

        auto submitResult = mDevice->GetGraphicsQueue().submit(
            1,
            &submitInfo,
            mInFlightFences[mCurrentFrameIndex]);

        if (submitResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        auto swapChains = { mSwapChain->Get() };

        auto imageIndices = { mSwapChain->GetCurrentFrameIndex() };
        auto presentInfo =
            vk::PresentInfoKHR()
                .setWaitSemaphores(signalSemaphores)
                .setSwapchains(swapChains)
                .setImageIndices(imageIndices);

        auto result = mDevice->GetPresentQueue().presentKHR(presentInfo);

        if (result == vk::Result::eErrorOutOfDateKHR ||
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
