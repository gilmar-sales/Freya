#include "Renderer.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
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
        const Ref<CommandPool>&            commandPool,
        const Ref<LightService>&           lightService,
        const Ref<skr::ServiceProvider>&   serviceProvider,
        const Ref<FreyaOptions>&           freyaOptions,
        const Ref<EventManager>&           eventManager) :
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mSwapChain(swapChain), mForwardPass(forwardPass),
        mDeferredPass(deferredPass), mCommandPool(commandPool),
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
    }

    Renderer::~Renderer()
    {
        mSwapChain.reset();
        mSurface.reset();
        mForwardPass.reset();
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

        // For deferred mode, also recreate the deferred pass (which owns
        // framebuffers referencing swapchain images)
        if (IsDeferred())
        {
            mDeferredPass.reset();
            mDeferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain);
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

        // Update light service with camera position for attenuation
        if (mLightService && mLightService->HasLights())
        {
            mLightService->Update(mSwapChain->GetCurrentFrameIndex(), position);
        }
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

        // Begin the active render pass
        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->Begin(mSwapChain, mCommandPool);
        }
        else
        {
            mForwardPass->Begin(mSwapChain, mCommandPool);
        }

        // Viewport and scissor are the same regardless of pass type
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
            // The user drew in subpass 0 (depth pre-pass) and should have
            // advanced to subpass 1 (G-buffer) and drawn there too.
            // EndFrame handles the remaining subpasses:
            //   subpass 2 (lighting)   — fullscreen triangle
            //   subpass 3 (translucent) — skip (no translucent geometry)
            //   subpass 4 (composite)  — fullscreen triangle

            auto frameIndex = mSwapChain->GetCurrentFrameIndex();

            mDeferredPass->AdvanceSubpass(DeferredLightingPass,
                                          mCommandPool,
                                          frameIndex);
            mDeferredPass->DrawFullscreenTriangle(mCommandPool);

            mDeferredPass->AdvanceSubpass(DeferredTranslucentPass,
                                          mCommandPool,
                                          frameIndex);
            mDeferredPass->AdvanceSubpass(DeferredCompositePass,
                                          mCommandPool,
                                          frameIndex);
            mDeferredPass->DrawFullscreenTriangle(mCommandPool);

            mDeferredPass->End(mCommandPool);
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
            mDeferredPass->BindPipeline(subpass,
                                        mCommandPool,
                                        mSwapChain->GetCurrentFrameIndex());
        }
    }

    void Renderer::AdvanceSubpass(const std::uint32_t subpass)
    {
        if (IsDeferred() && mDeferredPass)
        {
            mDeferredPass->AdvanceSubpass(subpass,
                                          mCommandPool,
                                          mSwapChain->GetCurrentFrameIndex());
        }
    }

} // namespace FREYA_NAMESPACE
