#pragma once

#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <span>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class CommandPool;
    class GpuAnimPassBuilder;

    class GpuAnimPass
    {
      public:
        class Impl;

        static constexpr std::uint32_t kMaxJoints           = 128;
        static constexpr std::uint32_t kMaxSkeletons        = 8;
        static constexpr std::uint32_t kMaxInstances        = 2048;
        static constexpr std::uint32_t kMaxClips            = 32;
        static constexpr std::uint32_t kMaxBakedJointsFloat = 65536;
        static constexpr std::uint32_t kMaxBakedJointsQuant = 196608;
        static constexpr std::uint32_t kMaxMaskFloats =
            kMaxJoints * kMaxSkeletons;
        static constexpr std::uint32_t kMaxAtlasJoints =
            kMaxJoints * kMaxSkeletons;
        static constexpr std::uint32_t kMaxExtractJoints        = 64;
        static constexpr std::uint32_t kTimestampQueriesPerSlot = 4;

        explicit GpuAnimPass(std::unique_ptr<Impl> impl);
        ~GpuAnimPass();

        GpuAnimPass(const GpuAnimPass&)            = delete;
        GpuAnimPass& operator=(const GpuAnimPass&) = delete;

        void                        SetEnabled(bool enabled);
        [[nodiscard]] bool          IsEnabled() const;
        [[nodiscard]] bool          UsesQuantizedJoints() const;
        [[nodiscard]] std::uint32_t MaxBakedJoints() const;

        void SetCopyPrevBones(bool enabled);

        void SetRigIndices(
            std::uint32_t lookJoint, std::uint32_t ikRoot, std::uint32_t ikMid,
            std::uint32_t ikTip, std::uint32_t rootJoint,
            glm::vec3 lookLocalForward = { 0.f, 0.f, 1.f },
            float lookMaxYawRad = 1.2f, float lookMaxPitchRad = 0.8f);

        void SetLookClamp(float maxYawRad, float maxPitchRad);

        [[nodiscard]] std::uint32_t GetDefaultLookJoint() const;
        [[nodiscard]] std::uint32_t GetDefaultIkRoot() const;
        [[nodiscard]] std::uint32_t GetDefaultIkMid() const;
        [[nodiscard]] std::uint32_t GetDefaultIkTip() const;
        [[nodiscard]] glm::vec3     GetDefaultLookLocalForward() const;
        [[nodiscard]] float         GetDefaultLookMaxYaw() const;
        [[nodiscard]] float         GetDefaultLookMaxPitch() const;

        void SetJointExtractList(std::span<const GpuJointExtractRequest> reqs);

        bool PollJointExtract(std::uint32_t frameIndex,
                              std::span<GpuJointExtractSample>
                                             out,
                              std::uint32_t* outCount = nullptr) const;

        bool PollTiming(std::uint32_t        frameIndex,
                        GpuAnimTimingSample& out) const;

        [[nodiscard]] bool HasTimestampQueries() const;

        /**
         * @brief Main-thread only. Forbidden while instance staging is open.
         * Writes / pins atlas slot 0 (single-rig back-compat).
         */
        void UploadSkeleton(const GpuSkeletonPack& skeleton);
        [[nodiscard]] std::uint32_t FindSkeletonSlot(std::uint64_t key) const;
        [[nodiscard]] std::uint32_t EnsureSkeletonResident(
            std::uint64_t key, const GpuSkeletonPack& skeleton,
            std::uint32_t rootJoint = 0xffffffffu);
        void PinSkeletonSlot(std::uint32_t slot, bool pinned);
        [[nodiscard]] std::uint32_t ResidentSkeletonCount() const;

        void UploadBakes(const GpuBakePack& pack);
        void ResetClipCache();
        bool UploadClipSlot(std::uint32_t slot, std::uint64_t key,
                            const BakedClip& clip);
        /**
         * @brief Thread-safe make-resident (SpinLock). Touches LRU on hit.
         * Miss fills a free slot; LRU evict of unpinned only when staging
         * is closed. With staging open, full cache → 0xffffffffu.
         */
        [[nodiscard]] std::uint32_t EnsureClipResident(std::uint64_t    key,
                                                       const BakedClip& clip);
        void PinClipSlot(std::uint32_t slot, bool pinned);
        void TouchClipSlot(std::uint32_t slot);
        void EvictClipSlot(std::uint32_t slot);
        /**
         * @brief Thread-safe clip-slot lookup (SpinLock).
         * @return Slot index, or 0xffffffffu on miss.
         */
        [[nodiscard]] std::uint32_t FindClipSlot(std::uint64_t key) const;
        [[nodiscard]] std::uint32_t JointsPerClipSlot() const;
        [[nodiscard]] std::uint32_t ResidentClipCount() const;

        void CaptureDebugSnapshot(GpuAnimDebugSnapshot& out) const;

        void UploadBoneMask(std::span<const float> weights);
        void UploadRestJoints(std::span<const GpuFloatJoint> joints);
        void UploadRestJoints(std::span<const GpuQuantJoint> joints);
        void UploadRestJoints(
            std::uint32_t skeletonSlot, std::span<const GpuFloatJoint> joints);
        void UploadRestJoints(
            std::uint32_t skeletonSlot, std::span<const GpuQuantJoint> joints);

        /**
         * @brief Cumulative GPU anim instance upload (thread-safe Upload).
         *
         * Begin → optional Reserve → Upload (any thread) → End.
         * UploadInstances wraps Begin/Upload/End for single-thread callers.
         * Evict / Reset / UploadClipSlot / UploadSkeleton / UploadBakes are
         * rejected while staging is open; Ensure (clip/skeleton) may fill
         * free slots.
         */
        void BeginInstanceUploads();
        void ReserveInstanceUploads(std::uint32_t count);
        void UploadInstanceUploads(std::span<const GpuAnimInstance> instances);
        void EndInstanceUploads();

        void UploadInstances(std::span<const GpuAnimInstance> instances);

        [[nodiscard]] std::uint32_t GetInstanceCount() const;

        void Dispatch(const skr::Arc<CommandPool>& commandPool,
                      std::uint32_t                frameIndex) const;

        void DispatchImmediate(const skr::Arc<CommandPool>& commandPool,
                               std::uint32_t                frameIndex) const;

        bool ReadbackBones(const skr::Arc<CommandPool>& commandPool,
                           std::uint32_t                frameIndex,
                           std::uint32_t                boneOffset,
                           std::uint32_t                count,
                           std::span<glm::mat4>
                               out) const;

      private:
        friend class GpuAnimPassBuilder;

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
