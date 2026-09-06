#include "Freya/Core/IndirectDrawSystem.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cmath>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kInitialInstanceCapacity = 256;

        struct UploadSortKey
        {
            std::uint32_t entityId    = 0;
            std::uint32_t uploadIndex = 0;
        };

        bool UploadSortKeyLess(const UploadSortKey& a, const UploadSortKey& b)
        {
            if (a.entityId != b.entityId)
                return a.entityId < b.entityId;
            return a.uploadIndex < b.uploadIndex;
        }

        std::uint64_t MixTopology(std::uint64_t h, std::uint32_t v)
        {
            h ^= static_cast<std::uint64_t>(v) + 0x9e3779b97f4a7c15ull +
                 (h << 6) + (h >> 2);
            return h;
        }

        vk::BufferMemoryBarrier BufferBarrier(
            vk::Buffer buffer, vk::AccessFlags src, vk::AccessFlags dst)
        {
            return vk::BufferMemoryBarrier()
                .setSrcAccessMask(src)
                .setDstAccessMask(dst)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(buffer)
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE);
        }
    } // namespace

    IndirectDrawSystem::IndirectDrawSystem(
        const skr::Arc<Device>&                      device,
        const skr::Arc<CommandPool>&                 commandPool,
        const skr::Arc<MeshPool>&                    meshPool,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const skr::Arc<MaterialPool>&                materialPool,
        const std::uint32_t                          frameCount,
        const vk::Pipeline                           cullPipeline,
        const vk::PipelineLayout                     cullPipelineLayout,
        const vk::DescriptorSetLayout                cullSetLayout,
        const vk::DescriptorPool                     cullDescriptorPool,
        std::vector<vk::DescriptorSet>
                                      cullDescriptorSets,
        const vk::Pipeline            bvhCullPipeline,
        const vk::PipelineLayout      bvhCullPipelineLayout,
        const vk::DescriptorSetLayout bvhCullSetLayout,
        const vk::DescriptorPool      bvhCullDescriptorPool,
        std::vector<vk::DescriptorSet>
                                      bvhCullDescriptorSets,
        const vk::Pipeline            preparePipeline,
        const vk::PipelineLayout      preparePipelineLayout,
        const vk::DescriptorSetLayout prepareSetLayout,
        const vk::DescriptorPool      prepareDescriptorPool,
        std::vector<vk::DescriptorSet>
            prepareDescriptorSets,
        skr::Arc<HiZPyramid>
            hiz,
        skr::Arc<Image>
                            hizFallbackImage,
        const bool          enableHierarchicalCulling,
        const std::uint32_t hierarchicalCullMinInstances) :
        mDevice(device), mCommandPool(commandPool), mMeshPool(meshPool),
        mMaterials(materials), mMaterialPool(materialPool),
        mFrameCount(std::max(1u, frameCount)), mCullPipeline(cullPipeline),
        mCullPipelineLayout(cullPipelineLayout), mCullSetLayout(cullSetLayout),
        mCullDescriptorPool(cullDescriptorPool),
        mCullDescriptorSets(std::move(cullDescriptorSets)),
        mBvhCullPipeline(bvhCullPipeline),
        mBvhCullPipelineLayout(bvhCullPipelineLayout),
        mBvhCullSetLayout(bvhCullSetLayout),
        mBvhCullDescriptorPool(bvhCullDescriptorPool),
        mBvhCullDescriptorSets(std::move(bvhCullDescriptorSets)),
        mPreparePipeline(preparePipeline),
        mPreparePipelineLayout(preparePipelineLayout),
        mPrepareSetLayout(prepareSetLayout),
        mPrepareDescriptorPool(prepareDescriptorPool),
        mPrepareDescriptorSets(std::move(prepareDescriptorSets)),
        mHiZ(std::move(hiz)), mHizFallbackImage(std::move(hizFallbackImage)),
        mEnableHierarchicalCulling(enableHierarchicalCulling),
        mHierarchicalCullMinInstances(hierarchicalCullMinInstances == 0
                                          ? kHierarchicalCullMinInstances
                                          : hierarchicalCullMinInstances)
    {
        mFrames.resize(mFrameCount);
        mFrameCullDescVersion.assign(mFrameCount, 0);
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
            ensureCapacityForFrame(f, kInitialInstanceCapacity);
        SyncMeshInfo();
    }

    IndirectDrawSystem::~IndirectDrawSystem()
    {
        mDevice->Get().waitIdle();
        auto& vkDevice = mDevice->Get();
        vkDevice.destroyPipeline(mCullPipeline);
        vkDevice.destroyPipelineLayout(mCullPipelineLayout);
        vkDevice.destroyDescriptorPool(mCullDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mCullSetLayout);
        if (mBvhCullPipeline)
            vkDevice.destroyPipeline(mBvhCullPipeline);
        if (mBvhCullPipelineLayout)
            vkDevice.destroyPipelineLayout(mBvhCullPipelineLayout);
        if (mBvhCullDescriptorPool)
            vkDevice.destroyDescriptorPool(mBvhCullDescriptorPool);
        if (mBvhCullSetLayout)
            vkDevice.destroyDescriptorSetLayout(mBvhCullSetLayout);
        if (mPreparePipeline)
            vkDevice.destroyPipeline(mPreparePipeline);
        if (mPreparePipelineLayout)
            vkDevice.destroyPipelineLayout(mPreparePipelineLayout);
        if (mPrepareDescriptorPool)
            vkDevice.destroyDescriptorPool(mPrepareDescriptorPool);
        if (mPrepareSetLayout)
            vkDevice.destroyDescriptorSetLayout(mPrepareSetLayout);
        mHiZ.reset();
        mHizFallbackImage.reset();
    }

    IndirectDrawSystem::FrameResources& IndirectDrawSystem::currentFrame()
    {
        return mFrames[mFrameIndex % mFrameCount];
    }

    IndirectDrawSystem::DrawListResources& IndirectDrawSystem::drawListFor(
        FrameResources& frame, const std::uint32_t techniqueFilter)
    {
        if (techniqueFilter == kTechniqueFilterAll)
            return frame.main;
        return frame
            .techniques[std::min(techniqueFilter, kMaxMaterialTechniques - 1u)];
    }

    vk::DescriptorSet IndirectDrawSystem::cullSetFor(
        const std::uint32_t techniqueFilter) const
    {
        const auto base = mFrameIndex * kCullSetsPerFrame;
        if (techniqueFilter == kTechniqueFilterAll)
            return mCullDescriptorSets[base];
        return mCullDescriptorSets[base + 1u +
                                   std::min(techniqueFilter,
                                            kMaxMaterialTechniques - 1u)];
    }

    bool IndirectDrawSystem::shouldUseHierarchicalCull() const
    {
        return mEnableHierarchicalCulling && mBvhCullPipeline &&
               !mSceneBvh.Empty() &&
               mInstanceCount >= mHierarchicalCullMinInstances;
    }

    void IndirectDrawSystem::SetCullView(const glm::vec3&   cameraPos,
                                         const vk::Extent2D screenSize)
    {
        mCameraPos  = cameraPos;
        mScreenSize = screenSize;
    }

    void IndirectDrawSystem::ResizeHiZ(const vk::Extent2D extent)
    {
        if (!mHiZ)
            return;
        mHiZ->Resize(extent.width, extent.height);
        mHiZ->Invalidate();
        bumpCullDescVersion();
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
        {
            updateCullDescriptors(f);
            updateBvhDescriptors(f);
            mFrameCullDescVersion[f] = mCullDescVersion;
        }
        mCullDescRefreshedThisFrame = false;
    }

    void IndirectDrawSystem::BuildHiZ(const skr::Arc<Image>& depthImage,
                                      const bool             reverseZ)
    {
        if (!mHiZ || !depthImage)
            return;
        const bool wasReady = mHiZ->IsReady();
        if (!mHiZ->IsValid())
            mHiZ->Resize(std::max(1u, mScreenSize.width),
                         std::max(1u, mScreenSize.height));
        mHiZ->Build(mCommandPool, depthImage, reverseZ, mFrameIndex);
        if (!wasReady && mHiZ->IsReady())
            bumpCullDescVersion();
    }

    void IndirectDrawSystem::bumpCullDescVersion()
    {
        ++mCullDescVersion;
        if (mCullDescVersion == 0)
            mCullDescVersion = 1;
    }

    void IndirectDrawSystem::refreshCullDescriptorsIfNeeded()
    {
        if (mCullDescRefreshedThisFrame)
            return;
        if (mFrameIndex >= mFrameCullDescVersion.size())
            return;

        if (mFrameCullDescVersion[mFrameIndex] != mCullDescVersion)
        {
            updateCullDescriptors(mFrameIndex);
            updateBvhDescriptors(mFrameIndex);
            mFrameCullDescVersion[mFrameIndex] = mCullDescVersion;
        }
        mCullDescRefreshedThisFrame = true;
    }

    void IndirectDrawSystem::ensureCapacity(const std::uint32_t instanceCount)
    {
        ensureCapacityForFrame(mFrameIndex, instanceCount);
        mSceneInstances.reserve(instanceCount);
        mInstanceTransforms.reserve(instanceCount);
    }

    void IndirectDrawSystem::ensureBvhCapacity(
        const std::uint32_t frameIndex, const std::uint32_t nodeCount,
        const std::uint32_t instanceCount)
    {
        if (frameIndex >= mFrames.size())
            return;

        auto&      frame = mFrames[frameIndex];
        const auto nodesNeeded =
            std::max(nodeCount, std::max(frame.bvhNodeCapacity, 1u));
        // Widest frontier ≤ instanceCount (all leaves); keep headroom for
        // the 2-slot enqueue per internal node.
        const auto queueNeeded = std::max(
            { nodeCount * 2u, instanceCount, frame.bvhQueueCapacity, 1u });
        const auto candNeeded =
            std::max(instanceCount, std::max(frame.candidateCapacity, 1u));

        bool rebuilt = false;
        if (!frame.bvhNodes || nodesNeeded > frame.bvhNodeCapacity)
        {
            frame.bvhNodes = BufferBuilder(mDevice)
                                 .SetUsage(BufferUsage::Storage)
                                 .SetSize(sizeof(BvhNode) * nodesNeeded)
                                 .Build();
            frame.bvhLeafInstances =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t) * candNeeded)
                    .Build();
            frame.bvhNodeCapacity = nodesNeeded;
            rebuilt               = true;
        }
        if (!frame.bvhQueueA || queueNeeded > frame.bvhQueueCapacity)
        {
            frame.bvhQueueA =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t) * queueNeeded)
                    .Build();
            frame.bvhQueueB =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t) * queueNeeded)
                    .Build();
            frame.bvhQueueCapacity = queueNeeded;
            rebuilt                = true;
        }
        if (!frame.candidateInstances || candNeeded > frame.candidateCapacity)
        {
            frame.candidateInstances =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t) * candNeeded)
                    .Build();
            frame.candidateCapacity = candNeeded;
            rebuilt                 = true;
        }
        if (!frame.bvhCounterA)
        {
            frame.bvhCounterA =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t))
                    .Build();
            frame.bvhCounterB =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t))
                    .Build();
            frame.bvhDispatchArgsA =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Indirect)
                    .SetSize(sizeof(vk::DispatchIndirectCommand))
                    .Build();
            frame.bvhDispatchArgsB =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Indirect)
                    .SetSize(sizeof(vk::DispatchIndirectCommand))
                    .Build();
            frame.candidateCount =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(std::uint32_t))
                    .Build();
            frame.cullDispatchArgs =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Indirect)
                    .SetSize(sizeof(vk::DispatchIndirectCommand))
                    .Build();
            rebuilt = true;
        }

        if (rebuilt)
        {
            bumpCullDescVersion();
            updateCullDescriptors(frameIndex);
            updateBvhDescriptors(frameIndex);
            if (frameIndex < mFrameCullDescVersion.size())
                mFrameCullDescVersion[frameIndex] = mCullDescVersion;
        }
    }

    void IndirectDrawSystem::ensureCapacityForFrame(
        const std::uint32_t frameIndex, const std::uint32_t instanceCount)
    {
        if (frameIndex >= mFrames.size())
            return;

        auto& frame      = mFrames[frameIndex];
        auto  listsReady = [&](const DrawListResources& list) {
            return list.compactTransforms && list.indirect && list.drawCount;
        };
        if (instanceCount <= frame.capacity && frame.sceneInstances &&
            frame.sourceTransforms && listsReady(frame.main))
        {
            bool techReady = true;
            for (const auto& tech : frame.techniques)
            {
                if (!listsReady(tech))
                {
                    techReady = false;
                    break;
                }
            }
            if (techReady)
            {
                if (mEnableHierarchicalCulling)
                    ensureBvhCapacity(frameIndex, frame.bvhNodeCapacity,
                                      instanceCount);
                return;
            }
        }

        const auto capacity =
            std::max(instanceCount,
                     std::max(frame.capacity * 2, kInitialInstanceCapacity));

        auto makeDrawList = [&]() {
            DrawListResources list;
            list.compactTransforms =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Instance)
                    .SetSize(sizeof(InstanceTransform) * capacity)
                    .Build();
            list.indirect =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Indirect)
                    .SetSize(sizeof(vk::DrawIndexedIndirectCommand) * capacity)
                    .Build();
            list.drawCount = BufferBuilder(mDevice)
                                 .SetUsage(BufferUsage::Indirect)
                                 .SetSize(sizeof(std::uint32_t))
                                 .Build();
            return list;
        };

        frame.sceneInstances =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(sizeof(SceneInstance) * capacity)
                .Build();
        frame.sourceTransforms =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(sizeof(InstanceTransform) * capacity)
                .Build();
        frame.main = makeDrawList();
        for (auto& tech : frame.techniques)
            tech = makeDrawList();
        frame.capacity = capacity;

        bumpCullDescVersion();
        updateCullDescriptors(frameIndex);
        if (mEnableHierarchicalCulling)
            ensureBvhCapacity(frameIndex, std::max(capacity * 2, 1u), capacity);
        else
            updateBvhDescriptors(frameIndex);
        if (frameIndex < mFrameCullDescVersion.size())
            mFrameCullDescVersion[frameIndex] = mCullDescVersion;
    }

    void IndirectDrawSystem::updateCullDescriptors(
        const std::uint32_t frameIndex)
    {
        if (!mMeshInfoBuffer || !mMeshLodBuffer ||
            frameIndex >= mFrames.size() ||
            mCullDescriptorSets.size() < (frameIndex + 1u) * kCullSetsPerFrame)
        {
            return;
        }

        auto& frame = mFrames[frameIndex];
        if (!frame.sceneInstances || !frame.sourceTransforms ||
            !frame.main.compactTransforms || !frame.main.indirect ||
            !frame.main.drawCount)
        {
            return;
        }

        const auto cap = std::max(frame.capacity, 1u);
        const auto meshInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mMeshInfoBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(MeshInfo) * std::max(mMeshInfoCapacity, 1u));
        const auto sceneInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.sceneInstances->Get())
                .setOffset(0)
                .setRange(sizeof(SceneInstance) * cap);
        const auto sourceInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.sourceTransforms->Get())
                .setOffset(0)
                .setRange(sizeof(InstanceTransform) * cap);
        const auto lodInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mMeshLodBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(MeshLodInfo) * std::max(mMeshLodCapacity, 1u));

        const bool useHiZ = mHiZ && mHiZ->IsReady() && mHiZ->GetSampledView();
        const auto hizView =
            useHiZ ? mHiZ->GetSampledView() : mHizFallbackImage->GetImageView();
        const auto hizSampler = mHiZ ? mHiZ->GetSampler() : vk::Sampler {};
        const auto hizInfo =
            vk::DescriptorImageInfo()
                .setSampler(hizSampler)
                .setImageView(hizView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        // Candidate buffers may not exist yet when hierarchical is off.
        skr::Arc<Buffer> candidateBuf =
            frame.candidateInstances ? frame.candidateInstances
                                     : frame.sceneInstances;
        skr::Arc<Buffer> candidateCountBuf =
            frame.candidateCount ? frame.candidateCount : frame.main.drawCount;
        const auto candCap =
            frame.candidateInstances ? std::max(frame.candidateCapacity, 1u)
                                     : cap;
        const auto candidateInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(candidateBuf->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t) * candCap);
        const auto candidateCountInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(candidateCountBuf->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t));

        auto writeSet = [&](vk::DescriptorSet set, DrawListResources& list) {
            if (!list.indirect || !list.compactTransforms || !list.drawCount)
                return;
            const auto indirectInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(list.indirect->Get())
                    .setOffset(0)
                    .setRange(sizeof(vk::DrawIndexedIndirectCommand) * cap);
            const auto compactInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(list.compactTransforms->Get())
                    .setOffset(0)
                    .setRange(sizeof(InstanceTransform) * cap);
            const auto countInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(list.drawCount->Get())
                    .setOffset(0)
                    .setRange(sizeof(std::uint32_t));

            const auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(meshInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(sceneInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(2)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(indirectInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(3)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(sourceInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(4)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(compactInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(5)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(lodInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(6)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(countInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(7)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(hizInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(8)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(candidateInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(9)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(candidateCountInfo),
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        };

        const auto base = frameIndex * kCullSetsPerFrame;
        writeSet(mCullDescriptorSets[base], frame.main);
        for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
            writeSet(mCullDescriptorSets[base + 1u + t], frame.techniques[t]);
    }

    void IndirectDrawSystem::updateBvhDescriptors(
        const std::uint32_t frameIndex)
    {
        if (!mEnableHierarchicalCulling || !mBvhCullPipeline ||
            frameIndex >= mFrames.size() ||
            mBvhCullDescriptorSets.size() <
                (frameIndex + 1u) * kBvhSetsPerFrame ||
            mPrepareDescriptorSets.size() <
                (frameIndex + 1u) * kPrepareSetsPerFrame)
        {
            return;
        }

        auto& frame = mFrames[frameIndex];
        if (!frame.bvhNodes || !frame.bvhLeafInstances || !frame.bvhQueueA ||
            !frame.bvhQueueB || !frame.bvhCounterA || !frame.bvhCounterB ||
            !frame.candidateInstances || !frame.candidateCount ||
            !frame.bvhDispatchArgsA || !frame.bvhDispatchArgsB ||
            !frame.cullDispatchArgs)
        {
            return;
        }

        const auto nodeCap  = std::max(frame.bvhNodeCapacity, 1u);
        const auto queueCap = std::max(frame.bvhQueueCapacity, 1u);
        const auto candCap  = std::max(frame.candidateCapacity, 1u);

        const auto nodesInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhNodes->Get())
                .setOffset(0)
                .setRange(sizeof(BvhNode) * nodeCap);
        const auto leafInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhLeafInstances->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t) * candCap);
        const auto queueAInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhQueueA->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t) * queueCap);
        const auto queueBInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhQueueB->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t) * queueCap);
        const auto counterAInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhCounterA->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t));
        const auto counterBInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhCounterB->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t));
        const auto candInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.candidateInstances->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t) * candCap);
        const auto candCountInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.candidateCount->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t));
        const auto dispatchAInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhDispatchArgsA->Get())
                .setOffset(0)
                .setRange(sizeof(vk::DispatchIndirectCommand));
        const auto dispatchBInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.bvhDispatchArgsB->Get())
                .setOffset(0)
                .setRange(sizeof(vk::DispatchIndirectCommand));
        const auto cullDispatchInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.cullDispatchArgs->Get())
                .setOffset(0)
                .setRange(sizeof(vk::DispatchIndirectCommand));

        const bool useHiZ = mHiZ && mHiZ->IsReady() && mHiZ->GetSampledView();
        const auto hizView =
            useHiZ ? mHiZ->GetSampledView() : mHizFallbackImage->GetImageView();
        const auto hizSampler = mHiZ ? mHiZ->GetSampler() : vk::Sampler {};
        const auto hizInfo =
            vk::DescriptorImageInfo()
                .setSampler(hizSampler)
                .setImageView(hizView)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        auto writeBvhSet = [&](vk::DescriptorSet set, bool aToB) {
            const auto& queueIn    = aToB ? queueAInfo : queueBInfo;
            const auto& queueOut   = aToB ? queueBInfo : queueAInfo;
            const auto& counterOut = aToB ? counterBInfo : counterAInfo;
            const auto& counterIn  = aToB ? counterAInfo : counterBInfo;
            const auto  writes     = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(nodesInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(leafInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(2)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(queueIn),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(3)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(queueOut),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(4)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(counterOut),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(5)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(candInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(6)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(candCountInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(7)
                    .setDescriptorType(
                        vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setImageInfo(hizInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(8)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(counterIn),
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        };

        auto writePrepareSet = [&](vk::DescriptorSet               set,
                                   const vk::DescriptorBufferInfo& counter,
                                   const vk::DescriptorBufferInfo& dispatch,
                                   const vk::DescriptorBufferInfo& clear) {
            const auto writes = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(counter),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(1)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(dispatch),
                vk::WriteDescriptorSet()
                    .setDstSet(set)
                    .setDstBinding(2)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(clear),
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        };

        const auto bvhBase     = frameIndex * kBvhSetsPerFrame;
        const auto prepareBase = frameIndex * kPrepareSetsPerFrame;
        writeBvhSet(mBvhCullDescriptorSets[bvhBase], true);      // A → B
        writeBvhSet(mBvhCullDescriptorSets[bvhBase + 1], false); // B → A

        // After A→B cull: counterB → dispatchArgsB, clear counterA
        writePrepareSet(mPrepareDescriptorSets[prepareBase], counterBInfo,
                        dispatchBInfo, counterAInfo);
        // After B→A cull: counterA → dispatchArgsA, clear counterB
        writePrepareSet(mPrepareDescriptorSets[prepareBase + 1], counterAInfo,
                        dispatchAInfo, counterBInfo);
        // After traversal: candidateCount → cullDispatchArgs, clear unused
        // (clear counterA again — harmless)
        writePrepareSet(mPrepareDescriptorSets[prepareBase + 2], candCountInfo,
                        cullDispatchInfo, counterAInfo);
    }

    void IndirectDrawSystem::SyncMeshInfo()
    {
        mMeshPool->FillMeshInfos(mMeshInfos);
        mMeshPool->FillMeshLods(mMeshLods);

        const auto meshCount =
            std::max(static_cast<std::uint32_t>(mMeshInfos.size()), 1u);
        const auto lodCount =
            std::max(static_cast<std::uint32_t>(mMeshLods.size()), 1u);

        bool rebind = false;
        if (meshCount > mMeshInfoCapacity || !mMeshInfoBuffer)
        {
            mDevice->Get().waitIdle();
            mMeshInfoBuffer =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(MeshInfo) * meshCount)
                    .Build();
            mMeshInfoCapacity = meshCount;
            rebind            = true;
        }
        if (lodCount > mMeshLodCapacity || !mMeshLodBuffer)
        {
            mDevice->Get().waitIdle();
            mMeshLodBuffer   = BufferBuilder(mDevice)
                                   .SetUsage(BufferUsage::Storage)
                                   .SetSize(sizeof(MeshLodInfo) * lodCount)
                                   .Build();
            mMeshLodCapacity = lodCount;
            rebind           = true;
        }

        if (rebind)
        {
            bumpCullDescVersion();
            for (std::uint32_t f = 0; f < mFrameCount; ++f)
            {
                updateCullDescriptors(f);
                updateBvhDescriptors(f);
                if (f < mFrameCullDescVersion.size())
                    mFrameCullDescVersion[f] = mCullDescVersion;
            }
        }

        if (!mMeshInfos.empty())
        {
            mMeshInfoBuffer->Copy(mMeshInfos.data(),
                                  sizeof(MeshInfo) * mMeshInfos.size());
        }
        if (!mMeshLods.empty())
        {
            mMeshLodBuffer->Copy(mMeshLods.data(),
                                 sizeof(MeshLodInfo) * mMeshLods.size());
        }
        mMeshInfoDirty = false;
    }

    void IndirectDrawSystem::uploadFrameBuffers()
    {
        auto& frame = currentFrame();
        if (mInstanceCount == 0 || !frame.sceneInstances)
            return;

        frame.sceneInstances->Copy(mSceneInstances.data(),
                                   sizeof(SceneInstance) * mInstanceCount);
        frame.sourceTransforms->Copy(
            mInstanceTransforms.data(),
            sizeof(InstanceTransform) * mInstanceCount);
        zeroDrawCount(kTechniqueFilterAll);
    }

    void IndirectDrawSystem::uploadBvhBuffers()
    {
        if (!mEnableHierarchicalCulling || mSceneBvh.Empty())
            return;

        auto& frame = currentFrame();
        ensureBvhCapacity(mFrameIndex, mSceneBvh.NodeCount(), mInstanceCount);
        if (!frame.bvhNodes || !frame.bvhLeafInstances)
            return;

        frame.bvhNodes->Copy(mSceneBvh.Nodes().data(),
                             sizeof(BvhNode) * mSceneBvh.NodeCount());
        if (!mSceneBvh.LeafInstances().empty())
        {
            frame.bvhLeafInstances->Copy(
                mSceneBvh.LeafInstances().data(),
                sizeof(std::uint32_t) * mSceneBvh.LeafInstanceCount());
        }
        mBvhGpuDirty = false;
    }

    void IndirectDrawSystem::zeroDrawCount(const std::uint32_t techniqueFilter)
    {
        auto& list = drawListFor(currentFrame(), techniqueFilter);
        if (!list.drawCount)
            return;
        mCommandPool->GetCommandBuffer().fillBuffer(
            list.drawCount->Get(), 0, sizeof(std::uint32_t), 0);
    }

    void IndirectDrawSystem::rebuildOrRefitBvh()
    {
        if (!mEnableHierarchicalCulling || mInstanceCount == 0)
        {
            mSceneBvh.Clear();
            return;
        }

        mInstanceAabbs.resize(mInstanceCount);
        std::uint64_t topo = MixTopology(0, mInstanceCount);
        for (std::uint32_t i = 0; i < mInstanceCount; ++i)
        {
            const auto& inst = mSceneInstances[i];
            topo             = MixTopology(topo, inst.meshId);
            topo             = MixTopology(topo, inst.entityId);

            glm::vec3 localMin(0.0f);
            glm::vec3 localMax(0.0f);
            if (inst.meshId < mMeshInfos.size())
            {
                localMin = glm::vec3(mMeshInfos[inst.meshId].aabbMin);
                localMax = glm::vec3(mMeshInfos[inst.meshId].aabbMax);
            }
            mInstanceAabbs[i] = TransformAabb(inst.model, localMin, localMax);
        }

        if (topo == mSceneBvh.TopologyHash() && !mSceneBvh.Empty())
        {
            mSceneBvh.Refit(mInstanceAabbs);
        }
        else
        {
            mSceneBvh.Build(mInstanceAabbs, kBvhDefaultLeafSize);
        }
        mSceneBvh.SetTopologyHash(topo);
        mBvhGpuDirty = true;
    }

    void IndirectDrawSystem::UploadSceneInstances(
        const std::span<const SceneInstanceUpload> uploads,
        const std::uint32_t                        frameIndex)
    {
        mFrameIndex = frameIndex % mFrameCount;
        ++mFrameSerial;
        mCullDescRefreshedThisFrame = false;

        if (uploads.empty())
        {
            mInstanceCount = 0;
            mSceneInstances.clear();
            mInstanceTransforms.clear();
            mUsedTechniqueMask = 0;
            mSceneBvh.Clear();
            return;
        }

        mUsedTechniqueMask = 0;

        if (mMeshInfoDirty || mMeshPool->GetMeshCount() != mMeshInfos.size())
            SyncMeshInfo();

        ensureCapacity(static_cast<std::uint32_t>(uploads.size()));
        refreshCullDescriptorsIfNeeded();
        mCullDescRefreshedThisFrame = false;

        mPrevTransforms.swap(mInstanceTransforms);
        mPrevModelByEntity.clear();
        mPrevModelByEntity.reserve(mPrevTransforms.size());
        for (const auto& prev : mPrevTransforms)
            mPrevModelByEntity.insert(prev.entityId, prev.model);

        std::vector<UploadSortKey> sortKeys(uploads.size());
        for (std::uint32_t i = 0; i < uploads.size(); ++i)
        {
            sortKeys[i] = UploadSortKey {
                .entityId    = uploads[i].entityId,
                .uploadIndex = i,
            };
        }

        bool alreadySorted = true;
        for (std::uint32_t i = 1; i < sortKeys.size(); ++i)
        {
            if (UploadSortKeyLess(sortKeys[i], sortKeys[i - 1]))
            {
                alreadySorted = false;
                break;
            }
        }
        if (!alreadySorted)
        {
            std::stable_sort(sortKeys.begin(), sortKeys.end(),
                             UploadSortKeyLess);
        }

        mInstanceCount = static_cast<std::uint32_t>(uploads.size());
        mSceneInstances.resize(mInstanceCount);
        mInstanceTransforms.resize(mInstanceCount);

        for (std::uint32_t dst = 0; dst < mInstanceCount; ++dst)
        {
            const auto& src = uploads[sortKeys[dst].uploadIndex];
            auto flags = src.castShadows ? kSceneInstanceFlagCastShadows : 0u;
            std::uint32_t techniqueId = 0;
            if (mMaterialPool)
            {
                const auto& matInfo =
                    mMaterialPool->GetCreateInfo(src.materialId);
                techniqueId = matInfo.techniqueId;
                if (techniqueId >= kMaxMaterialTechniques)
                    techniqueId = 0;
                if (matInfo.alphaMode == AlphaMode::Blend)
                    flags |= kSceneInstanceFlagTranslucent;
            }
            if (src.boneOffset != kNoSkin)
                flags |= kSceneInstanceFlagSkinned;

            mUsedTechniqueMask |= (1u << techniqueId);

            mSceneInstances[dst] = SceneInstance {
                .model       = src.model,
                .meshId      = src.meshId,
                .materialId  = src.materialId,
                .entityId    = src.entityId,
                .flags       = flags,
                .techniqueId = techniqueId,
            };

            glm::mat4 prev = src.model;
            if (const auto* found = mPrevModelByEntity.find(src.entityId))
                prev = *found;

            mInstanceTransforms[dst] = InstanceTransform {
                .model      = src.model,
                .prevModel  = prev,
                .materialId = src.materialId,
                .entityId   = src.entityId,
                .flags      = flags,
                .boneOffset = src.boneOffset,
            };
        }

        uploadFrameBuffers();
        rebuildOrRefitBvh();
        if (mBvhGpuDirty)
            uploadBvhBuffers();
    }

    void IndirectDrawSystem::dispatchBvhTraversal(
        const glm::mat4& viewProj, const bool reverseZ, const bool hizEnabled)
    {
        auto& frame = currentFrame();
        auto& cb    = mCommandPool->GetCommandBuffer();

        // Seed must be recorded into the CB (not host Copy): DispatchCull runs
        // multiple times per frame on shared BVH working buffers; a host
        // memcpy between recorded dispatches only applies once before submit,
        // so later traversals would start from the previous pass's leftovers.
        const std::uint32_t               root = mSceneBvh.RootIndex();
        const vk::DispatchIndirectCommand seedDispatch { 1, 1, 1 };

        cb.updateBuffer(frame.bvhQueueA->Get(), 0, sizeof(root), &root);
        cb.fillBuffer(frame.bvhCounterA->Get(), 0, sizeof(std::uint32_t), 1u);
        cb.fillBuffer(frame.bvhCounterB->Get(), 0, sizeof(std::uint32_t), 0u);
        cb.fillBuffer(frame.candidateCount->Get(), 0, sizeof(std::uint32_t),
                      0u);
        cb.updateBuffer(frame.bvhDispatchArgsA->Get(), 0, sizeof(seedDispatch),
                        &seedDispatch);

        const auto transferToCompute = std::array {
            BufferBarrier(frame.bvhNodes->Get(),
                          vk::AccessFlagBits::eHostWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(frame.bvhLeafInstances->Get(),
                          vk::AccessFlagBits::eHostWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(frame.bvhQueueA->Get(),
                          vk::AccessFlagBits::eTransferWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(frame.bvhCounterA->Get(),
                          vk::AccessFlagBits::eTransferWrite,
                          vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite),
            BufferBarrier(frame.bvhCounterB->Get(),
                          vk::AccessFlagBits::eTransferWrite,
                          vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite),
            BufferBarrier(frame.candidateCount->Get(),
                          vk::AccessFlagBits::eTransferWrite,
                          vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite),
            BufferBarrier(frame.candidateInstances->Get(),
                          vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eShaderWrite),
            BufferBarrier(frame.bvhDispatchArgsA->Get(),
                          vk::AccessFlagBits::eTransferWrite,
                          vk::AccessFlagBits::eIndirectCommandRead),
        };
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost |
                               vk::PipelineStageFlagBits::eTransfer |
                               vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eComputeShader |
                               vk::PipelineStageFlagBits::eDrawIndirect,
                           vk::DependencyFlags {}, 0, nullptr,
                           static_cast<std::uint32_t>(transferToCompute.size()),
                           transferToCompute.data(), 0, nullptr);

        BvhCullPushConstants bvhPc {};
        bvhPc.viewProj      = viewProj;
        bvhPc.screenSize    = glm::vec2(static_cast<float>(mScreenSize.width),
                                        static_cast<float>(mScreenSize.height));
        bvhPc.reverseZ      = reverseZ ? 1u : 0u;
        bvhPc.hizEnabled    = hizEnabled ? 1u : 0u;
        bvhPc.maxCandidates = frame.candidateCapacity;
        bvhPc.maxQueue      = frame.bvhQueueCapacity;
        bvhPc.hizDepthBias  = 1e-4f;

        PrepareBvhDispatchPushConstants prepPc {};
        prepPc.localSizeX = 64;

        const auto bvhBase     = mFrameIndex * kBvhSetsPerFrame;
        const auto prepareBase = mFrameIndex * kPrepareSetsPerFrame;
        const auto maxDepth    = std::max(1u, mSceneBvh.MaxDepth());

        bool aToB = true;
        for (std::uint32_t level = 0; level < maxDepth; ++level)
        {
            const auto bvhSet =
                mBvhCullDescriptorSets[bvhBase + (aToB ? 0u : 1u)];
            const auto prepSet =
                mPrepareDescriptorSets[prepareBase + (aToB ? 0u : 1u)];
            auto* dispatchArgs = aToB ? frame.bvhDispatchArgsA.get()
                                      : frame.bvhDispatchArgsB.get();

            bvhPc.queueCount = frame.bvhQueueCapacity;

            cb.bindPipeline(vk::PipelineBindPoint::eCompute, mBvhCullPipeline);
            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute, mBvhCullPipelineLayout, 0, 1,
                &bvhSet, 0, nullptr);
            cb.pushConstants(mBvhCullPipelineLayout,
                             vk::ShaderStageFlagBits::eCompute, 0,
                             sizeof(BvhCullPushConstants), &bvhPc);
            cb.dispatchIndirect(dispatchArgs->Get(), 0);

            // Queues/counters for prepare; candidates stay writable so later
            // levels can emit leaves (uneven trees emit at multiple depths).
            const auto afterCull = std::array {
                BufferBarrier(
                    aToB ? frame.bvhQueueB->Get() : frame.bvhQueueA->Get(),
                    vk::AccessFlagBits::eShaderWrite,
                    vk::AccessFlagBits::eShaderRead),
                BufferBarrier(
                    aToB ? frame.bvhCounterB->Get() : frame.bvhCounterA->Get(),
                    vk::AccessFlagBits::eShaderWrite,
                    vk::AccessFlagBits::eShaderRead),
                BufferBarrier(frame.candidateInstances->Get(),
                              vk::AccessFlagBits::eShaderWrite,
                              vk::AccessFlagBits::eShaderWrite),
                BufferBarrier(frame.candidateCount->Get(),
                              vk::AccessFlagBits::eShaderWrite,
                              vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite),
            };
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eComputeShader,
                               vk::DependencyFlags {}, 0, nullptr,
                               static_cast<std::uint32_t>(afterCull.size()),
                               afterCull.data(), 0, nullptr);

            cb.bindPipeline(vk::PipelineBindPoint::eCompute, mPreparePipeline);
            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eCompute, mPreparePipelineLayout, 0, 1,
                &prepSet, 0, nullptr);
            cb.pushConstants(mPreparePipelineLayout,
                             vk::ShaderStageFlagBits::eCompute, 0,
                             sizeof(PrepareBvhDispatchPushConstants), &prepPc);
            cb.dispatch(1, 1, 1);

            auto*      nextDispatch = aToB ? frame.bvhDispatchArgsB.get()
                                           : frame.bvhDispatchArgsA.get();
            const auto afterPrep    = std::array {
                BufferBarrier(nextDispatch->Get(),
                              vk::AccessFlagBits::eShaderWrite,
                              vk::AccessFlagBits::eIndirectCommandRead),
                BufferBarrier(
                    aToB ? frame.bvhCounterA->Get() : frame.bvhCounterB->Get(),
                    vk::AccessFlagBits::eShaderWrite,
                    vk::AccessFlagBits::eShaderRead |
                        vk::AccessFlagBits::eShaderWrite),
                BufferBarrier(frame.candidateInstances->Get(),
                              vk::AccessFlagBits::eShaderWrite,
                              vk::AccessFlagBits::eShaderWrite),
                BufferBarrier(frame.candidateCount->Get(),
                              vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite,
                              vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite),
            };
            cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eComputeShader |
                                   vk::PipelineStageFlagBits::eDrawIndirect,
                               vk::DependencyFlags {}, 0, nullptr,
                               static_cast<std::uint32_t>(afterPrep.size()),
                               afterPrep.data(), 0, nullptr);

            aToB = !aToB;
        }

        // Prepare cull dispatch from candidateCount.
        const auto cullPrepSet = mPrepareDescriptorSets[prepareBase + 2];
        cb.bindPipeline(vk::PipelineBindPoint::eCompute, mPreparePipeline);
        cb.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mPreparePipelineLayout, 0, 1,
            &cullPrepSet, 0, nullptr);
        cb.pushConstants(mPreparePipelineLayout,
                         vk::ShaderStageFlagBits::eCompute, 0,
                         sizeof(PrepareBvhDispatchPushConstants), &prepPc);
        cb.dispatch(1, 1, 1);

        const auto toCullDispatch = std::array {
            BufferBarrier(frame.cullDispatchArgs->Get(),
                          vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eIndirectCommandRead),
            BufferBarrier(frame.candidateInstances->Get(),
                          vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(frame.candidateCount->Get(),
                          vk::AccessFlagBits::eShaderWrite |
                              vk::AccessFlagBits::eShaderRead,
                          vk::AccessFlagBits::eShaderRead),
        };
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eComputeShader |
                               vk::PipelineStageFlagBits::eDrawIndirect,
                           vk::DependencyFlags {}, 0, nullptr,
                           static_cast<std::uint32_t>(toCullDispatch.size()),
                           toCullDispatch.data(), 0, nullptr);
    }

    void IndirectDrawSystem::DispatchCull(
        const glm::mat4& viewProj, const CullMode mode, const bool reverseZ,
        const std::uint32_t techniqueFilter)
    {
        if (mInstanceCount == 0)
            return;

        refreshCullDescriptorsIfNeeded();

        zeroDrawCount(techniqueFilter);

        if (mode == CullMode::Camera && mHiZMotionSerial != mFrameSerial)
        {
            bool viewChanged = !mHasLastCullViewProj;
            if (!viewChanged)
            {
                for (int col = 0; col < 4 && !viewChanged; ++col)
                    for (int row = 0; row < 4; ++row)
                        viewChanged |=
                            std::abs(viewProj[col][row] -
                                     mLastCullViewProj[col][row]) > 1e-5f;
            }
            mLastCullViewProj    = viewProj;
            mHasLastCullViewProj = true;
            mHiZSafeForFrame     = !viewChanged;
            mHiZMotionSerial     = mFrameSerial;
        }

        const bool hizOn = (mHiZSafeForFrame && mode == CullMode::Camera &&
                            mHiZ && mHiZ->IsReady());

        const bool useHier = shouldUseHierarchicalCull();
        if (useHier)
            dispatchBvhTraversal(viewProj, reverseZ, hizOn);

        CullPushConstants pc {};
        pc.viewProj   = viewProj;
        pc.cameraPos  = glm::vec4(mCameraPos, 0.0f);
        pc.screenSize = glm::vec2(static_cast<float>(mScreenSize.width),
                                  static_cast<float>(mScreenSize.height));
        // Flat: exact instance count. Hierarchical: candidateCountValue SSBO
        // gates threads; instanceCount is unused when useCandidates != 0.
        pc.instanceCount   = mInstanceCount;
        pc.cullMode        = static_cast<std::uint32_t>(mode);
        pc.reverseZ        = reverseZ ? 1u : 0u;
        pc.hizEnabled      = hizOn ? 1u : 0u;
        pc.lodPixelRef     = 256.0f;
        pc.lodStep         = 2.0f;
        pc.techniqueFilter = techniqueFilter;
        pc.useCandidates   = useHier ? 1u : 0u;
        pc.maxDraws        = currentFrame().capacity;
        pc.hizDepthBias    = 1e-4f;

        auto&      frame = currentFrame();
        auto&      list  = drawListFor(frame, techniqueFilter);
        auto&      cb    = mCommandPool->GetCommandBuffer();
        const auto set   = cullSetFor(techniqueFilter);

        const auto toCompute = std::array {
            BufferBarrier(mMeshInfoBuffer->Get(),
                          vk::AccessFlagBits::eHostWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(mMeshLodBuffer->Get(),
                          vk::AccessFlagBits::eHostWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(frame.sceneInstances->Get(),
                          vk::AccessFlagBits::eHostWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(frame.sourceTransforms->Get(),
                          vk::AccessFlagBits::eHostWrite,
                          vk::AccessFlagBits::eShaderRead),
            BufferBarrier(list.indirect->Get(),
                          vk::AccessFlagBits::eIndirectCommandRead |
                              vk::AccessFlagBits::eHostWrite |
                              vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite),
            BufferBarrier(list.drawCount->Get(),
                          vk::AccessFlagBits::eIndirectCommandRead |
                              vk::AccessFlagBits::eTransferWrite |
                              vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eShaderRead |
                              vk::AccessFlagBits::eShaderWrite),
            BufferBarrier(list.compactTransforms->Get(),
                          vk::AccessFlagBits::eVertexAttributeRead |
                              vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eShaderWrite),
        };

        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eHost |
                vk::PipelineStageFlagBits::eTransfer |
                vk::PipelineStageFlagBits::eDrawIndirect |
                vk::PipelineStageFlagBits::eVertexInput |
                vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags {},
            0, nullptr, static_cast<std::uint32_t>(toCompute.size()),
            toCompute.data(), 0, nullptr);

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, mCullPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                              mCullPipelineLayout, 0, 1, &set, 0, nullptr);
        cb.pushConstants(mCullPipelineLayout, vk::ShaderStageFlagBits::eCompute,
                         0, sizeof(CullPushConstants), &pc);

        if (useHier && frame.cullDispatchArgs)
        {
            cb.dispatchIndirect(frame.cullDispatchArgs->Get(), 0);
        }
        else
        {
            const auto groups = (mInstanceCount + 63u) / 64u;
            cb.dispatch(groups, 1, 1);
        }

        const auto toDraw = std::array {
            BufferBarrier(list.indirect->Get(),
                          vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eIndirectCommandRead),
            BufferBarrier(list.drawCount->Get(),
                          vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eIndirectCommandRead),
            BufferBarrier(list.compactTransforms->Get(),
                          vk::AccessFlagBits::eShaderWrite,
                          vk::AccessFlagBits::eVertexAttributeRead),
        };

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eDrawIndirect |
                               vk::PipelineStageFlagBits::eVertexInput,
                           vk::DependencyFlags {}, 0, nullptr,
                           static_cast<std::uint32_t>(toDraw.size()),
                           toDraw.data(), 0, nullptr);
    }

    void IndirectDrawSystem::ExecuteDraws(
        const bool bindMaterials, const vk::PipelineLayout pipelineLayout,
        const std::uint32_t techniqueFilter)
    {
        if (mInstanceCount == 0)
            return;

        auto& frame = currentFrame();
        auto& list  = drawListFor(frame, techniqueFilter);
        auto& cb    = mCommandPool->GetCommandBuffer();

        list.compactTransforms->Bind(mCommandPool);
        mMeshPool->BindGeometry();

        if (bindMaterials && pipelineLayout)
        {
            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, 1,
                &mMaterials->GetBindlessSet(), 0, nullptr);
        }

        cb.drawIndexedIndirectCount(
            list.indirect->Get(), 0, list.drawCount->Get(), 0, frame.capacity,
            sizeof(vk::DrawIndexedIndirectCommand));
    }

} // namespace FREYA_NAMESPACE
