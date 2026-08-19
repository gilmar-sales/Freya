#include "Freya/Internal/RendererImpl.hpp"

namespace FREYA_NAMESPACE
{
    Renderer::Renderer(std::unique_ptr<Impl> impl) : mImpl(std::move(impl))
    {
    }

    Renderer::~Renderer() = default;

    void Renderer::BeginFrame()
    {
        mImpl->BeginFrame();
    }
    void Renderer::EndScene()
    {
        mImpl->EndScene();
    }
    void Renderer::Present()
    {
        mImpl->Present();
    }
    void Renderer::EndFrame()
    {
        mImpl->EndFrame();
    }
    void Renderer::RebuildSwapChain()
    {
        mImpl->RebuildSwapChain();
    }

    void Renderer::SetShadowQuality(const ShadowQuality quality)
    {
        mImpl->SetShadowQuality(quality);
    }

    ShadowQuality Renderer::GetShadowQuality() const
    {
        return mImpl->mShadowQuality;
    }

    void Renderer::SetSsaoQuality(const SsaoQuality quality)
    {
        mImpl->SetSsaoQuality(quality);
    }

    SsaoQuality Renderer::GetSsaoQuality() const
    {
        return mImpl->mSsaoQuality;
    }

    void Renderer::SetSsaoDebugView(const SsaoDebugView view)
    {
        mImpl->SetSsaoDebugView(view);
    }

    void Renderer::SetShadowDebug(const bool enable)
    {
        mImpl->mFreyaOptions->shadowDebug = enable;
    }

    bool Renderer::GetShadowDebug() const
    {
        return mImpl->mFreyaOptions->shadowDebug;
    }

    SsaoDebugView Renderer::GetSsaoDebugView() const
    {
        return mImpl->mFreyaOptions->ssaoDebugView;
    }

    void Renderer::SetSsaoRadius(const float radius)
    {
        mImpl->SetSsaoRadius(radius);
    }

    void Renderer::SetSsaoBias(const float bias)
    {
        mImpl->SetSsaoBias(bias);
    }

    void Renderer::SetSsaoPower(const float power)
    {
        mImpl->SetSsaoPower(power);
    }

    void Renderer::SetSsaoIntensity(const float intensity)
    {
        mImpl->SetSsaoIntensity(intensity);
    }

    float Renderer::GetSsaoRadius() const
    {
        return mImpl->mFreyaOptions->ssaoRadius;
    }

    float Renderer::GetSsaoBias() const
    {
        return mImpl->mFreyaOptions->ssaoBias;
    }

    float Renderer::GetSsaoPower() const
    {
        return mImpl->mFreyaOptions->ssaoPower;
    }

    float Renderer::GetSsaoIntensity() const
    {
        return mImpl->mFreyaOptions->ssaoIntensity;
    }

    void Renderer::SetTaaQuality(const TaaQuality quality)
    {
        mImpl->SetTaaQuality(quality);
    }

    TaaQuality Renderer::GetTaaQuality() const
    {
        return mImpl->mTaaQuality;
    }

    void Renderer::SetBloomQuality(const BloomQuality quality)
    {
        mImpl->SetBloomQuality(quality);
    }

    BloomQuality Renderer::GetBloomQuality() const
    {
        return mImpl->mBloomQuality;
    }

    bool Renderer::GetVSync() const
    {
        return mImpl->mFreyaOptions->vSync;
    }

    void Renderer::SetVSync(const bool vSync)
    {
        mImpl->SetVSync(vSync);
    }

    void Renderer::SetSamples(const std::uint32_t samples)
    {
        mImpl->SetSamples(samples);
    }

    std::uint32_t Renderer::GetSamples() const
    {
        return mImpl->mFreyaOptions->sampleCount;
    }

    float Renderer::GetDrawDistance() const
    {
        return mImpl->mFreyaOptions->drawDistance;
    }

    void Renderer::SetDrawDistance(const float drawDistance)
    {
        mImpl->SetDrawDistance(drawDistance);
    }

    void Renderer::UploadSceneInstances(
        const std::span<const SceneInstanceUpload> uploads)
    {
        mImpl->UploadSceneInstances(uploads);
    }

    void Renderer::Draw(const std::uint32_t meshId,
                        const std::uint32_t materialId,
                        const std::uint32_t entityId, const bool castShadows)
    {
        mImpl->Draw(meshId, materialId, entityId, castShadows);
    }

    void Renderer::DrawInstanced(const std::uint32_t meshId,
                                 const std::uint32_t materialId,
                                 const size_t        instanceCount,
                                 const size_t        firstInstance,
                                 const bool          castShadows,
                                 const std::uint32_t entityId)
    {
        mImpl->DrawInstanced(meshId, materialId, instanceCount, firstInstance,
                             castShadows, entityId);
    }

    void Renderer::SetInstanceModels(const glm::mat4*  models,
                                     const std::size_t count)
    {
        mImpl->SetInstanceModels(models, count);
    }

    void Renderer::UploadBoneMatrices(const std::span<const glm::mat4> bones)
    {
        mImpl->UploadBoneMatrices(bones);
    }

