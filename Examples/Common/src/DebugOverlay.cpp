#include <FreyaExamples/CullFrameDumpIo.hpp>
#include <FreyaExamples/DebugOverlay.hpp>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

namespace FreyaExamples
{
    namespace
    {
        void checkVk(const VkResult err)
        {
            if (err != VK_SUCCESS)
                std::fprintf(stderr, "Vulkan error %d in DebugOverlay\n",
                             static_cast<int>(err));
        }

        const char* QualityLabel(const int index)
        {
            static constexpr const char* k[] = { "Low", "Medium", "High",
                                                 "Ultra", "Off" };
            return (index >= 0 && index <= 4) ? k[index] : "?";
        }
    } // namespace

    DebugOverlay::~DebugOverlay()
    {
        Shutdown();
    }

    void DebugOverlay::onNativeEvent(const void* nativeEvent, void* user)
    {
        auto* self = static_cast<DebugOverlay*>(user);
        if (!self || !self->mInitialized || !nativeEvent)
            return;
        ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(nativeEvent));
    }

    bool DebugOverlay::createDescriptorPool(void* vkDevice)
    {
        auto* device = static_cast<VkDevice>(vkDevice);
        if (!device)
            return false;

        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 },
        };
        VkDescriptorPoolCreateInfo poolInfo {};
        poolInfo.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 256;
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes    = poolSizes;

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) !=
            VK_SUCCESS)
            return false;
        mDescriptorPool = pool;
        return true;
    }

    void DebugOverlay::destroyDescriptorPool(void* vkDevice)
    {
        auto* device = static_cast<VkDevice>(vkDevice);
        if (device && mDescriptorPool)
        {
            vkDestroyDescriptorPool(
                device, static_cast<VkDescriptorPool>(mDescriptorPool),
                nullptr);
            mDescriptorPool = nullptr;
        }
    }

    bool DebugOverlay::Init(fra::Renderer&  renderer,
                            fra::Window&    window,
                            fra::IPlatform& platform)
    {
        if (mInitialized)
            return true;

        const auto width  = window.GetWidth();
        const auto height = window.GetHeight();
        auto       adv    = fra::Advanced(renderer);
        if (!adv.SetViewportTarget(width, height))
        {
            std::fprintf(stderr, "DebugOverlay: SetViewportTarget failed\n");
            return false;
        }

        auto handles = adv.GetImGuiNativeHandles();
        if (!handles.device || !handles.window || !handles.renderPass)
        {
            std::fprintf(stderr,
                         "DebugOverlay: incomplete ImGui native handles\n");
            return false;
        }

        mDevice    = handles.device;
        mRenderer  = &renderer;
        mPlatform  = &platform;
        mSdlWindow = handles.window;

        if (!createDescriptorPool(handles.device))
        {
            std::fprintf(stderr,
                         "DebugOverlay: descriptor pool creation failed\n");
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForVulkan(
                static_cast<SDL_Window*>(handles.window)))
        {
            std::fprintf(stderr, "DebugOverlay: SDL3 ImGui init failed\n");
            Shutdown();
            return false;
        }

        if (!reinitVulkanBackend(renderer))
        {
            Shutdown();
            return false;
        }

        platform.SetNativeEventObserver(&DebugOverlay::onNativeEvent, this);
        mInitialized = true;
        return true;
    }

    bool DebugOverlay::reinitVulkanBackend(fra::Renderer& renderer)
    {
        auto handles = fra::Advanced(renderer).GetImGuiNativeHandles();
        if (!handles.device || !handles.renderPass)
        {
            std::fprintf(stderr,
                         "DebugOverlay: incomplete ImGui native handles\n");
            return false;
        }

        // Required when the TU was built with VK_NO_PROTOTYPES /
        // IMGUI_IMPL_VULKAN_NO_PROTOTYPES; harmless when prototypes are linked.
        if (!ImGui_ImplVulkan_LoadFunctions(
                [](const char* functionName, void* userData) {
                    return vkGetInstanceProcAddr(
                        static_cast<VkInstance>(userData), functionName);
                },
                handles.instance))
        {
            std::fprintf(stderr,
                         "DebugOverlay: ImGui Vulkan LoadFunctions failed\n");
            return false;
        }

        ImGui_ImplVulkan_InitInfo initInfo {};
        initInfo.Instance = static_cast<VkInstance>(handles.instance);
        initInfo.PhysicalDevice =
            static_cast<VkPhysicalDevice>(handles.physicalDevice);
        initInfo.Device      = static_cast<VkDevice>(handles.device);
        initInfo.QueueFamily = handles.graphicsQueueFamily;
        initInfo.Queue       = static_cast<VkQueue>(handles.graphicsQueue);
        initInfo.DescriptorPool =
            static_cast<VkDescriptorPool>(mDescriptorPool);
        initInfo.MinImageCount = std::max(2u, handles.minImageCount);
        initInfo.ImageCount    = std::max(2u, handles.minImageCount);
        initInfo.MSAASamples   = VK_SAMPLE_COUNT_1_BIT;
        initInfo.RenderPass    = static_cast<VkRenderPass>(handles.renderPass);
        initInfo.CheckVkResultFn = checkVk;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            std::fprintf(stderr, "DebugOverlay: Vulkan ImGui init failed\n");
            return false;
        }
        return true;
    }

    void DebugOverlay::applyPendingSwapchainChanges()
    {
        if (!mPendingVSync || !mRenderer)
            return;

        mPendingVSync = false;
        if (mRenderer->GetVSync() == mPendingVSyncValue)
            return;

        // Between frames: no open command buffer. Rebuild then rebind ImGui
        // to the new CompositePass UI render pass / image count.
        mRenderer->SetVSync(mPendingVSyncValue);

        if (mDevice)
            vkDeviceWaitIdle(static_cast<VkDevice>(mDevice));
        releaseViewportTexture();
        ImGui_ImplVulkan_Shutdown();
        if (!reinitVulkanBackend(*mRenderer))
        {
            std::fprintf(stderr,
                         "DebugOverlay: failed to rebind ImGui after VSync\n");
            mEnabled = false;
        }
    }

    void DebugOverlay::releaseViewportTexture()
    {
        if (mViewportSet)
        {
            ImGui_ImplVulkan_RemoveTexture(
                static_cast<VkDescriptorSet>(mViewportSet));
            mViewportSet  = nullptr;
            mViewportView = nullptr;
        }
    }

    void DebugOverlay::ensureViewportTexture(void* sampler, void* imageView)
    {
        if (!sampler || !imageView)
        {
            releaseViewportTexture();
            return;
        }
        if (mViewportSet && mViewportView == imageView)
            return;

        releaseViewportTexture();
        const VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(
            static_cast<VkSampler>(sampler),
            static_cast<VkImageView>(imageView),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (!set)
        {
            std::fprintf(stderr,
                         "DebugOverlay: AddTexture for viewport failed\n");
            return;
        }
        mViewportSet  = set;
        mViewportView = imageView;
    }

    void DebugOverlay::Shutdown()
    {
        if (mPlatform)
        {
            mPlatform->SetNativeEventObserver(nullptr, nullptr);
            mPlatform = nullptr;
        }

        if (mInitialized)
        {
            if (mDevice)
            {
                vkDeviceWaitIdle(static_cast<VkDevice>(mDevice));
                releaseViewportTexture();
                ImGui_ImplVulkan_Shutdown();
            }
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            mInitialized = false;
        }

        if (mDevice)
        {
            destroyDescriptorPool(mDevice);
            mDevice = nullptr;
        }
        mRenderer          = nullptr;
        mSdlWindow         = nullptr;
        mPendingVSync      = false;
        mPendingVSyncValue = false;
    }

    void DebugOverlay::BeginFrame()
    {
        if (!mInitialized)
            return;

        // Apply swapchain rebuilds before Renderer::BeginFrame / recording.
        applyPendingSwapchainChanges();

        if (!mEnabled)
            return;

        auto* sdlWindow = static_cast<SDL_Window*>(mSdlWindow);
        int   w         = 0;
        int   h         = 0;
        if (!sdlWindow || !SDL_GetWindowSize(sdlWindow, &w, &h) || w < 0 ||
            h < 0)
            return;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
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
        if (!mInitialized || !mEnabled)
            return false;
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DebugOverlay::WantsCaptureKeyboard() const
    {
        if (!mInitialized || !mEnabled)
            return false;
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    void DebugOverlay::Draw(fra::Renderer&     renderer,
                            fra::FreyaOptions& options,
                            const float        cpuFrameMs,
                            const float        cpuUpdateMs)
    {
        if (!mInitialized || !mEnabled)
            return;

        auto viewport = fra::Advanced(renderer).GetViewportImage();
        if (viewport.valid && viewport.imageView && viewport.sampler)
        {
            ensureViewportTexture(viewport.sampler, viewport.imageView);
            if (mViewportSet)
            {
                // Fullscreen scene behind panels (swapchain UI pass is
                // otherwise empty when SetViewportTarget is active).
                const ImVec2 display = ImGui::GetIO().DisplaySize;
                ImGui::GetBackgroundDrawList()->AddImage(
                    reinterpret_cast<ImTextureID>(mViewportSet),
                    ImVec2(0.f, 0.f), display);
            }
        }

        ImGui::SetNextWindowPos(ImVec2(12.f, 12.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.f, 480.f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Freya Debug"))
        {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("CPU frame:  %.2f ms (%.1f FPS)", cpuFrameMs,
                        cpuFrameMs > 1e-3f ? 1000.f / cpuFrameMs : 0.f);
            ImGui::Text("CPU update: %.2f ms", cpuUpdateMs);
            if (viewport.valid)
                ImGui::Text("Render:     %ux%u", viewport.width,
                            viewport.height);
            else
                ImGui::Text("Window:     %ux%u", options.width, options.height);

            fra::FrameGpuTimingSample gpu {};
            if (renderer.PollFrameGpuTiming(gpu) && gpu.enabled)
            {
                ImGui::Text("GPU total:  %.2f ms", gpu.totalGpuMs);
                ImGui::Separator();
                for (std::uint32_t i = 0; i < gpu.stageCount; ++i)
                {
                    ImGui::Text("%-16s %6.2f ms", gpu.stages[i].name,
                                gpu.stages[i].gpuMs);
                }
            }
            else
            {
                ImGui::TextDisabled("GPU timestamps: warming up / unavailable");
            }
            ImGui::TextWrapped(
                "GPU times are Vulkan timestamp deltas per frame "
                "stage (desktop). Not Mali HWCPipe PTILES / late-ZS.");
        }

        if (ImGui::CollapsingHeader("Quality", ImGuiTreeNodeFlags_DefaultOpen))
        {
            int shadow = static_cast<int>(renderer.GetShadowQuality());
            if (ImGui::Combo("Shadow", &shadow,
                             "Low\0Medium\0High\0Ultra\0Off\0"))
                renderer.SetShadowQuality(
                    static_cast<fra::ShadowQuality>(shadow));

            int ssao = static_cast<int>(renderer.GetSsaoQuality());
            if (ImGui::Combo("SSAO", &ssao, "Low\0Medium\0High\0Ultra\0Off\0"))
                renderer.SetSsaoQuality(static_cast<fra::SsaoQuality>(ssao));

            int taa = static_cast<int>(renderer.GetTaaQuality());
            if (ImGui::Combo("TAA", &taa, "Low\0Medium\0High\0Ultra\0Off\0"))
                renderer.SetTaaQuality(static_cast<fra::TaaQuality>(taa));

            int bloom = static_cast<int>(renderer.GetBloomQuality());
            if (ImGui::Combo("Bloom", &bloom,
                             "Low\0Medium\0High\0Ultra\0Off\0"))
                renderer.SetBloomQuality(static_cast<fra::BloomQuality>(bloom));

            // Defer SetVSync: rebuilding the swapchain mid-frame (open CB /
            // stale image index / new framebuffer count) aborts.
            bool vsync =
                mPendingVSync ? mPendingVSyncValue : renderer.GetVSync();
            if (ImGui::Checkbox("VSync", &vsync))
            {
                mPendingVSync      = true;
                mPendingVSyncValue = vsync;
            }

            (void) QualityLabel;
        }

        if (ImGui::CollapsingHeader("Debug views",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            int view = static_cast<int>(renderer.GetDeferredDebugView());
            if (ImGui::Combo(
                    "Deferred view", &view,
                    "Lit\0Albedo\0Normal\0Depth\0Roughness\0Metalness\0"
                    "Material AO\0Material ID\0Velocity\0SSAO Blurred\0"
                    "SSAO Raw\0Shadows\0"))
            {
                renderer.SetDeferredDebugView(
                    static_cast<fra::DeferredDebugView>(view));
            }

            bool dbgDraw = renderer.IsDebugDrawEnabled();
            if (ImGui::Checkbox("Debug draw", &dbgDraw))
                renderer.SetDebugDrawEnabled(dbgDraw);

            ImGui::SliderFloat("SSAO radius", &options.ssaoRadius, 0.05f, 2.0f);
            renderer.SetSsaoRadius(options.ssaoRadius);
            ImGui::SliderFloat("SSAO bias", &options.ssaoBias, 0.0f, 0.1f);
            renderer.SetSsaoBias(options.ssaoBias);
            ImGui::SliderFloat("SSAO power", &options.ssaoPower, 0.5f, 4.0f);
            renderer.SetSsaoPower(options.ssaoPower);
            ImGui::SliderFloat("SSAO intensity", &options.ssaoIntensity, 0.0f,
                               2.0f);
            renderer.SetSsaoIntensity(options.ssaoIntensity);
        }

        if (ImGui::CollapsingHeader("GPU Cull"))
        {
            if (ImGui::Button("Dump cull frame"))
            {
                fra::Advanced(renderer).RequestCullFrameDump();
                mCullDumpPending = true;
                mLastCullDumpPath.clear();
            }
            if (mCullDumpPending)
                ImGui::TextDisabled("Waiting for GPU readback…");
            else if (!mLastCullDumpPath.empty())
                ImGui::TextWrapped("Wrote %s", mLastCullDumpPath.c_str());
            ImGui::TextWrapped(
                "Writes frame.json (+ hiz.r32f) under ./cull_dumps/ "
                "for FreyaGpuTests fixtures.");

            ImGui::Separator();
            if (ImGui::Checkbox("Show cull AABBs", &mShowCullAabbs))
            {
                if (mShowCullAabbs)
                    renderer.SetDebugDrawEnabled(true);
            }
            ImGui::TextWrapped(
                "Wireframe of the AABB the GPU cull compute shader tests "
                "per instance (mesh-local aabbMin/aabbMax x model). Color "
                "coding is example-defined (e.g. CellBulbasaur highlights "
                "eye submeshes in magenta).");
        }

        ImGui::End();
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
        if (!mInitialized)
        {
            renderer.EndFrame();
            return;
        }

        if (mEnabled)
        {
            ImGui::Render();
            renderer.EndFrame([&] {
                ImGui_ImplVulkan_RenderDrawData(
                    ImGui::GetDrawData(),
                    static_cast<VkCommandBuffer>(
                        fra::Advanced(renderer).NativeCommandBuffer()));
            });
            return;
        }

        renderer.EndFrame();
    }
} // namespace FreyaExamples
