#include "Freya/Internal/GpuAnimPassImpl.hpp"

namespace FREYA_NAMESPACE
{
    GpuAnimPass::GpuAnimPass(std::unique_ptr<Impl> impl) :
        mImpl(std::move(impl))
    {
    }

    GpuAnimPass::~GpuAnimPass() = default;

    void GpuAnimPass::SetEnabled(const bool enabled)
    {
        mImpl->SetEnabled(enabled);
    }

    bool GpuAnimPass::IsEnabled() const
    {
        return mImpl->IsEnabled();
    }

    bool GpuAnimPass::UsesQuantizedJoints() const
    {
        return mImpl->UsesQuantizedJoints();
    }

    std::uint32_t GpuAnimPass::MaxBakedJoints() const
    {
        return mImpl->MaxBakedJoints();
    }

    void GpuAnimPass::SetCopyPrevBones(const bool enabled)
    {
        mImpl->SetCopyPrevBones(enabled);
    }

    void GpuAnimPass::SetRigIndices(
        const std::uint32_t lookJoint, const std::uint32_t ikRoot,
        const std::uint32_t ikMid, const std::uint32_t ikTip,
        const std::uint32_t rootJoint, const glm::vec3 lookLocalForward,
        const float lookMaxYawRad, const float lookMaxPitchRad)
    {
        mImpl->SetRigIndices(lookJoint, ikRoot, ikMid, ikTip, rootJoint,
                             lookLocalForward, lookMaxYawRad, lookMaxPitchRad);
    }

    void GpuAnimPass::SetLookClamp(const float maxYawRad,
                                   const float maxPitchRad)
    {
        mImpl->SetLookClamp(maxYawRad, maxPitchRad);
    }

    std::uint32_t GpuAnimPass::GetDefaultLookJoint() const
    {
        return mImpl->GetDefaultLookJoint();
    }
    std::uint32_t GpuAnimPass::GetDefaultIkRoot() const
    {
        return mImpl->GetDefaultIkRoot();
    }
    std::uint32_t GpuAnimPass::GetDefaultIkMid() const
    {
        return mImpl->GetDefaultIkMid();
    }
    std::uint32_t GpuAnimPass::GetDefaultIkTip() const
    {
        return mImpl->GetDefaultIkTip();
    }
    glm::vec3 GpuAnimPass::GetDefaultLookLocalForward() const
    {
        return mImpl->GetDefaultLookLocalForward();
    }
    float GpuAnimPass::GetDefaultLookMaxYaw() const
    {
        return mImpl->GetDefaultLookMaxYaw();
    }
    float GpuAnimPass::GetDefaultLookMaxPitch() const
    {
        return mImpl->GetDefaultLookMaxPitch();
    }

    void GpuAnimPass::SetJointExtractList(
        const std::span<const GpuJointExtractRequest> reqs)
    {
        mImpl->SetJointExtractList(reqs);
    }

    bool GpuAnimPass::PollJointExtract(const std::uint32_t frameIndex,
                                       std::span<GpuJointExtractSample>
                                                      out,
                                       std::uint32_t* outCount) const
    {
        return mImpl->PollJointExtract(frameIndex, out, outCount);
    }

    bool GpuAnimPass::PollTiming(const std::uint32_t  frameIndex,
                                 GpuAnimTimingSample& out) const
    {
        return mImpl->PollTiming(frameIndex, out);
    }

    bool GpuAnimPass::HasTimestampQueries() const
    {
        return mImpl->HasTimestampQueries();
    }

    void GpuAnimPass::UploadSkeleton(const GpuSkeletonPack& skeleton)
    {
        mImpl->UploadSkeleton(skeleton);
    }

    void GpuAnimPass::UploadBakes(const GpuBakePack& pack)
    {
        mImpl->UploadBakes(pack);
    }

    void GpuAnimPass::ResetClipCache()
    {
        mImpl->ResetClipCache();
    }

    bool GpuAnimPass::UploadClipSlot(const std::uint32_t slot,
                                     const std::uint64_t key,
                                     const BakedClip&    clip)
    {
        return mImpl->UploadClipSlot(slot, key, clip);
    }

    std::uint32_t GpuAnimPass::EnsureClipResident(const std::uint64_t key,
                                                  const BakedClip&    clip)
    {
        return mImpl->EnsureClipResident(key, clip);
    }

    void GpuAnimPass::PinClipSlot(const std::uint32_t slot, const bool pinned)
    {
        mImpl->PinClipSlot(slot, pinned);
    }

    void GpuAnimPass::TouchClipSlot(const std::uint32_t slot)
    {
        mImpl->TouchClipSlot(slot);
    }

    void GpuAnimPass::EvictClipSlot(const std::uint32_t slot)
    {
        mImpl->EvictClipSlot(slot);
    }

    std::uint32_t GpuAnimPass::FindClipSlot(const std::uint64_t key) const
    {
        return mImpl->FindClipSlot(key);
    }

    std::uint32_t GpuAnimPass::JointsPerClipSlot() const
    {
        return mImpl->JointsPerClipSlot();
    }

    std::uint32_t GpuAnimPass::ResidentClipCount() const
    {
        return mImpl->ResidentClipCount();
    }

    void GpuAnimPass::CaptureDebugSnapshot(GpuAnimDebugSnapshot& out) const
    {
        mImpl->CaptureDebugSnapshot(out);
    }

    void GpuAnimPass::UploadBoneMask(const std::span<const float> weights)
    {
        mImpl->UploadBoneMask(weights);
    }

    void GpuAnimPass::UploadRestJoints(
        const std::span<const GpuFloatJoint> joints)
    {
        mImpl->UploadRestJoints(joints);
    }

    void GpuAnimPass::UploadRestJoints(
        const std::span<const GpuQuantJoint> joints)
    {
        mImpl->UploadRestJoints(joints);
    }

    void GpuAnimPass::UploadInstances(
        const std::span<const GpuAnimInstance> instances)
    {
        mImpl->UploadInstances(instances);
    }

    std::uint32_t GpuAnimPass::GetInstanceCount() const
    {
        return mImpl->GetInstanceCount();
    }

    void GpuAnimPass::Dispatch(const skr::Arc<CommandPool>& commandPool,
                               const std::uint32_t          frameIndex) const
    {
        mImpl->Dispatch(commandPool, frameIndex);
    }

    void GpuAnimPass::DispatchImmediate(
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        mImpl->DispatchImmediate(commandPool, frameIndex);
    }

    bool GpuAnimPass::ReadbackBones(const skr::Arc<CommandPool>& commandPool,
                                    const std::uint32_t          frameIndex,
                                    const std::uint32_t          boneOffset,
                                    const std::uint32_t          count,
                                    std::span<glm::mat4>
                                        out) const
    {
        return mImpl->ReadbackBones(
            commandPool, frameIndex, boneOffset, count, out);
    }

} // namespace FREYA_NAMESPACE