    void Renderer::ClearDrawCommands()
    {
        mImpl->ClearDrawCommands();
    }

    void Renderer::ExecuteDrawCommands(const bool bindMaterials)
    {
        mImpl->ExecuteDrawCommands(bindMaterials);
    }

    void Renderer::ExecutePickDrawCommands()
    {
        mImpl->ExecutePickDrawCommands();
    }

    void Renderer::DispatchCull(const glm::mat4& viewProj, const CullMode mode)
    {
        mImpl->DispatchCull(viewProj, mode);
    }

    void Renderer::RequestPick(const std::uint32_t x, const std::uint32_t y)
    {
        mImpl->RequestPick(x, y);
    }

    bool Renderer::TryConsumePickResult(std::uint32_t& outEntityId)
    {
        return mImpl->TryConsumePickResult(outEntityId);
    }

    bool Renderer::InsertFrameStage(const char* beforeName, FrameStagePtr stage)
    {
        return mImpl->InsertFrameStage(beforeName, std::move(stage));
    }

    bool Renderer::ReplaceFrameStage(const char* name, FrameStagePtr stage)
    {
        return mImpl->ReplaceFrameStage(name, std::move(stage));
    }

    const std::vector<FrameStagePtr>& Renderer::GetFrameStages() const
    {
        return mImpl->mFrameStages;
    }

    glm::mat4 Renderer::MakeProjection(const float fovRadians,
                                       const float aspect, const float near,
                                       const float far) const
    {
        return mImpl->MakeProjection(fovRadians, aspect, near, far);
    }

    void Renderer::ClearProjections()
    {
        mImpl->ClearProjections();
    }

    glm::mat4 Renderer::CalculateProjectionMatrix(const float near,
                                                  const float far) const
    {
        return mImpl->CalculateProjectionMatrix(near, far);
    }

    const ProjectionUniformBuffer& Renderer::GetCurrentProjection() const
    {
        return mImpl->mCurrentProjection;
    }

    void Renderer::UpdateProjection(
        ProjectionUniformBuffer& projectionUniformBuffer)
    {
        mImpl->UpdateProjection(projectionUniformBuffer);
    }

    void Renderer::UpdateCamera(const glm::vec3& position,
                                const glm::vec3& target, const glm::vec3& up)
    {
        mImpl->UpdateCamera(position, target, up);
    }

    void Renderer::SetAmbient(const glm::vec3& color, const float intensity)
    {
        mImpl->SetAmbient(color, intensity);
    }

    void Renderer::SetDebugDrawEnabled(const bool enabled)
    {
        mImpl->mDebugDrawEnabled = enabled;
    }

    bool Renderer::IsDebugDrawEnabled() const
    {
        return mImpl->mDebugDrawEnabled;
    }

    DebugDraw& Renderer::GetDebugDraw()
    {
        return mImpl->mDebugDraw;
    }

    BillboardDraw& Renderer::GetBillboardDraw()
    {
        return mImpl->mBillboardDraw;
    }

    void Renderer::SetGpuAnimEnabled(const bool enabled)
    {
        if (mImpl->mGpuAnimPass)
            mImpl->mGpuAnimPass->SetEnabled(enabled);
    }

    bool Renderer::IsGpuAnimEnabled() const
    {
        return mImpl->mGpuAnimPass && mImpl->mGpuAnimPass->IsEnabled();
    }

    GpuAnimPass* Renderer::GetGpuAnimPass()
    {
        return mImpl->mGpuAnimPass.get();
    }

    void Renderer::RebuildGpuAnimPass()
    {
        mImpl->RebuildGpuAnimPass();
    }

    bool Renderer::ReadbackGpuAnimBones(const std::uint32_t frameIndex,
                                        const std::uint32_t boneOffset,
                                        std::span<glm::mat4>
                                            out)
    {
        return mImpl->ReadbackGpuAnimBones(frameIndex, boneOffset, out);
    }

    bool Renderer::DispatchGpuAnimImmediate(
        const std::span<const GpuAnimInstance> instances,
        const std::uint32_t                    frameIndex)
    {
        return mImpl->DispatchGpuAnimImmediate(instances, frameIndex);
    }

    void Renderer::SetGpuAnimJointExtract(
        const std::span<const GpuJointExtractRequest> requests)
    {
        mImpl->SetGpuAnimJointExtract(requests);
    }

    bool Renderer::PollGpuAnimJointExtract(std::span<GpuJointExtractSample> out,
                                           std::uint32_t* outCount)
    {
        return mImpl->PollGpuAnimJointExtract(out, outCount);
    }

    bool Renderer::PollGpuAnimTiming(GpuAnimTimingSample& out)
    {
        return mImpl->PollGpuAnimTiming(out);
    }

    std::uint32_t Renderer::GetCurrentFrameIndex() const
    {
        return mImpl->mSwapChain->GetCurrentFrameIndex();
    }

    std::uint32_t Renderer::GetFrameCount() const
    {
        return mImpl->mSwapChain->GetFrameCount();
    }

} // namespace FREYA_NAMESPACE
