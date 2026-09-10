#include "Freya/Core/IndirectDrawSystem.hpp"

#include "Freya/Asset/MeshPoolGpu.hpp"
#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

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
    } // namespace

    IndirectDrawSystem::IndirectDrawSystem(
        const skr::Arc<Device>&                      device,
        const skr::Arc<CommandPool>&                 commandPool,
        const skr::Arc<MeshPool>&                    meshPool,
        const skr::Arc<MaterialDescriptorResources>& materials,
        const std::uint32_t                          frameCount,
        const vk::Pipeline                           cullPipeline,
        const vk::PipelineLayout                     cullPipelineLayout,
        const vk::DescriptorSetLayout                cullSetLayout,
        const vk::DescriptorPool                     cullDescriptorPool,
        std::vector<vk::DescriptorSet>
            cullDescriptorSets,
        const vk::Pipeline            expandPipeline,
        const vk::PipelineLayout      expandPipelineLayout,
        const vk::DescriptorSetLayout expandSetLayout,
        const vk::DescriptorPool      expandDescriptorPool,
        std::vector<vk::DescriptorSet>
            expandDescriptorSets,
        skr::Arc<HiZPyramid>
            hiz,
        skr::Arc<Image>
            hizFallbackImage) :
        mDevice(device), mCommandPool(commandPool), mMeshPool(meshPool),
        mMaterials(materials), mFrameCount(std::max(1u, frameCount)),
        mCullPipeline(cullPipeline), mCullPipelineLayout(cullPipelineLayout),
        mCullSetLayout(cullSetLayout), mCullDescriptorPool(cullDescriptorPool),
        mCullDescriptorSets(std::move(cullDescriptorSets)),
        mExpandPipeline(expandPipeline),
        mExpandPipelineLayout(expandPipelineLayout),
        mExpandSetLayout(expandSetLayout),
        mExpandDescriptorPool(expandDescriptorPool),
        mExpandDescriptorSets(std::move(expandDescriptorSets)),
        mHiZ(std::move(hiz)), mHizFallbackImage(std::move(hizFallbackImage))
    {
        mFrames.resize(mFrameCount);
        mFrameCullDescVersion.assign(mFrameCount, 0);
        mFrameSceneVersion.assign(mFrameCount, 0);
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
            ensureCapacityForFrame(f, kInitialInstanceCapacity);
        ensurePrevCapacity(kInitialInstanceCapacity);
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
        vkDevice.destroyPipeline(mExpandPipeline);
        vkDevice.destroyPipelineLayout(mExpandPipelineLayout);
        vkDevice.destroyDescriptorPool(mExpandDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mExpandSetLayout);
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

    void IndirectDrawSystem::SetCullView(const glm::vec3&   cameraPos,
                                         const vk::Extent2D screenSize)
    {
        mCameraPos  = cameraPos;
        mScreenSize = screenSize;
    }

    void IndirectDrawSystem::SetMeshLodCull(const float pixelRef,
                                            const float step)
    {
        mLodPixelRef = std::max(1.0f, pixelRef);
        mLodStep     = std::max(1.01f, step);
    }

    void IndirectDrawSystem::ResizeHiZ(const vk::Extent2D extent)
    {
        if (!mHiZ)
            return;
        const auto pyramid = HiZPyramid::PyramidExtent(extent);
        mHiZ->Resize(pyramid.width, pyramid.height);
        mHiZ->Invalidate();
        bumpCullDescVersion();
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
        {
            updateCullDescriptors(f);
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
        {
            const auto pyramid = HiZPyramid::PyramidExtent(mScreenSize);
            mHiZ->Resize(std::max(1u, pyramid.width),
                         std::max(1u, pyramid.height));
        }
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
            mFrameCullDescVersion[mFrameIndex] = mCullDescVersion;
        }
        mCullDescRefreshedThisFrame = true;
    }

    void IndirectDrawSystem::ensureCapacity(const std::uint32_t instanceCount)
    {
        ensureCapacityForFrame(mFrameIndex, instanceCount);
        mSceneInstances.reserve(instanceCount);
        mSceneTransforms.reserve(instanceCount);
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
            frame.sourceTransforms && frame.sourceTransformsTRS &&
            listsReady(frame.main))
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
                return;
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
        frame.sourceTransformsTRS =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(sizeof(SceneTransform) * capacity)
                .Build();
        frame.main = makeDrawList();
        for (auto& tech : frame.techniques)
            tech = makeDrawList();
        frame.capacity = capacity;

        bumpCullDescVersion();
        updateCullDescriptors(frameIndex);
        updateExpandDescriptors(frameIndex);
        if (frameIndex < mFrameCullDescVersion.size())
            mFrameCullDescVersion[frameIndex] = mCullDescVersion;
    }

    void IndirectDrawSystem::ensurePrevCapacity(
        const std::uint32_t instanceCount)
    {
        if (instanceCount <= mPrevCapacity && mPrevSourceTransforms)
            return;

        const auto capacity =
            std::max(instanceCount,
                     std::max(mPrevCapacity * 2, kInitialInstanceCapacity));
        // Prefer growing only when idle path already ran; avoid an extra
        // waitIdle here — Buffer::~Buffer syncs if an in-flight slot held the
        // old allocation. Call ReserveSceneInstances with the high-water mark.
        mPrevSourceTransforms =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Storage)
                .SetSize(sizeof(InstanceTransform) * capacity)
                .Build();
        mPrevCapacity = capacity;
        mPrevInstanceCount = 0;
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
            updateExpandDescriptors(f);
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
            };
            mDevice->Get().updateDescriptorSets(writes, nullptr);
        };

        const auto base = frameIndex * kCullSetsPerFrame;
        writeSet(mCullDescriptorSets[base], frame.main);
        for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
            writeSet(mCullDescriptorSets[base + 1u + t], frame.techniques[t]);
    }

    void IndirectDrawSystem::updateExpandDescriptors(
        const std::uint32_t frameIndex)
    {
        if (frameIndex >= mFrames.size() ||
            frameIndex >= mExpandDescriptorSets.size())
            return;

        auto& frame = mFrames[frameIndex];
        if (!frame.sceneInstances || !frame.sourceTransforms ||
            !frame.sourceTransformsTRS || !mPrevSourceTransforms)
            return;

        const auto cap = std::max(frame.capacity, 1u);
        const auto trsInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.sourceTransformsTRS->Get())
                .setOffset(0)
                .setRange(sizeof(SceneTransform) * cap);
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
        const auto prevInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mPrevSourceTransforms->Get())
                .setOffset(0)
                .setRange(
                    sizeof(InstanceTransform) * std::max(mPrevCapacity, 1u));

        const auto set    = mExpandDescriptorSets[frameIndex];
        const auto writes = std::array {
            vk::WriteDescriptorSet()
                .setDstSet(set)
                .setDstBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(trsInfo),
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
                .setBufferInfo(sourceInfo),
            vk::WriteDescriptorSet()
                .setDstSet(set)
                .setDstBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(prevInfo),
        };
        mDevice->Get().updateDescriptorSets(writes, nullptr);
    }

    void IndirectDrawSystem::SyncMeshInfo()
    {
        MeshPoolGpuAccess::FillMeshInfos(*mMeshPool, mMeshInfos);
        MeshPoolGpuAccess::FillMeshLods(*mMeshPool, mMeshLods);

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
        {
            return;
        }

        // ExpandTransforms fills InstanceTransform (model/prevModel + meta).
        // Host only uploads scene meta + packed TRS.
        frame.sceneInstances->Copy(mSceneInstances.data(),
                                   sizeof(SceneInstance) * mInstanceCount);
        frame.sourceTransformsTRS->Copy(
            mSceneTransforms.data(), sizeof(SceneTransform) * mInstanceCount);
        mExpandedThisFrame = false;
    }

    void IndirectDrawSystem::beginFrameUpload(const std::uint32_t frameIndex)
    {
        mFrameIndex = frameIndex % mFrameCount;
        ++mFrameSerial;
        mCullDescRefreshedThisFrame = false;
        mExpandedThisFrame          = false;
    }

    void IndirectDrawSystem::stampSceneVersion()
    {
        ++mSceneVersion;
        if (mFrameIndex < mFrameSceneVersion.size())
            mFrameSceneVersion[mFrameIndex] = mSceneVersion;
    }

    void IndirectDrawSystem::zeroDrawCount(const std::uint32_t techniqueFilter)
    {
        auto& list = drawListFor(currentFrame(), techniqueFilter);
        if (!list.drawCount)
            return;
        mCommandPool->GetCommandBuffer().fillBuffer(
            list.drawCount->Get(), 0, sizeof(std::uint32_t), 0);
    }

    void IndirectDrawSystem::CommitSceneFrame(const std::uint32_t frameIndex)
    {
        beginFrameUpload(frameIndex);
        if (mFrameIndex < mFrameSceneVersion.size() &&
            mFrameSceneVersion[mFrameIndex] == mSceneVersion)
            return;

        if (mMeshInfoDirty || mMeshPool->GetMeshCount() != mMeshInfos.size())
            SyncMeshInfo();

        if (mInstanceCount > 0)
        {
            ensureCapacity(mInstanceCount);
            ensurePrevCapacity(mInstanceCount);
            refreshCullDescriptorsIfNeeded();
            mCullDescRefreshedThisFrame = false;
        }
        uploadFrameBuffers();
        if (mFrameIndex < mFrameSceneVersion.size())
            mFrameSceneVersion[mFrameIndex] = mSceneVersion;
    }

    void IndirectDrawSystem::BeginSceneInstances()
    {
        mStagingOpen = true;
        mStagingCount.store(0, std::memory_order_relaxed);
    }

    void IndirectDrawSystem::ReserveSceneInstances(const std::uint32_t count)
    {
        {
            std::unique_lock lock(mStagingMutex);
            if (mStaging.size() < count)
                mStaging.resize(count);
        }

        // Grow GPU FiF slots up-front so End never hits Buffer::~ waitIdle.
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
            ensureCapacityForFrame(f, count);
        ensurePrevCapacity(count);
        mSceneInstances.reserve(count);
        mSceneTransforms.reserve(count);
    }

    void IndirectDrawSystem::UploadSceneInstances(
        const std::span<const SceneInstanceUpload> uploads)
    {
        const auto n = static_cast<std::uint32_t>(uploads.size());
        if (n == 0)
            return;

        const auto base = mStagingCount.fetch_add(n, std::memory_order_relaxed);

        {
            std::shared_lock readLock(mStagingMutex);
            if (base + n <= mStaging.size())
            {
                std::copy(uploads.begin(), uploads.end(),
                          mStaging.begin() + static_cast<std::ptrdiff_t>(base));
                return;
            }
        }

        std::unique_lock writeLock(mStagingMutex);
        if (base + n > mStaging.size())
        {
            const auto grown = std::max(
                base + n,
                std::max<std::uint32_t>(
                    1u, static_cast<std::uint32_t>(mStaging.size()) * 2u));
            mStaging.resize(grown);
        }
        std::copy(uploads.begin(), uploads.end(),
                  mStaging.begin() + static_cast<std::ptrdiff_t>(base));
    }

    void IndirectDrawSystem::finalizeStagingToHost()
    {
        const auto count   = mStagingCount.load(std::memory_order_relaxed);
        mUsedTechniqueMask = 0;

        if (count == 0)
        {
            mInstanceCount = 0;
            mSceneInstances.clear();
            mSceneTransforms.clear();
            return;
        }

        std::vector<UploadSortKey> sortKeys(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            sortKeys[i] = UploadSortKey {
                .entityId    = mStaging[i].entityId,
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

        mInstanceCount = count;
        mSceneInstances.resize(mInstanceCount);
        mSceneTransforms.resize(mInstanceCount);
        // InstanceTransform is filled on GPU by ExpandTransforms.

        for (std::uint32_t dst = 0; dst < mInstanceCount; ++dst)
        {
            const auto&   src         = mStaging[sortKeys[dst].uploadIndex];
            std::uint32_t techniqueId = src.techniqueId;
            if (techniqueId >= kMaxMaterialTechniques)
                techniqueId = 0;

            auto flags = src.flags;
            if (src.boneOffset != kNoSkin)
                flags |= kSceneInstanceFlagSkinned;

            mUsedTechniqueMask |= (1u << techniqueId);

            mSceneTransforms[dst] = src.transform;

            mSceneInstances[dst] = SceneInstance {
                .model       = glm::mat4(1.0f),
                .meshId      = src.mesh.Id(),
                .materialId  = src.material.Id(),
                .entityId    = src.entityId,
                .flags       = flags,
                .techniqueId = techniqueId,
                .boneOffset  = src.boneOffset,
            };
        }
    }

    void IndirectDrawSystem::EndSceneInstances(const std::uint32_t frameIndex)
    {
        mStagingOpen = false;
        beginFrameUpload(frameIndex);

        if (mMeshInfoDirty || mMeshPool->GetMeshCount() != mMeshInfos.size())
            SyncMeshInfo();

        finalizeStagingToHost();

        if (mInstanceCount > 0)
        {
            ensureCapacity(mInstanceCount);
            ensurePrevCapacity(mInstanceCount);
            refreshCullDescriptorsIfNeeded();
            mCullDescRefreshedThisFrame = false;
        }

        uploadFrameBuffers();
        stampSceneVersion();
    }

    void IndirectDrawSystem::DispatchCull(
        const glm::mat4& viewProj, const CullMode mode, const bool reverseZ,
        const std::uint32_t techniqueFilter)
    {
        if (mInstanceCount == 0)
            return;

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

        CullPushConstants pc {};
        pc.viewProj      = viewProj;
        pc.cameraPos     = glm::vec4(mCameraPos, 0.0f);
        pc.screenSize    = glm::vec2(static_cast<float>(mScreenSize.width),
                                     static_cast<float>(mScreenSize.height));
        pc.instanceCount = mInstanceCount;
        pc.cullMode      = static_cast<std::uint32_t>(mode);
        pc.reverseZ      = reverseZ ? 1u : 0u;
        pc.hizEnabled = (mHiZSafeForFrame && mode == CullMode::Camera && mHiZ &&
                         mHiZ->IsReady())
                            ? 1u
                            : 0u;
        pc.lodPixelRef     = mLodPixelRef;
        pc.lodStep         = mLodStep;
        pc.techniqueFilter = techniqueFilter;
        pc.maxDraws        = currentFrame().capacity;
        pc.hizDepthBias    = 1e-4f;

        if (mode == CullMode::Camera && techniqueFilter == kTechniqueFilterAll)
            mLastCullPushConstants = pc;

        recordDispatchCull(pc);
    }

    void IndirectDrawSystem::DispatchCullExact(const CullPushConstants& pc)
    {
        if (pc.instanceCount == 0 && mInstanceCount == 0)
            return;

        CullPushConstants local = pc;
        if (local.instanceCount == 0)
            local.instanceCount = mInstanceCount;
        if (local.maxDraws == 0)
            local.maxDraws = currentFrame().capacity;

        mLastCullPushConstants = local;
        mCameraPos =
            glm::vec3(local.cameraPos.x, local.cameraPos.y, local.cameraPos.z);
        mScreenSize = vk::Extent2D {
            static_cast<std::uint32_t>(std::max(local.screenSize.x, 1.0f)),
            static_cast<std::uint32_t>(std::max(local.screenSize.y, 1.0f))
        };

        recordDispatchCull(local);
    }

    void IndirectDrawSystem::ensureExpandedThisFrame()
    {
        if (mExpandedThisFrame || mInstanceCount == 0)
            return;

        auto& frame = currentFrame();
        if (!frame.sourceTransformsTRS || !frame.sceneInstances ||
            !frame.sourceTransforms || !mPrevSourceTransforms)
            return;

        updateExpandDescriptors(mFrameIndex);

        auto& cb = mCommandPool->GetCommandBuffer();

        const auto hostToExpand = std::array {
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sourceTransformsTRS->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sceneInstances->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sourceTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(mPrevSourceTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost |
                               vk::PipelineStageFlagBits::eTransfer |
                               vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eComputeShader, {},
                           nullptr, hostToExpand, nullptr);

        ExpandPushConstants epc {};
        epc.instanceCount = mInstanceCount;
        epc.prevCount     = mPrevInstanceCount;

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, mExpandPipeline);
        cb.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mExpandPipelineLayout, 0, 1,
            &mExpandDescriptorSets[mFrameIndex], 0, nullptr);
        cb.pushConstants(mExpandPipelineLayout,
                         vk::ShaderStageFlagBits::eCompute, 0, sizeof(epc),
                         &epc);
        const auto groups = (mInstanceCount + 63u) / 64u;
        cb.dispatch(groups, 1, 1);

        const auto expandToCopy = std::array {
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sourceTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eTransfer, {}, nullptr,
                           expandToCopy, nullptr);

        const auto copyBytes = sizeof(InstanceTransform) *
                               static_cast<vk::DeviceSize>(mInstanceCount);
        const auto region =
            vk::BufferCopy().setSrcOffset(0).setDstOffset(0).setSize(copyBytes);
        cb.copyBuffer(frame.sourceTransforms->Get(),
                      mPrevSourceTransforms->Get(), 1, &region);
        mPrevInstanceCount = mInstanceCount;

        const auto copyToCull = std::array {
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sceneInstances->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eTransferRead)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sourceTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader |
                               vk::PipelineStageFlagBits::eTransfer,
                           vk::PipelineStageFlagBits::eComputeShader, {},
                           nullptr, copyToCull, nullptr);

        mExpandedThisFrame = true;
    }

    void IndirectDrawSystem::recordDispatchCull(const CullPushConstants& pc)
    {
        if (mInstanceCount == 0 && pc.instanceCount == 0)
            return;

        ensureExpandedThisFrame();
        refreshCullDescriptorsIfNeeded();
        zeroDrawCount(pc.techniqueFilter);

        auto&      frame = currentFrame();
        auto&      list  = drawListFor(frame, pc.techniqueFilter);
        auto&      cb    = mCommandPool->GetCommandBuffer();
        const auto set   = cullSetFor(pc.techniqueFilter);

        const auto toCompute = std::array {
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(mMeshInfoBuffer->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(mMeshLodBuffer->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sceneInstances->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.sourceTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead |
                                  vk::AccessFlagBits::eHostWrite |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.indirect->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead |
                                  vk::AccessFlagBits::eTransferWrite |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.drawCount->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eVertexAttributeRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.compactTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
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

        const auto count  = std::max(pc.instanceCount, mInstanceCount);
        const auto groups = (count + 63u) / 64u;
        cb.dispatch(groups, 1, 1);

        const auto toDraw = std::array {
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.indirect->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.drawCount->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.compactTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eDrawIndirect |
                               vk::PipelineStageFlagBits::eVertexInput,
                           vk::DependencyFlags {}, 0, nullptr,
                           static_cast<std::uint32_t>(toDraw.size()),
                           toDraw.data(), 0, nullptr);
    }

    void IndirectDrawSystem::CaptureCullInputs(CullFrameSnapshot& out) const
    {
        out.pushConstants = mLastCullPushConstants;
        out.meshes        = mMeshInfos;
        out.lods          = mMeshLods;
        out.instances     = mSceneInstances;
        out.sources.resize(mSceneInstances.size());
        for (std::uint32_t i = 0; i < out.instances.size(); ++i)
        {
            const auto M =
                i < mSceneTransforms.size() ? mSceneTransforms[i].ToMatrix()
                                            : glm::mat4(1.0f);
            out.instances[i].model = M;
            out.sources[i]         = InstanceTransform {
                        .model      = M,
                        .prevModel  = M,
                        .materialId = mSceneInstances[i].materialId,
                        .entityId   = mSceneInstances[i].entityId,
                        .flags      = mSceneInstances[i].flags,
                        .boneOffset = mSceneInstances[i].boneOffset,
            };
        }
        out.hiz.present = mHiZ && mHiZ->IsValid();
        out.hiz.ready   = mHiZ && mHiZ->IsReady();
        out.hiz.enabled = mLastCullPushConstants.hizEnabled != 0;
        if (mHiZ && mHiZ->IsValid())
        {
            const auto extent = mHiZ->GetExtent();
            out.hiz.width     = extent.width;
            out.hiz.height    = extent.height;
            out.hiz.mipCount  = mHiZ->GetMipLevels();
            out.hiz.file      = "hiz.r32f";
        }
    }

    bool IndirectDrawSystem::ReadbackCullOutputs(
        const std::uint32_t        frameIndex,
        const std::uint32_t        techniqueFilter,
        std::uint32_t&             outDrawCount,
        std::vector<CullSurvivor>& outSurvivors)
    {
        outDrawCount = 0;
        outSurvivors.clear();

        if (mFrames.empty())
            return false;

        const auto prevFrame = mFrameIndex;
        mFrameIndex          = frameIndex % mFrameCount;

        auto& frame = currentFrame();
        auto& list  = drawListFor(frame, techniqueFilter);
        if (!list.drawCount || !list.compactTransforms)
        {
            mFrameIndex = prevFrame;
            return false;
        }

        const auto countBytes = sizeof(std::uint32_t);
        const auto maxDraws   = frame.capacity;
        const auto compactBytes =
            sizeof(InstanceTransform) * static_cast<std::size_t>(maxDraws);

        auto countStaging =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Readback)
                .SetSize(countBytes)
                .Build();
        auto compactStaging =
            BufferBuilder(mDevice)
                .SetUsage(BufferUsage::Readback)
                .SetSize(compactBytes)
                .Build();
        if (!countStaging || !countStaging->GetMapped() || !compactStaging ||
            !compactStaging->GetMapped())
        {
            mFrameIndex = prevFrame;
            return false;
        }

        mDevice->Get().waitIdle();

        auto cb = mCommandPool->CreateCommandBuffer();
        cb.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        const auto barriers = std::array {
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eIndirectCommandRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.drawCount->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eVertexAttributeRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(list.compactTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader |
                               vk::PipelineStageFlagBits::eDrawIndirect |
                               vk::PipelineStageFlagBits::eVertexInput,
                           vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr,
                           static_cast<std::uint32_t>(barriers.size()),
                           barriers.data(), 0, nullptr);

        cb.copyBuffer(list.drawCount->Get(), countStaging->Get(),
                      vk::BufferCopy().setSize(countBytes));
        cb.copyBuffer(list.compactTransforms->Get(), compactStaging->Get(),
                      vk::BufferCopy().setSize(compactBytes));
        cb.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cb);
        mDevice->GetGraphicsQueue().submit(submitInfo);
        mDevice->GetGraphicsQueue().waitIdle();
        mCommandPool->FreeCommandBuffer(cb);

        std::memcpy(&outDrawCount, countStaging->GetMapped(), countBytes);
        outDrawCount = std::min(outDrawCount, maxDraws);

        const auto* compact =
            static_cast<const InstanceTransform*>(compactStaging->GetMapped());
        outSurvivors.resize(outDrawCount);
        for (std::uint32_t i = 0; i < outDrawCount; ++i)
        {
            outSurvivors[i].entityId = compact[i].entityId;
            outSurvivors[i].slot     = i;
            outSurvivors[i].meshId   = 0;
            for (const auto& inst : mSceneInstances)
            {
                if (inst.entityId == compact[i].entityId)
                {
                    outSurvivors[i].meshId = inst.meshId;
                    break;
                }
            }
        }
        mFrameIndex = prevFrame;
        return true;
    }

    bool IndirectDrawSystem::ReadbackCullOutputsAggregated(
        const std::uint32_t        frameIndex,
        std::uint32_t&             outDrawCount,
        std::vector<CullSurvivor>& outSurvivors)
    {
        outDrawCount = 0;
        outSurvivors.clear();

        std::vector<CullSurvivor> merged;
        merged.reserve(64);

        auto appendUnique = [&](const std::vector<CullSurvivor>& part) {
            for (const auto& s : part)
            {
                bool found = false;
                for (const auto& e : merged)
                {
                    if (e.entityId == s.entityId)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    merged.push_back(s);
            }
        };

        std::uint32_t             count = 0;
        std::vector<CullSurvivor> part;
        // Technique lists hold Camera opaque survivors (DeferredGeometry).
        for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
        {
            if (!ReadbackCullOutputs(frameIndex, t, count, part))
                return false;
            appendUnique(part);
        }
        // Main list is reused by Translucent / Shadow; still include whatever
        // remains (typically translucent survivors at end of frame).
        if (!ReadbackCullOutputs(frameIndex, kTechniqueFilterAll, count, part))
            return false;
        appendUnique(part);

        outSurvivors = std::move(merged);
        outDrawCount = static_cast<std::uint32_t>(outSurvivors.size());
        return true;
    }

    bool IndirectDrawSystem::CaptureHiZ(CullHiZDump& out)
    {
        out = {};
        if (!mHiZ || !mHiZ->IsValid())
            return false;

        const auto extent = mHiZ->GetExtent();
        out.present       = true;
        out.ready         = mHiZ->IsReady();
        out.enabled       = mLastCullPushConstants.hizEnabled != 0;
        out.width         = extent.width;
        out.height        = extent.height;
        out.mipCount      = mHiZ->GetMipLevels();
        out.file          = "hiz.r32f";
        if (!out.ready)
            return true;
        return mHiZ->ReadbackMips(mCommandPool, out.pixels);
    }

    bool IndirectDrawSystem::UploadHiZFromDump(const CullHiZDump& dump)
    {
        if (!mHiZ || !dump.present || dump.pixels.empty() || dump.width == 0 ||
            dump.height == 0)
            return false;

        if (!mHiZ->UploadMips(mCommandPool, dump.width, dump.height,
                              dump.pixels))
            return false;

        bumpCullDescVersion();
        mCullDescRefreshedThisFrame = false;
        refreshCullDescriptorsIfNeeded();
        return true;
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
        mMeshPool->BindGeometry(mCommandPool);

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
