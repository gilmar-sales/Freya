#pragma once

#include <Freya/Freya.hpp>

#include <chrono>

namespace FreyaExamples
{
    /**
     * @brief Dear ImGui overlay for Freya examples (options, debug views,
     * CPU/GPU stage timing via Vulkan timestamps).
     *
     * Requires Renderer::SetViewportTarget so the scene composites to an
     * offscreen target and BeginUI opens the swapchain UI pass.
     */
    class DebugOverlay
    {
      public:
        DebugOverlay() = default;
        ~DebugOverlay();

        DebugOverlay(const DebugOverlay&)            = delete;
        DebugOverlay& operator=(const DebugOverlay&) = delete;

        /**
         * @brief Initialize ImGui backends and register the platform event
         * observer. Call once from StartUp after the renderer exists.
         */
        bool Init(fra::Renderer&  renderer,
                  fra::Window&    window,
                  fra::IPlatform& platform);

        void Shutdown();

        /** @brief ImGui NewFrame — call at the start of Update. */
        void BeginFrame();

        /**
         * @brief Draw debug panels. Call after scene upload / before EndFrame.
         * @param cpuUpdateMs Wall time of the example Update body so far.
         */
        void Draw(fra::Renderer&     renderer,
                  fra::FreyaOptions& options,
                  float              cpuFrameMs,
                  float              cpuUpdateMs);

        /**
         * @brief EndScene + ImGui into UI pass + Present. Replaces
         * Renderer::EndFrame when the overlay is active.
         */
        void EndFrame(fra::Renderer& renderer);

        [[nodiscard]] bool WantsCaptureMouse() const;
        [[nodiscard]] bool WantsCaptureKeyboard() const;

        [[nodiscard]] bool Enabled() const { return mEnabled; }
        void               SetEnabled(bool enabled) { mEnabled = enabled; }

        /** @brief Mark the start of the measured Update section. */
        void MarkUpdateStart();

        [[nodiscard]] float ElapsedUpdateMs() const;

      private:
        static void onNativeEvent(const void* nativeEvent, void* user);

        bool createDescriptorPool(void* vkDevice);
        void destroyDescriptorPool(void* vkDevice);
        void releaseViewportTexture();
        void ensureViewportTexture(void* sampler, void* imageView);
        bool reinitVulkanBackend(fra::Renderer& renderer);
        void applyPendingSwapchainChanges();

        bool            mInitialized    = false;
        bool            mEnabled        = true;
        fra::Renderer*  mRenderer       = nullptr;
        fra::IPlatform* mPlatform       = nullptr;
        void*           mDescriptorPool = nullptr; ///< VkDescriptorPool
        void*           mDevice         = nullptr; ///< VkDevice (for shutdown)
        void*           mViewportSet    = nullptr; ///< VkDescriptorSet
        void*           mViewportView   = nullptr; ///< VkImageView cached key

        bool mPendingVSync      = false;
        bool mPendingVSyncValue = false;

        std::chrono::steady_clock::time_point mUpdateStart {};
    };
} // namespace FreyaExamples
