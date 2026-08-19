#pragma once

#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/GpuAnimPass.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Stored draw command for reuse across rendering passes.
     */
    struct DrawCommand
    {
        std::uint32_t meshId;
        std::uint32_t materialId;
        std::uint32_t instanceCount;
        std::uint32_t firstInstance;
        std::uint32_t entityId    = kPickMissId;
        bool          castShadows = true;
    };

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

        void ClearDrawCommands();
        void ExecuteDrawCommands(bool bindMaterials = true);
        void ExecutePickDrawCommands();
        void DispatchCull(const glm::mat4& viewProj, CullMode mode);

        void RequestPick(std::uint32_t x, std::uint32_t y);
        bool TryConsumePickResult(std::uint32_t& outEntityId);

        bool InsertFrameStage(const char* beforeName, FrameStagePtr stage);
        bool ReplaceFrameStage(const char* name, FrameStagePtr stage);

        [[nodiscard]] const std::vector<FrameStagePtr>& GetFrameStages() const;

        glm::mat4 MakeProjection(float fovRadians, float aspect, float near,
                                 float far) const;

        void      ClearProjections();
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        [[nodiscard]] const ProjectionUniformBuffer& GetCurrentProjection()
            const;

        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);

        void UpdateCamera(const glm::vec3& position,
                          const glm::vec3& target,
                          const glm::vec3& up);

        void SetAmbient(const glm::vec3& color, float intensity);

        void                     SetDebugDrawEnabled(bool enabled);
        [[nodiscard]] bool       IsDebugDrawEnabled() const;
        [[nodiscard]] DebugDraw& GetDebugDraw();

        [[nodiscard]] BillboardDraw& GetBillboardDraw();

        void                       SetGpuAnimEnabled(bool enabled);
        [[nodiscard]] bool         IsGpuAnimEnabled() const;
        [[nodiscard]] GpuAnimPass* GetGpuAnimPass();

        void RebuildGpuAnimPass();

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
