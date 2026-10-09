#include <Freya/Advanced.hpp>
#include <Freya/Freya.hpp>

#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderFrameContext.hpp"
#include "Freya/Core/SwapChain.hpp"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

namespace
{
    struct ResizeState
    {
        // Model previews can keep these alive after Renderer replaces them.
        std::vector<skr::Arc<fra::SwapChain>> retained;
        std::string                           error;
        unsigned                              replacements = 0;
    };

    class RetainSwapChainStage final : public fra::IFrameStage
    {
      public:
        explicit RetainSwapChainStage(ResizeState& state) : mState(state) {}
        const char* Name() const override { return "RetainSwapChain"; }
        void        Execute(fra::StageContext& ctx) override
        {
            auto& frame = static_cast<fra::RenderFrameContext&>(ctx);
            if (mState.retained.empty() ||
                mState.retained.back() != frame.swapChain)
                mState.retained.push_back(frame.swapChain);
        }

      private:
        ResizeState& mState;
    };

    ResizeState* gResizeState = nullptr;

    class ResizeApp final : public fra::AbstractApplication
    {
      public:
        explicit ResizeApp(const skr::Arc<skr::ServiceProvider>& services) :
            AbstractApplication(services)
        {
        }

        void StartUp() override
        {
            if (!fra::RendererAdvanced(*mRenderer)
                     .InsertFrameStage(
                         "GpuAnim",
                         std::make_shared<RetainSwapChainStage>(*gResizeState)))
                gResizeState->error = "Failed to insert retention stage";
        }

        void Update() override
        {
            auto& state = *gResizeState;
            if (!state.error.empty() || mFrame == kSizes.size())
            {
                mWindow->Close();
                return;
            }

            auto device = GetMainServiceProvider()->GetService<fra::Device>();
            if (mFrame > 0)
            {
                const auto previous        = state.retained.back();
                const auto [width, height] = kSizes[mFrame];
                if (!SDL_SetWindowSize(
                        static_cast<SDL_Window*>(mWindow->NativeWindow()),
                        width, height))
                {
                    state.error = SDL_GetError();
                    mWindow->Close();
                    return;
                }
                mWindow->Resize(width, height);
                mRenderer->RebuildSwapChain();

                // A replacement must retire the previous native swapchain
                // even while its Arc is retained. Timeout=0 keeps this check
                // bounded if a regression leaves the previous chain active.
                const auto    fence  = device->Get().createFence({});
                std::uint32_t image  = 0;
                const auto    result = vkAcquireNextImageKHR(
                    static_cast<VkDevice>(device->Get()),
                    static_cast<VkSwapchainKHR>(previous->Get()), 0,
                    VK_NULL_HANDLE, static_cast<VkFence>(fence), &image);
                if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
                {
                    (void) device->Get().waitForFences(
                        1, &fence, true, UINT64_MAX);
                }
                device->Get().destroyFence(fence);
                if (result != VK_ERROR_OUT_OF_DATE_KHR)
                    state.error = "Previous swapchain was not retired: " +
                                  std::to_string(result);
                ++state.replacements;
            }

            mRenderer->BeginFrame();
            mScene.Upload(*mRenderer);
            mRenderer->EndFrame();
            ++mFrame;
        }

      private:
        static constexpr std::array<std::array<int, 2>, 5> kSizes {
            { { 64, 64 }, { 128, 96 }, { 80, 60 }, { 160, 100 }, { 96, 72 } }
        };
        fra::Scene mScene;
        unsigned   mFrame = 0;
    };
} // namespace

TEST(SwapChainResize, RetiresPreviousChainWhileReferencesRemainAlive)
{
    ResizeState state;
    gResizeState = &state;
    try
    {
        const auto app =
            skr::ApplicationBuilder()
                .WithExtension<fra::FreyaExtension>(
                    [](fra::FreyaExtension freya) {
                        freya.WithOptions(
                            [](fra::FreyaOptionsBuilder& options) {
                                options.SetTitle("Freya resize regression")
                                    .SetWidth(64)
                                    .SetHeight(64)
                                    .SetVSync(false)
                                    .SetSampleCount(1)
                                    .SetShaderRoot(FREYA_SHADER_ROOT);
                            });
                    })
                .Build<ResizeApp>();
        app->Run();
    }
    catch (const std::exception& error)
    {
        FAIL() << error.what();
    }
    EXPECT_TRUE(state.error.empty()) << state.error;
    EXPECT_EQ(state.replacements, 4u);
    EXPECT_GE(state.retained.size(), 5u);
    // Destroy retained resources while the test's state is still valid.
    state.retained.clear();
    gResizeState = nullptr;
}
