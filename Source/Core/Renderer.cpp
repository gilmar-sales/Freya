#include "Freya/Core/Renderer.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ForwardPassBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/UniformBuffer.hpp"

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
        const auto     cameraForward =
            glm::vec3(glm::vec4(0.0f, 0.0f, 1.0f, 0.0) * cameraMatrix);
        const auto cameraRight = glm::normalize(
            glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), cameraForward));
        const auto cameraUp =
            glm::normalize(glm::cross(cameraForward, cameraRight));

        auto projectionUniformBuffer = ProjectionUniformBuffer {
            .view       = glm::lookAt(cameraPosition,
                                      cameraPosition + cameraForward,
                                      cameraUp),
            .projection = glm::perspectiveFov(
                glm::radians(45.0f),
                static_cast<float>(extent.width),
                static_cast<float>(extent.height),
                0.1f,
                mDrawDistance),
            .ambientLight =
                glm::vec4(glm::normalize(glm::vec3(0.0f, 3.0f, 0.0f)), 0.1f)
        };

        projectionUniformBuffer.projection[1][1] *= -1.f;

        for (auto frameIndex = 0; frameIndex < mSwapChain->GetFrameCount();
             frameIndex++)
        {
            mRenderPass->UpdateProjection(projectionUniformBuffer, frameIndex);
        }

        mCurrentProjection = projectionUniformBuffer;
    }

    glm::mat4 Renderer::CalculateProjectionMatrix(const float near,
                                                  const float far) const
    {
        const auto extent = mSurface->QueryExtent();
        return glm::perspective(
            glm::radians(75.0f),
            static_cast<float>(extent.width) /
                static_cast<float>(extent.height),
            near,
            far);
    }

    void Renderer::UpdateProjection(
        ProjectionUniformBuffer& projectionUniformBuffer)
    {
        mRenderPass->UpdateProjection(projectionUniformBuffer,
                                      mSwapChain->GetCurrentFrameIndex());
        mCurrentProjection = projectionUniformBuffer;
    }

    void Renderer::UpdateModel(const glm::mat4& model) const
    {
        mCommandPool->GetCommandBuffer().pushConstants(
            mRenderPass->GetPipelineLayout(),
            vk::ShaderStageFlagBits::eVertex,
            0,
            sizeof(model),
            &model);
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
        mSwapChain->WaitNextFrame();

        if (mResizeEvent.has_value())
        {
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
        mRenderPass->SetFrameIndex(mSwapChain->GetCurrentFrameIndex());

        const auto& commandBuffer = mCommandPool->GetCommandBuffer();

        if (mSwapChain->GetCurrentFrameIndex() == 0)
            mDevice->Get().resetCommandPool(mCommandPool->Get());

        constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        commandBuffer.begin(beginInfo);

        auto clearValues = std::vector {
            vk::ClearValue().setColor(mClearColor),
            vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue().setDepth(1.0f)),
        };

        if (mSamples > vk::SampleCountFlagBits::e1)
            clearValues.push_back(vk::ClearValue().setColor(mClearColor));

        const auto renderPassBeginInfo =
            vk::RenderPassBeginInfo()
                .setRenderPass(mRenderPass->Get())
                .setFramebuffer(swapChainFrame.frameBuffer)
                .setRenderArea(vk::Rect2D().setOffset({ 0, 0 }).setExtent(
                    mSwapChain->GetExtent()))
                .setClearValues(clearValues);

        commandBuffer.beginRenderPass(renderPassBeginInfo,
                                      vk::SubpassContents::eInline);

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

        mRenderPass->BindDescriptorSet(mCommandPool,
                                       mSwapChain->GetCurrentFrameIndex());
    }

    void Renderer::EndFrame()
    {
        auto& commandBuffer = mCommandPool->GetCommandBuffer();

        commandBuffer.endRenderPass();
        commandBuffer.end();

        auto commandBuffers = { commandBuffer };

        mSwapChain->WaitCommandBuffersSubmission(commandBuffers);

        auto presentResult = mSwapChain->Present();

        if (presentResult == vk::Result::eErrorOutOfDateKHR ||
            presentResult == vk::Result::eSuboptimalKHR)
        {
            RebuildSwapChain();
        }
        else if (presentResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        mDevice->Get().waitIdle();
    }

} // namespace FREYA_NAMESPACE
