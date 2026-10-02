#include <FreyaExamples/CullFrameDumpIo.hpp>
#include <FreyaExamples/DebugOverlay.hpp>

#include <Freya/Advanced.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string_view>

namespace FreyaExamples
{
    namespace
    {
        constexpr const char* kFontPath =
            "./Resources/Fonts/NotoSans-Regular.ttf";

        constexpr std::array<std::string_view, 5> kQualityItems = {
            "Low", "Medium", "High", "Ultra", "Off"
        };

        constexpr std::array<std::string_view, 12> kDebugViews = {
            "Lit",          "Albedo",      "Normal",   "Depth",
            "Roughness",    "Metalness",   "Material AO", "Material ID",
            "Velocity",     "SSAO Blurred", "SSAO Raw",  "Shadows"
        };

        // Window content metrics (logical px; the overlay UI runs at the
        // host UI scale).
        constexpr float kWindowW   = 400.f;
        constexpr float kContentW  = kWindowW - 36.f;
        constexpr float kRowH      = 30.f;
        constexpr float kHeaderH   = 38.f;
        constexpr float kWrapWidth = kContentW - 8.f;

        template <typename... Args>
        std::string Format(const char* fmt, Args... args)
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), fmt, args...);
            return buf;
        }

        // Two-column "label  value" row (the UI font is proportional).
        void StatRow(fra::UiContext& ui, std::string_view id,
                     std::string_view label, const std::string& value)
        {
            float widths[] = { 200.f, 150.f };
            ui.BeginColumns(id, 2, widths);
            ui.Label(label, 14.f);
            ui.NextColumn();
            ui.Label(value, 14.f);
            ui.EndColumns();
        }

        // Quality combo with a caption row. Returns true when changed.
        bool QualityCombo(fra::UiContext& ui, std::string_view id, int* value)
        {
            ui.Label(id, 14.f);
            return ui.ComboBox(
                id, std::span<const std::string_view>(kQualityItems), value,
                { kContentW, 30.f });
        }
    } // namespace

    DebugOverlay::~DebugOverlay()
    {
        Shutdown();
    }

    bool DebugOverlay::Init(fra::Renderer&                        renderer,
                            fra::Window&                          window,
                            const skr::Arc<skr::ServiceProvider>& services)
    {
        (void) window;
        if (mInitialized)
            return true;

        mEvents = services->GetService<fra::EventManager>();
        auto textures = services->GetService<fra::TexturePool>();
        if (!mEvents || !textures)
        {
            std::fprintf(stderr, "DebugOverlay: missing engine services\n");
            return false;
        }

        mFont = fra::FontAtlas::Create(*textures, kFontPath);
        if (!mFont.Valid())
            std::fprintf(stderr, "DebugOverlay: failed to load %s\n",
                         kFontPath);

        mRenderer = &renderer;
        mUi.SetDraw(renderer.GetUiContext().GetDraw());
        mUi.BindEvents(*mEvents);

        mEvents->Subscribe<fra::KeyReleasedEvent>(
            [this, alive = mAlive](const fra::KeyReleasedEvent& event) {
                if (*alive && mInitialized &&
                    event.key == fra::KeyCode::F1)
                    mEnabled = !mEnabled;
            });

        mLastDraw    = std::chrono::steady_clock::now();
        mInitialized = true;
        return true;
    }

    void DebugOverlay::Shutdown()
    {
        if (mInitialized && mEvents)
            mUi.UnbindEvents(*mEvents);
        *mAlive            = false;
        mInitialized       = false;
        mRenderer          = nullptr;
        mEvents            = nullptr;
        mPendingVSync      = false;
        mPendingVSyncValue = false;
    }

    void DebugOverlay::applyPendingSwapchainChanges()
    {
        if (!mPendingVSync || !mRenderer)
            return;
        mPendingVSync = false;
        // Between frames: no open command buffer, safe to rebuild.
        if (mRenderer->GetVSync() != mPendingVSyncValue)
            mRenderer->SetVSync(mPendingVSyncValue);
    }

    void DebugOverlay::BeginFrame()
    {
        if (!mInitialized)
            return;
        applyPendingSwapchainChanges();
    }

    void DebugOverlay::MarkUpdateStart()
    {
        mUpdateStart = std::chrono::steady_clock::now();
    }

    float DebugOverlay::ElapsedUpdateMs() const
    {
        using Ms = std::chrono::duration<float, std::milli>;
        return Ms(std::chrono::steady_clock::now() - mUpdateStart).count();
    }

    bool DebugOverlay::WantsCaptureMouse() const
    {
        return mInitialized && mEnabled && mUi.WantCaptureMouse();
    }

    bool DebugOverlay::WantsCaptureKeyboard() const
    {
        return mInitialized && mEnabled && mUi.WantCaptureKeyboard();
    }

    void DebugOverlay::Draw(fra::Renderer&     renderer,
                            fra::FreyaOptions& options,
                            const float        cpuFrameMs,
                            const float        cpuUpdateMs,
                            fra::LightService* lights)
    {
        if (!mInitialized || !mEnabled)
            return;

        const auto  now = std::chrono::steady_clock::now();
        const float dt  = std::clamp(
            std::chrono::duration<float>(now - mLastDraw).count(), 1e-4f,
            0.25f);
        mLastDraw = now;

        // The renderer scales the whole shared UiDraw queue with its own
        // context's scale. Using the same reference size and framebuffer
        // extent gives this context the same (resolution-relative) scale, so
        // the overlay follows 4K / HiDPI like the game UI does.
        const glm::uvec2 fb { std::max(options.width, 1u),
                              std::max(options.height, 1u) };
        mUi.SetDraw(renderer.GetUiContext().GetDraw());
        mUi.SetReferenceSize(renderer.GetUiContext().ReferenceSize());
        mUi.Style().font = mFont.Valid() ? &mFont : nullptr;

        auto& ui = mUi;
        ui.Begin(dt, fb);

        // Tall enough to show the first sections without scrolling.
        const float winH = std::clamp(
            ui.LogicalSize().y - 24.f, 320.f, 900.f);

        fra::UiWindowOpts winOpts {};
        winOpts.defaultPos  = { 12.f, 12.f };
        winOpts.defaultSize = { kWindowW, winH };
        winOpts.closable    = false;

        float contentH = 0.f;
        if (ui.BeginWindow("freya_debug", "Freya Debug  (F1 hide)",
                           { kWindowW, winH }, winOpts))
        {
            ui.BeginScrollView("freya_debug_scroll",
                               { kWindowW - 16.f, winH - 64.f },
                               mLastContentH);

            // ---- Timing -------------------------------------------------
            contentH += kHeaderH;
            if (ui.CollapsingHeader("dbg_timing", "Timing", true))
            {
                StatRow(ui, "t_cpu", "CPU frame",
                        Format("%.2f ms (%.1f FPS)", cpuFrameMs,
                               cpuFrameMs > 1e-3f ? 1000.f / cpuFrameMs
                                                  : 0.f));
                StatRow(ui, "t_upd", "CPU update",
                        Format("%.2f ms", cpuUpdateMs));
                StatRow(ui, "t_res", "Render",
                        Format("%ux%u", options.width, options.height));
                contentH += 3 * 24.f;

                fra::FrameGpuTimingSample gpu {};
                if (renderer.PollFrameGpuTiming(gpu) && gpu.enabled)
                {
                    StatRow(ui, "t_gpu", "GPU total",
                            Format("%.2f ms", gpu.totalGpuMs));
                    ui.Separator();
                    contentH += 24.f + 12.f;
                    for (std::uint32_t i = 0; i < gpu.stageCount; ++i)
                    {
                        StatRow(ui, Format("t_stage%u", i),
                                gpu.stages[i].name,
                                Format("%.2f ms", gpu.stages[i].gpuMs));
                        contentH += 24.f;
                    }
                }
                else
                {
                    ui.Label("GPU timestamps: warming up / unavailable", 13.f);
                    contentH += 24.f;
                }
                ui.TextWrapped(
                    "GPU times are Vulkan timestamp deltas per frame stage "
                    "(desktop). Not Mali HWCPipe PTILES / late-ZS.",
                    kWrapWidth, 12.f);
                contentH += 64.f;
            }

            // ---- Quality ------------------------------------------------
            contentH += kHeaderH;
            if (ui.CollapsingHeader("dbg_quality", "Quality", true))
            {
                int shadow = static_cast<int>(renderer.GetShadowQuality());
                if (QualityCombo(ui, "Shadow", &shadow))
                    renderer.SetShadowQuality(
                        static_cast<fra::ShadowQuality>(shadow));

                int ssao = static_cast<int>(renderer.GetSsaoQuality());
                if (QualityCombo(ui, "SSAO", &ssao))
                    renderer.SetSsaoQuality(
                        static_cast<fra::SsaoQuality>(ssao));

                int taa = static_cast<int>(renderer.GetTaaQuality());
                if (QualityCombo(ui, "TAA", &taa))
                    renderer.SetTaaQuality(
                        static_cast<fra::TaaQuality>(taa));

                int bloom = static_cast<int>(renderer.GetBloomQuality());
                if (QualityCombo(ui, "Bloom", &bloom))
                    renderer.SetBloomQuality(
                        static_cast<fra::BloomQuality>(bloom));

                // Defer SetVSync: rebuilding the swapchain mid-frame (open
                // CB / stale image index / new framebuffer count) aborts.
                bool vsync =
                    mPendingVSync ? mPendingVSyncValue : renderer.GetVSync();
                if (ui.Checkbox("VSync", &vsync))
                {
                    mPendingVSync      = true;
                    mPendingVSyncValue = vsync;
                }
                contentH += 4 * 58.f + kRowH;
            }

            // ---- Lights -------------------------------------------------
            if (lights != nullptr)
            {
                contentH += kHeaderH;
                if (ui.CollapsingHeader("dbg_lights", "Lights", true))
                {
                    auto typeToggle = [&](const char*     label,
                                          fra::LightType type) {
                        bool on = HasFlag(lights->GetLightTypeFlags(type),
                                          fra::LightFlags::Enabled);
                        if (ui.Checkbox(label, &on))
                        {
                            auto flags = lights->GetLightTypeFlags(type);
                            SetFlag(flags, fra::LightFlags::Enabled, on);
                            lights->SetLightTypeFlags(type, flags);
                        }
                    };
                    typeToggle("Directional", fra::LightType::Directional);
                    typeToggle("Point", fra::LightType::Point);
                    typeToggle("Spot", fra::LightType::Spot);
                    typeToggle("Area", fra::LightType::Area);
                    ui.TextWrapped(
                        "Mutes lighting and shadow casting for the type; "
                        "host light data is unchanged.",
                        kWrapWidth, 12.f);
                    contentH += 4 * kRowH + 64.f;
                }
            }

            // ---- Debug views --------------------------------------------
            contentH += kHeaderH;
            if (ui.CollapsingHeader("dbg_views", "Debug views", true))
            {
                int view = static_cast<int>(renderer.GetDeferredDebugView());
                ui.Label("Deferred view", 14.f);
                if (ui.ComboBox("dbg_deferred_view",
                                std::span<const std::string_view>(kDebugViews),
                                &view, { kContentW, 30.f }))
                {
                    renderer.SetDeferredDebugView(
                        static_cast<fra::DeferredDebugView>(view));
                }

                bool dbgDraw = renderer.IsDebugDrawEnabled();
                if (ui.Checkbox("Debug draw", &dbgDraw))
                    renderer.SetDebugDrawEnabled(dbgDraw);

                const glm::vec2 sliderSize { kContentW, 24.f };
                ui.SliderFloat("SSAO radius", &options.ssaoRadius, 0.05f,
                               2.0f, sliderSize);
                renderer.SetSsaoRadius(options.ssaoRadius);
                ui.SliderFloat("SSAO bias", &options.ssaoBias, 0.0f, 0.1f,
                               sliderSize);
                renderer.SetSsaoBias(options.ssaoBias);
                ui.SliderFloat("SSAO power", &options.ssaoPower, 0.5f, 4.0f,
                               sliderSize);
                renderer.SetSsaoPower(options.ssaoPower);
                ui.SliderFloat("SSAO intensity", &options.ssaoIntensity, 0.0f,
                               2.0f, sliderSize);
                renderer.SetSsaoIntensity(options.ssaoIntensity);
                contentH += 58.f + kRowH + 4 * 34.f;
            }

            // ---- GPU Cull -----------------------------------------------
            contentH += kHeaderH;
            if (ui.CollapsingHeader("dbg_cull", "GPU Cull", false))
            {
                if (ui.Button("Dump cull frame", { kContentW, 32.f }))
                {
                    fra::Advanced(renderer).RequestCullFrameDump();
                    mCullDumpPending = true;
                    mLastCullDumpPath.clear();
                }
                contentH += 40.f;
                if (mCullDumpPending)
                {
                    ui.Label("Waiting for GPU readback...", 13.f);
                    contentH += 24.f;
                }
                else if (!mLastCullDumpPath.empty())
                {
                    ui.TextWrapped("Wrote " + mLastCullDumpPath, kWrapWidth,
                                   12.f);
                    contentH += 48.f;
                }
                ui.TextWrapped(
                    "Writes frame.json (+ hiz.r32f) under ./cull_dumps/ for "
                    "FreyaGpuTests fixtures.",
                    kWrapWidth, 12.f);
                ui.Separator();
                if (ui.Checkbox("Show cull AABBs", &mShowCullAabbs))
                {
                    if (mShowCullAabbs)
                        renderer.SetDebugDrawEnabled(true);
                }
                ui.TextWrapped(
                    "Wireframe of the AABB the GPU cull compute shader tests "
                    "per instance (mesh-local aabbMin/aabbMax x model). "
                    "Color coding is example-defined.",
                    kWrapWidth, 12.f);
                contentH += 64.f + 12.f + kRowH + 96.f;
            }

            ui.EndScrollView();
            mLastContentH = contentH + 24.f;
        }
        ui.EndWindow();
        ui.End();

        pollCullFrameDump(renderer);
    }

    void DebugOverlay::pollCullFrameDump(fra::Renderer& renderer)
    {
        if (!mCullDumpPending)
            return;

        fra::CullFrameSnapshot snap {};
        if (!fra::Advanced(renderer).TryConsumeCullFrameDump(snap))
            return;

        snap.example = mCullDumpExample;
        if (snap.label.empty())
            snap.label = "dump";

        const auto dir  = MakeCullDumpDirectory();
        const auto path = WriteCullFrameDump(snap, dir);
        if (path.empty())
            std::fprintf(stderr, "DebugOverlay: cull dump write failed\n");
        else
            mLastCullDumpPath = path;
        mCullDumpPending = false;
    }

    void DebugOverlay::EndFrame(fra::Renderer& renderer)
    {
        renderer.EndFrame();
    }
} // namespace FreyaExamples
