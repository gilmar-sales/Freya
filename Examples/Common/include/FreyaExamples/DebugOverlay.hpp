#pragma once

#include <Freya/Freya.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace FreyaExamples
{
    /**
     * @brief Debug overlay for Freya examples (options, debug views, CPU/GPU
     * stage timing via Vulkan timestamps) built on the engine's own
     * `fra::UiContext`.
     *
     * The overlay owns a separate UiContext that records into the renderer's
     * shared UiDraw queue, so it composes on top of an app's own game UI
     * without sharing frame state. Toggle with F1.
     *
     * Needs `Resources/Fonts/NotoSans-Regular.ttf` next to the executable
     * (copied by add_freya_example()).
     */
    class DebugOverlay
    {
      public:
        DebugOverlay() = default;
        ~DebugOverlay();

        DebugOverlay(const DebugOverlay&)            = delete;
        DebugOverlay& operator=(const DebugOverlay&) = delete;

        /**
         * @brief Load the font, bind UI input events and register the toggle
         * key. Call once from StartUp after the renderer exists.
         * @param services Main-window service provider
         * (`GetMainServiceProvider()`), used to resolve EventManager and
         * TexturePool.
         */
        bool Init(fra::Renderer&                              renderer,
                  fra::Window&                                window,
                  const skr::Arc<skr::ServiceProvider>&       services);

        void Shutdown();

        /** @brief Apply deferred swapchain changes — call at the start of
         * Update. */
        void BeginFrame();

        /**
         * @brief Draw debug panels. Call after scene upload / before EndFrame.
         * @param cpuUpdateMs Wall time of the example Update body so far.
         */
        void Draw(fra::Renderer&     renderer,
                  fra::FreyaOptions& options,
                  float              cpuFrameMs,
                  float              cpuUpdateMs,
                  fra::LightService* lights = nullptr);

        /** @brief Present the frame (Renderer::EndFrame). */
        void EndFrame(fra::Renderer& renderer);

        [[nodiscard]] bool WantsCaptureMouse() const;
        [[nodiscard]] bool WantsCaptureKeyboard() const;

        [[nodiscard]] bool Enabled() const { return mEnabled; }
        void               SetEnabled(bool enabled) { mEnabled = enabled; }

        /** @brief Mark the start of the measured Update section. */
        void MarkUpdateStart();

        [[nodiscard]] float ElapsedUpdateMs() const;

        /** @brief Optional label written into cull dump meta.example. */
        void SetCullDumpExampleName(std::string name)
        {
            mCullDumpExample = std::move(name);
        }

        /**
         * @brief "Show cull AABBs" checkbox state (GPU Cull section). Apps
         * that push per-instance AABB wireframes (see
         * FreyaExamples::DrawCullAabb) should gate that work on this flag
         * and ensure `Renderer::SetDebugDrawEnabled(true)` so the queued
         * lines actually render.
         */
        [[nodiscard]] bool ShowCullAabbs() const { return mShowCullAabbs; }
        void SetShowCullAabbs(bool enabled) { mShowCullAabbs = enabled; }

      private:
        void applyPendingSwapchainChanges();
        void pollCullFrameDump(fra::Renderer& renderer);

        bool            mInitialized = false;
        bool            mEnabled     = true;
        fra::Renderer*  mRenderer    = nullptr;
        skr::Arc<fra::EventManager> mEvents;

        fra::UiContext mUi;
        fra::FontAtlas mFont;
        /// Guards the key listener (events have no unsubscribe API yet).
        std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);

        float mLastContentH = 900.f; ///< scroll extent estimate (logical px)

        bool mPendingVSync      = false;
        bool mPendingVSyncValue = false;

        std::chrono::steady_clock::time_point mUpdateStart {};
        std::chrono::steady_clock::time_point mLastDraw {};

        std::string mCullDumpExample = "Example";
        std::string mLastCullDumpPath;
        bool        mCullDumpPending = false;
        bool        mShowCullAabbs   = false;
    };
} // namespace FreyaExamples
