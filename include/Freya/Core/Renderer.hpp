#pragma once

#include "Freya/Asset/CullFrameDump.hpp"
#include "Freya/Asset/SceneInstanceUpload.hpp"
#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/FrameGpuTiming.hpp"
#include "Freya/Core/GpuAnimationSystem.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/RendererUi.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <functional>
#include <memory>
#include <span>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Per-window renderer façade.
     *
     * Canonical frame path:
     *   BeginFrame() → UpdateCamera / lights / UploadSceneInstances →
     *   EndFrame() or EndFrame(uiDraw).
     *
     * Prefer Scene::Upload and Camera::Apply for app code. Prefer
     * GpuAnimation() for GPU skinning. Draw / DrawInstanced /
     * SetInstanceModels are deprecated.
     */
    class Renderer
    {
      public:
        class Impl;

        explicit Renderer(std::unique_ptr<Impl> impl);
        ~Renderer();

        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;

        void BeginFrame();
        void EndScene();
        void Present();

        /**
         * @brief EndScene + Present (no UI pass). Prefer this when not drawing
         * Dear ImGui into the swapchain.
         */
        void EndFrame();

        /**
         * @brief EndScene, run @p uiDraw inside the swapchain UI pass when a
         * viewport target is active, then Present. Replaces the manual
         * EndScene + BeginUI + EndUI + Present sequence.
         */
        void EndFrame(const std::function<void()>& uiDraw);

        void RebuildSwapChain();

        void                        SetShadowQuality(ShadowQuality quality);
        [[nodiscard]] ShadowQuality GetShadowQuality() const;

        void                      SetSsaoQuality(SsaoQuality quality);
        [[nodiscard]] SsaoQuality GetSsaoQuality() const;

        void SetDeferredDebugView(DeferredDebugView view);
        [[nodiscard]] DeferredDebugView GetDeferredDebugView() const;

        void SetSsaoRadius(float radius);
        void SetSsaoBias(float bias);
        void SetSsaoPower(float power);
        void SetSsaoIntensity(float intensity);

        [[nodiscard]] float GetSsaoRadius() const;
        [[nodiscard]] float GetSsaoBias() const;
        [[nodiscard]] float GetSsaoPower() const;
        [[nodiscard]] float GetSsaoIntensity() const;

        void                     SetTaaQuality(TaaQuality quality);
        [[nodiscard]] TaaQuality GetTaaQuality() const;

        void                       SetBloomQuality(BloomQuality quality);
        [[nodiscard]] BloomQuality GetBloomQuality() const;

        [[nodiscard]] bool GetVSync() const;
        void               SetVSync(bool vSync);

        void                        SetSamples(std::uint32_t samples);
        [[nodiscard]] std::uint32_t GetSamples() const;

        [[nodiscard]] float GetDrawDistance() const;
        void                SetDrawDistance(float drawDistance);

        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads);

        [[deprecated("Use UploadSceneInstances or Scene::Upload")]]
        void Draw(std::uint32_t meshId,
                  std::uint32_t materialId,
                  std::uint32_t entityId    = kPickMissId,
                  bool          castShadows = true);

        [[deprecated("Use UploadSceneInstances or Scene::Upload")]]
        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0,
                           bool          castShadows   = true,
                           std::uint32_t entityId      = kPickMissId);

        [[deprecated("Use UploadSceneInstances or Scene::Upload")]]
        void SetInstanceModels(const glm::mat4* models, std::size_t count);

        void UploadBoneMatrices(std::span<const glm::mat4> bones);

        void RequestPick(std::uint32_t x, std::uint32_t y);
        bool TryConsumePickResult(std::uint32_t& outEntityId);

        /**
         * @brief One-shot: capture cull inputs/outputs after the next EndScene.
         *
         * Call TryConsumeCullFrameDump on a later frame (after Present / FiF
         * wait) to retrieve the snapshot.
         */
        void RequestCullFrameDump();

        /**
         * @brief Pop a pending cull-frame dump filled after GPU work completed.
         * @return false when no dump is ready.
         */
        bool TryConsumeCullFrameDump(CullFrameSnapshot& out);

        bool InsertFrameStage(const char* beforeName, FrameStagePtr stage);
        bool ReplaceFrameStage(const char* name, FrameStagePtr stage);

        /**
         * @brief Current frame command buffer as a Vulkan handle
         * (VkCommandBuffer).
         */
        [[nodiscard]] void* NativeCommandBuffer();

        /**
         * @brief Logical device as a Vulkan handle (VkDevice).
         */
        [[nodiscard]] void* NativeDevice();

        /**
         * @brief Opens the swapchain UI render pass so the app can draw its
         * Dear ImGui frame into it.
         *
         * Only succeeds while an offscreen viewport target is set; otherwise it
         * returns false and the scene presents directly to the swapchain. Pair
         * with EndUI() and call Present() afterwards. Prefer EndFrame(uiDraw).
         */
        [[nodiscard]] bool BeginUI();

        /**
         * @brief Closes the swapchain UI render pass opened by BeginUI().
         *
         * Present() closes an open UI pass automatically, so calling EndUI() is
         * optional unless the app wants to end the pass before Present().
         */
        void EndUI();

        /**
         * @brief Opaque Vulkan/SDL handles for initializing the Dear ImGui
         * back-ends in the app (ImGui_ImplVulkan / ImGui_ImplSDL3).
         */
        [[nodiscard]] ImGuiNativeHandles GetImGuiNativeHandles();

        /**
         * @brief Offscreen composite viewport (VkImageView + VkSampler) for
         * ImGui::Image(). Valid only while a viewport target is set.
         */
        [[nodiscard]] ImGuiViewportImage GetViewportImage();

        /**
         * @brief Renders subsequent frames into an offscreen viewport target of
         * the given pixel size instead of the swapchain. The composite step
         * draws offscreen and the swapchain UI pass is opened for ImGui.
         *
         * The target is owned by the renderer and resized when the swapchain
         * changes. Returns false if target creation failed.
         */
        [[nodiscard]] bool SetViewportTarget(std::uint32_t width,
                                             std::uint32_t height);

        /**
         * @brief Clears the offscreen viewport target, restoring direct
         * presentation to the swapchain.
         */
        void ClearOutputTarget();

        glm::mat4 MakeProjection(float fovRadians, float aspect, float near,
                                 float far) const;

        void      ClearProjections();
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        void UpdateCamera(const glm::vec3& position,
                          const glm::vec3& target,
                          const glm::vec3& up);

        void UpdateCamera(const glm::vec3& position,
                          const glm::vec3& target,
                          const glm::vec3& up,
                          float            fovRadians,
                          float            nearPlane,
                          float            farPlane);

        void SetAmbient(const glm::vec3& color, float intensity);

        void                     SetDebugDrawEnabled(bool enabled);
        [[nodiscard]] bool       IsDebugDrawEnabled() const;
        [[nodiscard]] DebugDraw& GetDebugDraw();

        [[nodiscard]] BillboardDraw& GetBillboardDraw();

        /**
         * @brief GPU animation / crowd skinning subsystem.
         */
        [[nodiscard]] GpuAnimationSystem&       GpuAnimation();
        [[nodiscard]] const GpuAnimationSystem& GpuAnimation() const;

        /**
         * @brief GPU ms for each frame stage from the previous finished frame.
         * @return false when timestamp queries are unavailable or not ready.
         */
        bool PollFrameGpuTiming(FrameGpuTimingSample& out);

        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const;
        [[nodiscard]] std::uint32_t GetFrameCount() const;

      private:
        friend class RendererBuilder;

        std::unique_ptr<Impl> mImpl;
        GpuAnimationSystem    mGpuAnim;
    };

} // namespace FREYA_NAMESPACE
