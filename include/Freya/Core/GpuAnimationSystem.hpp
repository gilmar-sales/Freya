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
     *
     * Parallel packing (ECS EachAsync / workers): prefer pre-resident + pin
     * at load (clips and skeletons), then `FindClipSlot` /
     * `FindSkeletonSlot` + `UploadGpuAnimInstanceUploads`. Workers may also
     * call `EnsureClipResident` / `EnsureSkeletonResident` (SpinLock;
     * free-slot fills only while instance staging is open — no LRU evict).
     * Do not call Evict / Reset / UploadClipSlot / UploadSkeleton /
     * UploadBakes during staging. Multi-rig mid-tier: up to
     * `kMaxSkeletons` atlased slabs; set `GpuAnimInstance::skeletonSlot`.
     */

    class GpuAnimationSystem

    {

      public:
        explicit GpuAnimationSystem(void* rendererImpl = nullptr) :

            mImpl(rendererImpl)

        {
        }

        void SetEnabled(bool enabled);

        [[nodiscard]] bool IsEnabled() const;

        void RebuildPass();

        void SetCopyPrevBones(bool enabled);

        /**
         * @brief Per-frame cumulative GPU anim instance upload (thread-safe
         * Upload).
         *
         * Begin → optional Reserve → Upload* from any threads → End.
         * UploadInstances wraps Begin/Upload/End for single-thread callers.
         */

        void BeginGpuAnimInstanceUploads();

        void ReserveGpuAnimInstanceUploads(std::uint32_t count);

        void UploadGpuAnimInstanceUploads(

            std::span<const GpuAnimInstance> instances);

        void EndGpuAnimInstanceUploads();

        void UploadInstances(std::span<const GpuAnimInstance> instances);

        void CaptureDebugSnapshot(GpuAnimDebugSnapshot& out) const;

        /**
         * @brief Thread-safe clip-slot lookup (internal SpinLock).
         * @return Slot index, or 0xffffffffu on miss.
         */

        [[nodiscard]] std::uint32_t FindClipSlot(std::uint64_t key) const;

        /**
         * @brief Thread-safe make-resident (internal SpinLock + host Copy).
         *
         * Touches LRU on hit. On miss: fills a free slot; if full, LRU-evicts
         * an unpinned slot only when instance staging is **closed** (main).
         * With staging open (workers), a full cache returns 0xffffffffu —
         * no concurrent evict. Prefer pre-pin at load for hot-path hits.
         */

        [[nodiscard]] std::uint32_t EnsureClipResident(std::uint64_t key,

                                                       const BakedClip& clip);

        [[nodiscard]] std::uint32_t GetResidentClipCount() const;

        [[nodiscard]] std::uint32_t GetJointsPerClipSlot() const;

        /**
         * @brief Thread-safe skeleton-slot lookup (internal SpinLock).
         * @return Slot index, or 0xffffffffu on miss.
         */
        [[nodiscard]] std::uint32_t FindSkeletonSlot(std::uint64_t key) const;

        /**
         * @brief Thread-safe make-resident skeleton atlas slab.
         *
         * Same staging rules as EnsureClipResident (free-slot only while
         * staging open). Dedupes by key. Writes parents / IBM / header.
         */
        [[nodiscard]] std::uint32_t EnsureSkeletonResident(
            std::uint64_t key, const GpuSkeletonPack& skeleton,
            std::uint32_t rootJoint = 0xffffffffu);

        [[nodiscard]] std::uint32_t GetResidentSkeletonCount() const;

        void PinSkeletonSlot(std::uint32_t slot, bool pinned);

        /**
         * @brief Main-thread only; not during instance staging.
         * Writes / pins atlas slot 0 (single-rig back-compat).
         */

        void UploadSkeleton(const GpuSkeletonPack& skeleton);

        /** @brief Main-thread only; not during instance staging. */

        void ResetClipCache();

        /** @brief Main-thread only; not during instance staging. */

        bool UploadClipSlot(std::uint32_t slot, std::uint64_t key,

                            const BakedClip& clip);

        /** @brief Thread-safe pin (between frames / at load preferred). */

        void PinClipSlot(std::uint32_t slot, bool pinned);

        void UploadBoneMask(std::span<const float> weights);

        /** @brief Rest joints for skeleton atlas slot 0 (back-compat). */
        void UploadRestJoints(std::span<const GpuFloatJoint> joints);

        void UploadRestJoints(std::span<const GpuQuantJoint> joints);

        /** @brief Rest joints for a specific skeleton atlas slot. */
        void UploadRestJoints(
            std::uint32_t skeletonSlot, std::span<const GpuFloatJoint> joints);

        void UploadRestJoints(
            std::uint32_t skeletonSlot, std::span<const GpuQuantJoint> joints);

        void SetRigIndices(

            std::uint32_t lookJoint, std::uint32_t ikRoot, std::uint32_t ikMid,

            std::uint32_t ikTip, std::uint32_t rootJoint,

            glm::vec3 lookLocalForward = { 0.f, 0.f, 1.f },

            float lookMaxYawRad = 1.2f, float lookMaxPitchRad = 0.8f);

        bool ReadbackBones(std::uint32_t frameIndex, std::uint32_t boneOffset,

                           std::span<glm::mat4> out);

        bool DispatchImmediate(std::span<const GpuAnimInstance> instances,

                               std::uint32_t frameIndex);

        void SetJointExtract(std::span<const GpuJointExtractRequest> requests);

        bool PollJointExtract(std::span<GpuJointExtractSample> out,

                              std::uint32_t* outCount = nullptr);

        bool PollTiming(GpuAnimTimingSample& out);

      private:
        void* mImpl = nullptr;
    };

} // namespace FREYA_NAMESPACE
