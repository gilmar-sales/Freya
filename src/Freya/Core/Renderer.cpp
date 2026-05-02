#include "Renderer.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <cmath>

namespace FREYA_NAMESPACE
{
    Renderer::~Renderer()
    {
        mSwapChain.reset();

        mSurface.reset();

        mRenderPass.reset();

        mCommandPool.reset();

        mDevice.reset();

        mInstance.reset();
    }

    void Renderer::RebuildSwapChain()
    {
        mDevice->Get().waitIdle();

        mSwapChain.reset();

        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();
    }

    void Renderer::SetVSync(bool vSync)
    {
        mFreyaOptions->vSync = vSync;
        RebuildSwapChain();
    }

    void Renderer::SetSamples(std::uint32_t samples)
    {
        mFreyaOptions->sampleCount = samples;
        const auto frameCount      = mSwapChain->GetFrames().size();
        const auto extent          = mSurface->QueryExtent();

        mSwapChain.reset();
        mRenderPass.reset();

        mRenderPass =
            mServiceProvider->GetService<RenderPassBuilder>()->Build();

        mSwapChain = mServiceProvider->GetService<SwapChainBuilder>()->Build();
    }

    void Renderer::SetDrawDistance(const float drawDistance)
    {
        mFreyaOptions->drawDistance = drawDistance;
        ClearProjections();
    }

    glm::mat4 Renderer::MakeReverseZProjection(const float fovRadians,
                                                const float aspect,
                                                const float near,
                                                const float far) const
    {
        // Reverse-Z Vulkan projection matrix.
        // Maps:  near plane → depth 1.0,  far plane → depth 0.0
        // Floating-point precision is highest near 0, so the far field gets
        // exponentially better precision than the standard (near→0, far→1)
        // mapping.
        const auto f = 1.0f / std::tan(fovRadians * 0.5f);

        auto proj      = glm::mat4(0.0f);
        proj[0][0]     = f / aspect;
        proj[1][1]     = -f; // Vulkan Y flip (handled here instead of post-multiply)
        // Reverse-Z: maps near plane → depth 1, far plane → depth 0
        // For right-handed view space (z_eye negative), w_clip = -z_eye
        // is positive, ensuring fragments pass the near/far clip.
        proj[2][2]     = near / (far - near);     // z-scale (positive)
        proj[2][3]     = -1.0f;                   // w_clip = -z_eye
        proj[3][2]     = near * far / (far - near); // z-bias
        proj[3][3]     = 0.0f;

        return proj;
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

        // Use a more reasonable near plane for planetary-scale scenes.
        // With near=1.0 and reverse-Z, depth precision at far distances
        // improves dramatically over the old near=0.1.
        const auto near = 1.0f;
        const auto far  = mFreyaOptions->drawDistance;

        auto projectionUniformBuffer = ProjectionUniformBuffer {
            .view       = glm::lookAt(cameraPosition,
                                      cameraPosition + cameraForward,
                                      cameraUp),
            .projection = MakeReverseZProjection(
                glm::radians(45.0f),
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
            mRenderPass->UpdateProjection(projectionUniformBuffer, frameIndex);
        }

        mCurrentProjection = projectionUniformBuffer;
    }

    glm::mat4 Renderer::CalculateProjectionMatrix(const float near,
                                                  const float far) const
    {
        const auto extent = mSurface->QueryExtent();
        return MakeReverseZProjection(
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
            mFreyaOptions->width = mResizeEvent->width;
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

        mRenderPass->Begin(mSwapChain, mCommandPool);

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
        mRenderPass->End(mCommandPool);

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

} // namespace FREYA_NAMESPACE
