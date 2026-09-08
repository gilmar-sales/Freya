#pragma once

#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/FrameGpuTiming.hpp"
#include "Freya/Core/Limits.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <functional>
#include <memory>
#include <span>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct SceneInstanceUpload;
    class RendererAdvanced;
    class Scene;

    /**
     * @brief Per-window renderer façade (application tier).
     *
     * Canonical frame path:
     *   BeginFrame() → Camera::Apply / lights → Scene::Upload →
     *   EndFrame() or EndFrame(uiDraw).
     *
     * For frame stages, ImGui natives, cull dumps, and GPU animation, use
     * RendererAdvanced (via <Freya/Advanced.hpp>).
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
         * viewport target is active, then Present.
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

        void UploadBoneMatrices(std::span<const glm::mat4> bones);
        void UploadBoneMatrices(
            std::uint32_t boneOffset, std::span<const glm::mat4> bones);

        void RequestPick(std::uint32_t x, std::uint32_t y);
        bool TryConsumePickResult(std::uint32_t& outEntityId);

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
         * @brief GPU ms for each frame stage from the previous finished frame.
         * @return false when timestamp queries are unavailable or not ready.
         */
        bool PollFrameGpuTiming(FrameGpuTimingSample& out);

        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const;
        [[nodiscard]] std::uint32_t GetFrameCount() const;

      private:
        friend class RendererBuilder;
        friend class RendererAdvanced;
        friend class Scene;

        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads);
        void PatchSceneInstances(std::span<const SceneInstanceUpload> uploads);
        void CommitSceneFrame();

        [[nodiscard]] Impl*       ImplPtr() { return mImpl.get(); }
        [[nodiscard]] const Impl* ImplPtr() const { return mImpl.get(); }

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
