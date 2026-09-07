#pragma once

#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"

#include <cstdint>
#include <span>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief GPU skinning / crowd animation subsystem (extracted from
     * Renderer).
     *
     * Obtain via RendererAdvanced::GpuAnimation().
     */
    class GpuAnimationSystem
    {
      public:
        explicit GpuAnimationSystem(void* rendererImpl = nullptr) :
            mImpl(rendererImpl)
        {
        }

        void               SetEnabled(bool enabled);
        [[nodiscard]] bool IsEnabled() const;

        void RebuildPass();

        void SetCopyPrevBones(bool enabled);
        void UploadInstances(std::span<const GpuAnimInstance> instances);
        void CaptureDebugSnapshot(GpuAnimDebugSnapshot& out) const;

        [[nodiscard]] std::uint32_t FindClipSlot(std::uint64_t key) const;
        [[nodiscard]] std::uint32_t EnsureClipResident(std::uint64_t    key,
                                                       const BakedClip& clip);
        [[nodiscard]] std::uint32_t GetResidentClipCount() const;
        [[nodiscard]] std::uint32_t GetJointsPerClipSlot() const;

        void UploadSkeleton(const GpuSkeletonPack& skeleton);
        void ResetClipCache();
        bool UploadClipSlot(std::uint32_t slot, std::uint64_t key,
                            const BakedClip& clip);
        void PinClipSlot(std::uint32_t slot, bool pinned);
        void UploadBoneMask(std::span<const float> weights);
        void UploadRestJoints(std::span<const GpuFloatJoint> joints);
        void UploadRestJoints(std::span<const GpuQuantJoint> joints);
        void SetRigIndices(
            std::uint32_t lookJoint, std::uint32_t ikRoot, std::uint32_t ikMid,
            std::uint32_t ikTip, std::uint32_t rootJoint,
            glm::vec3 lookLocalForward = { 0.f, 0.f, 1.f },
            float lookMaxYawRad = 1.2f, float lookMaxPitchRad = 0.8f);

        bool ReadbackBones(std::uint32_t frameIndex, std::uint32_t boneOffset,
                           std::span<glm::mat4> out);

        bool DispatchImmediate(std::span<const GpuAnimInstance> instances,
                               std::uint32_t                    frameIndex);

        void SetJointExtract(std::span<const GpuJointExtractRequest> requests);

        bool PollJointExtract(std::span<GpuJointExtractSample> out,
                              std::uint32_t* outCount = nullptr);

        bool PollTiming(GpuAnimTimingSample& out);

      private:
        void* mImpl = nullptr;
    };

} // namespace FREYA_NAMESPACE
