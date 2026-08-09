#include "Freya/Core/GpuAnimPass.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace FREYA_NAMESPACE
{
    GpuAnimPass::GpuAnimPass(
        const skr::Arc<Device>&              device,
        const skr::Arc<BoneMatrixResources>& boneResources,
        const vk::PipelineLayout             pipelineLayout,
        const vk::Pipeline                   pipeline,
        const vk::DescriptorSetLayout        animSetLayout,
        const vk::DescriptorPool             animPool,
        const vk::DescriptorSet              animSet,
        const skr::Arc<Buffer>&              parentsBuffer,
        const skr::Arc<Buffer>&              invBindBuffer,
        const skr::Arc<Buffer>&              clipHeaderBuffer,
        const skr::Arc<Buffer>&              jointsBuffer,
        const skr::Arc<Buffer>&              instanceBuffer,
        const skr::Arc<Buffer>&              boneMaskBuffer,
        const skr::Arc<Buffer>&              restJointsBuffer,
        const skr::Arc<Buffer>&              localScratchBuffer,
        const skr::Arc<Buffer>&              globalScratchBuffer,
        const skr::Arc<Buffer>&              readbackBuffer,
        const skr::Arc<Buffer>&              extractRingBuffer,
        const std::uint32_t                  frameCount) :
        mDevice(device), mBoneResources(boneResources),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mAnimSetLayout(animSetLayout), mAnimPool(animPool), mAnimSet(animSet),
        mParentsBuffer(parentsBuffer), mInvBindBuffer(invBindBuffer),
        mClipHeaderBuffer(clipHeaderBuffer), mJointsBuffer(jointsBuffer),
        mInstanceBuffer(instanceBuffer), mBoneMaskBuffer(boneMaskBuffer),
        mRestJointsBuffer(restJointsBuffer),
        mLocalScratchBuffer(localScratchBuffer),
        mGlobalScratchBuffer(globalScratchBuffer),
        mReadbackBuffer(readbackBuffer), mExtractRingBuffer(extractRingBuffer),
        mFrameCount(std::max(1u, frameCount))
    {
        mExtractMeta.resize(mFrameCount);
        mExtractCounts.resize(mFrameCount, 0);
        mExtractValid.resize(mFrameCount, 0);
        mExtractSourceFrame.resize(mFrameCount, 0);
    }

    GpuAnimPass::~GpuAnimPass()
    {
        if (!mDevice)
            return;
        if (mPipeline)
            mDevice->Get().destroyPipeline(mPipeline);
        if (mPipelineLayout)
            mDevice->Get().destroyPipelineLayout(mPipelineLayout);
        if (mAnimPool)
            mDevice->Get().destroyDescriptorPool(mAnimPool);
        if (mAnimSetLayout)
            mDevice->Get().destroyDescriptorSetLayout(mAnimSetLayout);
    }

    void GpuAnimPass::SetJointExtractList(
        const std::span<const GpuJointExtractRequest> reqs)
    {
        const auto n = std::min(
            static_cast<std::uint32_t>(reqs.size()), kMaxExtractJoints);
        mExtractRequests.assign(reqs.begin(), reqs.begin() + n);
    }

    bool GpuAnimPass::PollJointExtract(const std::uint32_t frameIndex,
                                       const std::span<GpuJointExtractSample>
                                                      out,
                                       std::uint32_t* outCount) const
    {
        if (outCount)
            *outCount = 0;
        if (!mExtractRingBuffer || mFrameCount == 0 || out.empty())
            return false;

        // Previous finished frame in the FiF ring (N+1 when polled at Update
        // start before this slot is overwritten by Dispatch).
        const auto prev =
            (extractSlot(frameIndex) + mFrameCount - 1u) % mFrameCount;
        if (!mExtractValid[prev] || mExtractCounts[prev] == 0)
            return false;

        const auto n = std::min(
            { mExtractCounts[prev], static_cast<std::uint32_t>(out.size()),
              static_cast<std::uint32_t>(mExtractMeta[prev].size()) });
        if (n == 0)
            return false;

        const void* mapped = mExtractRingBuffer->GetMapped();
        if (!mapped)
            return false;

        const auto* mats = reinterpret_cast<const glm::mat4*>(
            static_cast<const std::uint8_t*>(mapped) +
            static_cast<std::size_t>(prev) * extractSlotBytes());

        for (std::uint32_t i = 0; i < n; ++i)
        {
            out[i].boneOffset  = mExtractMeta[prev][i].boneOffset;
            out[i].jointIndex  = mExtractMeta[prev][i].jointIndex;
            out[i].sourceFrame = mExtractSourceFrame[prev];
            out[i]._pad        = 0;
            out[i].skinMatrix  = mats[i];
        }
        if (outCount)
            *outCount = n;
        return true;
    }

    void GpuAnimPass::UploadSkeleton(const GpuSkeletonPack& skeleton)
    {
        mJointCount = std::min(skeleton.jointCount, kMaxJoints);
        if (mJointCount == 0)
            return;

        std::vector<std::int32_t> parents(kMaxJoints, -1);
        for (std::uint32_t i = 0; i < mJointCount; ++i)
            parents[i] = skeleton.parents[i];
        mParentsBuffer->Copy(
            parents.data(),
            static_cast<std::uint32_t>(parents.size() * sizeof(std::int32_t)));

        std::vector<glm::mat4> inv(kMaxJoints, glm::mat4(1.f));
        for (std::uint32_t i = 0; i < mJointCount; ++i)
            inv[i] = skeleton.inverseBind[i];
        mInvBindBuffer->Copy(
            inv.data(),
            static_cast<std::uint32_t>(inv.size() * sizeof(glm::mat4)));
    }

    void GpuAnimPass::UploadBakes(const GpuBakePack& pack)
    {
        const auto clipCount = std::min(
            static_cast<std::uint32_t>(pack.headers.size()), kMaxClips);
        if (clipCount == 0)
            return;

        mClipHeaderBuffer->Copy(
            pack.headers.data(),
            static_cast<std::uint32_t>(clipCount * sizeof(GpuClipHeader)));

        const auto jointCount = std::min(
            static_cast<std::uint32_t>(pack.joints.size()), kMaxBakedJoints);
        mJointsBuffer->Copy(
            pack.joints.data(),
            static_cast<std::uint32_t>(jointCount * sizeof(GpuBakedJoint)));
    }

    void GpuAnimPass::UploadBoneMask(const std::span<const float> weights)
    {
        if (!mBoneMaskBuffer || weights.empty())
            return;
        const auto n = std::min(static_cast<std::uint32_t>(weights.size()),
                                kMaxMaskFloats);
        mBoneMaskBuffer->Copy(weights.data(),
                              static_cast<std::uint32_t>(n * sizeof(float)));
    }

    void GpuAnimPass::UploadRestJoints(
        const std::span<const GpuBakedJoint> joints)
    {
        if (!mRestJointsBuffer || joints.empty())
            return;
        const auto n =
            std::min(static_cast<std::uint32_t>(joints.size()), kMaxJoints);
        mRestJointsBuffer->Copy(
            joints.data(),
            static_cast<std::uint32_t>(n * sizeof(GpuBakedJoint)));
    }

    void GpuAnimPass::UploadInstances(
        const std::span<const GpuAnimInstance> instances)
    {
        mInstanceCount = std::min(static_cast<std::uint32_t>(instances.size()),
                                  kMaxInstances);
        if (mInstanceCount == 0)
            return;
        mInstanceBuffer->Copy(instances.data(),
                              static_cast<std::uint32_t>(
                                  mInstanceCount * sizeof(GpuAnimInstance)));
    }

    void GpuAnimPass::Dispatch(const skr::Arc<CommandPool>& commandPool,
                               const std::uint32_t          frameIndex) const
    {
        if (!mEnabled || mInstanceCount == 0 || !mBoneResources || !mPipeline)
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();

        if (mCopyPrevBones)
        {
            // Sparse LOD: seed this FiF slot from last frame so foxes not in
            // the active list keep a continuous pose (not a stale ring entry).
            mBoneResources->RecordCarryBonesFromPreviousFrame(commandBuffer,
                                                              frameIndex);
            mBoneResources->RecordCopyCurrentToPrev(commandBuffer, frameIndex);
        }

        recordCompute(commandBuffer, frameIndex);
        recordJointExtract(commandBuffer, frameIndex);
    }

    void GpuAnimPass::DispatchImmediate(
        const skr::Arc<CommandPool>& commandPool,
        const std::uint32_t          frameIndex) const
    {
        if (!mEnabled || mInstanceCount == 0 || !mBoneResources || !mPipeline ||
            !mDevice)
            return;

        mDevice->Get().waitIdle();

        auto cb = commandPool->CreateCommandBuffer();
        cb.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        recordCompute(cb, frameIndex);
        recordJointExtract(cb, frameIndex);
        cb.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cb);
        mDevice->GetGraphicsQueue().submit(submitInfo);
        mDevice->GetGraphicsQueue().waitIdle();
        commandPool->FreeCommandBuffer(cb);
    }

    void GpuAnimPass::recordCompute(const vk::CommandBuffer commandBuffer,
                                    const std::uint32_t     frameIndex) const
    {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, mPipeline);
        const auto boneSet = mBoneResources->GetSet(frameIndex);
        const auto sets    = std::array { boneSet, mAnimSet };
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mPipelineLayout, 0, sets, {});

        PushConstants pc {};
        pc.instanceCount    = mInstanceCount;
        pc.jointCount       = mJointCount;
        pc.lookJoint        = mLookJoint;
        pc.ikRoot           = mIkRoot;
        pc.ikMid            = mIkMid;
        pc.ikTip            = mIkTip;
        pc.rootJoint        = mRootJoint;
        pc.lookLocalForward = glm::vec4(mLookLocalForward, 0.f);
        pc.lookMaxYaw       = mLookMaxYawRad;
        pc.lookMaxPitch     = mLookMaxPitchRad;
        commandBuffer.pushConstants(
            mPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
            sizeof(PushConstants), &pc);

        const auto groups = (mInstanceCount + 63u) / 64u;
        commandBuffer.dispatch(groups, 1, 1);

        const auto bonesOff = mBoneResources->BonesByteOffset(frameIndex);
        const auto bytes    = mBoneResources->PaletteBytes();
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eVertexShader, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setBuffer(mBoneResources->GetBuffer()->Get())
                .setOffset(bonesOff)
                .setSize(bytes),
            {});
    }

    void GpuAnimPass::recordJointExtract(const vk::CommandBuffer commandBuffer,
                                         const std::uint32_t frameIndex) const
    {
        if (!mExtractRingBuffer || !mBoneResources || mExtractRequests.empty())
            return;

        const auto slot = extractSlot(frameIndex);
        const auto n =
            std::min(static_cast<std::uint32_t>(mExtractRequests.size()),
                     kMaxExtractJoints);
        if (n == 0)
            return;

        mExtractMeta[slot].assign(mExtractRequests.begin(),
                                  mExtractRequests.begin() + n);
        mExtractCounts[slot]      = n;
        mExtractSourceFrame[slot] = frameIndex;
        // Valid for Poll only after GPU finishes this FiF slot (next wrap).
        mExtractValid[slot] = 1;

        const auto bonesBase = mBoneResources->BonesByteOffset(frameIndex);
        const auto dstBase =
            static_cast<vk::DeviceSize>(slot) * extractSlotBytes();
        const auto matBytes = static_cast<vk::DeviceSize>(sizeof(glm::mat4));

        // Compute → transfer; vertex still uses bones via prior barrier.
        std::vector<vk::BufferCopy> regions;
        regions.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const auto& req = mExtractRequests[i];
            const auto  src =
                bonesBase +
                static_cast<vk::DeviceSize>(req.boneOffset + req.jointIndex) *
                    matBytes;
            regions.push_back(vk::BufferCopy()
                                  .setSrcOffset(src)
                                  .setDstOffset(dstBase + i * matBytes)
                                  .setSize(matBytes));
        }

        const auto rangeBytes = n * matBytes;
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eVertexShader,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setBuffer(mBoneResources->GetBuffer()->Get())
                .setOffset(bonesBase)
                .setSize(mBoneResources->PaletteBytes()),
            {});
        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eHost,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setBuffer(mExtractRingBuffer->Get())
                .setOffset(dstBase)
                .setSize(rangeBytes),
            {});

        commandBuffer.copyBuffer(mBoneResources->GetBuffer()->Get(),
                                 mExtractRingBuffer->Get(), regions);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eHost, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eHostRead)
                .setBuffer(mExtractRingBuffer->Get())
                .setOffset(dstBase)
                .setSize(rangeBytes),
            {});
    }

    bool GpuAnimPass::ReadbackBones(const skr::Arc<CommandPool>& commandPool,
                                    const std::uint32_t          frameIndex,
                                    const std::uint32_t          boneOffset,
                                    const std::uint32_t          count,
                                    const std::span<glm::mat4>
                                        out) const
    {
        if (!mBoneResources || !mReadbackBuffer || count == 0 ||
            out.size() < count || mJointCount == 0)
            return false;

        const auto n = std::min({ count, mJointCount, kMaxJoints,
                                  static_cast<std::uint32_t>(out.size()) });
        const auto byteCount =
            static_cast<vk::DeviceSize>(n) * sizeof(glm::mat4);
        const auto srcOff =
            mBoneResources->BonesByteOffset(frameIndex) +
            static_cast<vk::DeviceSize>(boneOffset) * sizeof(glm::mat4);

        mDevice->Get().waitIdle();

        auto cb = commandPool->CreateCommandBuffer();
        cb.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eVertexShader |
                vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setBuffer(mBoneResources->GetBuffer()->Get())
                .setOffset(srcOff)
                .setSize(byteCount),
            {});
        const auto region =
            vk::BufferCopy().setSrcOffset(srcOff).setDstOffset(0).setSize(
                byteCount);
        cb.copyBuffer(mBoneResources->GetBuffer()->Get(),
                      mReadbackBuffer->Get(), region);
        cb.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cb);
        mDevice->GetGraphicsQueue().submit(submitInfo);
        mDevice->GetGraphicsQueue().waitIdle();
        commandPool->FreeCommandBuffer(cb);

        const void* mapped = mReadbackBuffer->GetMapped();
        if (!mapped)
            return false;
        std::memcpy(out.data(), mapped, static_cast<std::size_t>(byteCount));
        return true;
    }

} // namespace FREYA_NAMESPACE
