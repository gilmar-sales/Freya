#pragma once

#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/RendererUi.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <memory>
#include <span>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
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
        void EndFrame();

        void RebuildSwapChain();

        void                        SetShadowQuality(ShadowQuality quality);
        [[nodiscard]] ShadowQuality GetShadowQuality() const;

        void                      SetSsaoQuality(SsaoQuality quality);
        [[nodiscard]] SsaoQuality GetSsaoQuality() const;

        void                        SetSsaoDebugView(SsaoDebugView view);
        void                        SetShadowDebug(bool enable);
        [[nodiscard]] bool          GetShadowDebug() const;
        [[nodiscard]] SsaoDebugView GetSsaoDebugView() const;

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

        void Draw(std::uint32_t meshId,
                  std::uint32_t materialId,
                  std::uint32_t entityId    = kPickMissId,
                  bool          castShadows = true);

        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0,
                           bool          castShadows   = true,
                           std::uint32_t entityId      = kPickMissId);

        void SetInstanceModels(const glm::mat4* models, std::size_t count);

        void UploadBoneMatrices(std::span<const glm::mat4> bones);

        void RequestPick(std::uint32_t x, std::uint32_t y);
        bool TryConsumePickResult(std::uint32_t& outEntityId);

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
         * with EndUI() and call Present() afterwards. BeginFrame()/EndFrame()
         * already honours the UI pass; this seam exists for apps that split the
         * frame (BeginFrame -> ... -> EndScene -> BeginUI -> draw -> EndUI ->
         * Present).
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

        void SetAmbient(const glm::vec3& color, float intensity);

        void                     SetDebugDrawEnabled(bool enabled);
        [[nodiscard]] bool       IsDebugDrawEnabled() const;
        [[nodiscard]] DebugDraw& GetDebugDraw();

        [[nodiscard]] BillboardDraw& GetBillboardDraw();

        void               SetGpuAnimEnabled(bool enabled);
        [[nodiscard]] bool IsGpuAnimEnabled() const;

        void RebuildGpuAnimPass();

        void SetGpuAnimCopyPrevBones(bool enabled);
        void UploadGpuAnimInstances(std::span<const GpuAnimInstance> instances);
        void CaptureGpuAnimDebugSnapshot(GpuAnimDebugSnapshot& out) const;
        [[nodiscard]] std::uint32_t FindGpuAnimClipSlot(
            std::uint64_t key) const;
        [[nodiscard]] std::uint32_t EnsureGpuAnimClipResident(
            std::uint64_t key, const BakedClip& clip);
        [[nodiscard]] std::uint32_t GetGpuAnimResidentClipCount() const;
        [[nodiscard]] std::uint32_t GetGpuAnimJointsPerClipSlot() const;

        void UploadGpuAnimSkeleton(const GpuSkeletonPack& skeleton);
        void ResetGpuAnimClipCache();
        bool UploadGpuAnimClipSlot(std::uint32_t slot, std::uint64_t key,
                                   const BakedClip& clip);
        void PinGpuAnimClipSlot(std::uint32_t slot, bool pinned);
        void UploadGpuAnimBoneMask(std::span<const float> weights);
        void UploadGpuAnimRestJoints(std::span<const GpuFloatJoint> joints);
        void UploadGpuAnimRestJoints(std::span<const GpuQuantJoint> joints);
        void SetGpuAnimRigIndices(
            std::uint32_t lookJoint, std::uint32_t ikRoot, std::uint32_t ikMid,
            std::uint32_t ikTip, std::uint32_t rootJoint,
            glm::vec3 lookLocalForward = { 0.f, 0.f, 1.f },
            float lookMaxYawRad = 1.2f, float lookMaxPitchRad = 0.8f);

        bool ReadbackGpuAnimBones(std::uint32_t frameIndex,
                                  std::uint32_t boneOffset,
                                  std::span<glm::mat4>
                                      out);

        bool DispatchGpuAnimImmediate(
            std::span<const GpuAnimInstance> instances,
            std::uint32_t                    frameIndex);

        void SetGpuAnimJointExtract(
            std::span<const GpuJointExtractRequest> requests);

        bool PollGpuAnimJointExtract(std::span<GpuJointExtractSample> out,
                                     std::uint32_t* outCount = nullptr);

        bool PollGpuAnimTiming(GpuAnimTimingSample& out);

        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const;
        [[nodiscard]] std::uint32_t GetFrameCount() const;

      private:
        friend class RendererBuilder;

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
