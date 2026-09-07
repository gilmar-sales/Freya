#include "Freya/Internal/RendererImpl.hpp"

#include "Freya/Core/GpuAnimPass.hpp"

#include <functional>

namespace FREYA_NAMESPACE
{
    Renderer::Renderer(std::unique_ptr<Impl> impl) :
        mImpl(std::move(impl)), mGpuAnim(mImpl.get())
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

    void Renderer::EndFrame(const std::function<void()>& uiDraw)
    {
        mImpl->EndScene();
        if (uiDraw && mImpl->BeginUI())
        {
            uiDraw();
            mImpl->EndUI();
        }
        mImpl->Present();
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

    void Renderer::SetDeferredDebugView(const DeferredDebugView view)
    {
        mImpl->SetDeferredDebugView(view);
    }

    DeferredDebugView Renderer::GetDeferredDebugView() const
    {
        return mImpl->mFreyaOptions->deferredDebugView;
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

    void Renderer::RequestPick(const std::uint32_t x, const std::uint32_t y)
    {
        mImpl->RequestPick(x, y);
    }

    bool Renderer::TryConsumePickResult(std::uint32_t& outEntityId)
    {
        return mImpl->TryConsumePickResult(outEntityId);
    }

    void Renderer::RequestCullFrameDump()
    {
        mImpl->RequestCullFrameDump();
    }

    bool Renderer::TryConsumeCullFrameDump(CullFrameSnapshot& out)
    {
        return mImpl->TryConsumeCullFrameDump(out);
    }

    bool Renderer::InsertFrameStage(const char* beforeName, FrameStagePtr stage)
    {
        return mImpl->InsertFrameStage(beforeName, std::move(stage));
    }

    bool Renderer::ReplaceFrameStage(const char* name, FrameStagePtr stage)
    {
        return mImpl->ReplaceFrameStage(name, std::move(stage));
    }

    void* Renderer::NativeCommandBuffer()
    {
        return mImpl->NativeCommandBuffer();
    }

    void* Renderer::NativeDevice()
    {
        return mImpl->NativeDevice();
    }

    bool Renderer::BeginUI()
    {
        return mImpl->BeginUI();
    }

    void Renderer::EndUI()
    {
        mImpl->EndUI();
    }

    ImGuiNativeHandles Renderer::GetImGuiNativeHandles()
    {
        return mImpl->GetImGuiNativeHandles();
    }

    ImGuiViewportImage Renderer::GetViewportImage()
    {
        return mImpl->GetViewportImage();
    }

    bool Renderer::SetViewportTarget(const std::uint32_t width,
                                     const std::uint32_t height)
    {
        return mImpl->SetViewportTarget(width, height);
    }

    void Renderer::ClearOutputTarget()
    {
        mImpl->ClearOutputTarget();
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

    void Renderer::UpdateCamera(const glm::vec3& position,
                                const glm::vec3& target, const glm::vec3& up)
    {
        mImpl->UpdateCamera(position, target, up);
    }

    void Renderer::UpdateCamera(
        const glm::vec3& position, const glm::vec3& target, const glm::vec3& up,
        const float fovRadians, const float nearPlane, const float farPlane)
    {
        mImpl->UpdateCamera(position, target, up, fovRadians, nearPlane,
                            farPlane);
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

    GpuAnimationSystem& Renderer::GpuAnimation()
    {
        return mGpuAnim;
    }

    const GpuAnimationSystem& Renderer::GpuAnimation() const
    {
        return mGpuAnim;
    }

    bool Renderer::PollFrameGpuTiming(FrameGpuTimingSample& out)
    {
        return mImpl->PollFrameGpuTiming(out);
    }

    std::uint32_t Renderer::GetCurrentFrameIndex() const
    {
        return mImpl->mSwapChain->GetCurrentFrameIndex();
    }

    std::uint32_t Renderer::GetFrameCount() const
    {
        return mImpl->mSwapChain->GetFrameCount();
    }

    // --- GpuAnimationSystem (forwards into Renderer::Impl) ---

    void GpuAnimationSystem::SetEnabled(const bool enabled)
    {
        auto* impl = static_cast<Renderer::Impl*>(mImpl);
        if (impl->mGpuAnimPass)
            impl->mGpuAnimPass->SetEnabled(enabled);
    }

    bool GpuAnimationSystem::IsEnabled() const
    {
        auto* impl = static_cast<Renderer::Impl*>(mImpl);
        return impl->mGpuAnimPass && impl->mGpuAnimPass->IsEnabled();
    }

    void GpuAnimationSystem::RebuildPass()
    {
        static_cast<Renderer::Impl*>(mImpl)->RebuildGpuAnimPass();
    }

    void GpuAnimationSystem::SetCopyPrevBones(const bool enabled)
    {
        static_cast<Renderer::Impl*>(mImpl)->SetGpuAnimCopyPrevBones(enabled);
    }

    void GpuAnimationSystem::UploadInstances(
        const std::span<const GpuAnimInstance> instances)
    {
        static_cast<Renderer::Impl*>(mImpl)->UploadGpuAnimInstances(instances);
    }

    void GpuAnimationSystem::CaptureDebugSnapshot(
        GpuAnimDebugSnapshot& out) const
    {
        static_cast<Renderer::Impl*>(mImpl)->CaptureGpuAnimDebugSnapshot(out);
    }

    std::uint32_t GpuAnimationSystem::FindClipSlot(
        const std::uint64_t key) const
    {
        return static_cast<Renderer::Impl*>(mImpl)->FindGpuAnimClipSlot(key);
    }

    std::uint32_t GpuAnimationSystem::EnsureClipResident(
        const std::uint64_t key, const BakedClip& clip)
    {
        return static_cast<Renderer::Impl*>(mImpl)->EnsureGpuAnimClipResident(
            key, clip);
    }

    std::uint32_t GpuAnimationSystem::GetResidentClipCount() const
    {
        return static_cast<Renderer::Impl*>(mImpl)
            ->GetGpuAnimResidentClipCount();
    }

    std::uint32_t GpuAnimationSystem::GetJointsPerClipSlot() const
    {
        return static_cast<Renderer::Impl*>(mImpl)
            ->GetGpuAnimJointsPerClipSlot();
    }

    void GpuAnimationSystem::UploadSkeleton(const GpuSkeletonPack& skeleton)
    {
        static_cast<Renderer::Impl*>(mImpl)->UploadGpuAnimSkeleton(skeleton);
    }

    void GpuAnimationSystem::ResetClipCache()
    {
        static_cast<Renderer::Impl*>(mImpl)->ResetGpuAnimClipCache();
    }

    bool GpuAnimationSystem::UploadClipSlot(const std::uint32_t slot,
                                            const std::uint64_t key,
                                            const BakedClip&    clip)
    {
        return static_cast<Renderer::Impl*>(mImpl)->UploadGpuAnimClipSlot(
            slot, key, clip);
    }

    void GpuAnimationSystem::PinClipSlot(const std::uint32_t slot,
                                         const bool          pinned)
    {
        static_cast<Renderer::Impl*>(mImpl)->PinGpuAnimClipSlot(slot, pinned);
    }

    void GpuAnimationSystem::UploadBoneMask(
        const std::span<const float> weights)
    {
        static_cast<Renderer::Impl*>(mImpl)->UploadGpuAnimBoneMask(weights);
    }

    void GpuAnimationSystem::UploadRestJoints(
        const std::span<const GpuFloatJoint> joints)
    {
        static_cast<Renderer::Impl*>(mImpl)->UploadGpuAnimRestJoints(joints);
    }

    void GpuAnimationSystem::UploadRestJoints(
        const std::span<const GpuQuantJoint> joints)
    {
        static_cast<Renderer::Impl*>(mImpl)->UploadGpuAnimRestJoints(joints);
    }

    void GpuAnimationSystem::SetRigIndices(
        const std::uint32_t lookJoint, const std::uint32_t ikRoot,
        const std::uint32_t ikMid, const std::uint32_t ikTip,
        const std::uint32_t rootJoint, const glm::vec3 lookLocalForward,
        const float lookMaxYawRad, const float lookMaxPitchRad)
    {
        static_cast<Renderer::Impl*>(mImpl)->SetGpuAnimRigIndices(
            lookJoint, ikRoot, ikMid, ikTip, rootJoint, lookLocalForward,
            lookMaxYawRad, lookMaxPitchRad);
    }

    bool GpuAnimationSystem::ReadbackBones(const std::uint32_t frameIndex,
                                           const std::uint32_t boneOffset,
                                           std::span<glm::mat4>
                                               out)
    {
        return static_cast<Renderer::Impl*>(mImpl)->ReadbackGpuAnimBones(
            frameIndex, boneOffset, out);
    }

    bool GpuAnimationSystem::DispatchImmediate(
        const std::span<const GpuAnimInstance> instances,
        const std::uint32_t                    frameIndex)
    {
        return static_cast<Renderer::Impl*>(mImpl)->DispatchGpuAnimImmediate(
            instances, frameIndex);
    }

    void GpuAnimationSystem::SetJointExtract(
        const std::span<const GpuJointExtractRequest> requests)
    {
        static_cast<Renderer::Impl*>(mImpl)->SetGpuAnimJointExtract(requests);
    }

    bool GpuAnimationSystem::PollJointExtract(
        std::span<GpuJointExtractSample> out, std::uint32_t* outCount)
    {
        return static_cast<Renderer::Impl*>(mImpl)->PollGpuAnimJointExtract(
            out, outCount);
    }

    bool GpuAnimationSystem::PollTiming(GpuAnimTimingSample& out)
    {
        return static_cast<Renderer::Impl*>(mImpl)->PollGpuAnimTiming(out);
    }

} // namespace FREYA_NAMESPACE
