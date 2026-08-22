#include "Freya/Core/IndirectDrawSystem.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
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
        const skr::Arc<MaterialPool>&                materialPool,
        const std::uint32_t                          frameCount,
        const vk::Pipeline                           cullPipeline,
        const vk::PipelineLayout                     cullPipelineLayout,
        const vk::DescriptorSetLayout                cullSetLayout,
        const vk::DescriptorPool                     cullDescriptorPool,
        std::vector<vk::DescriptorSet>
            cullDescriptorSets,
        skr::Arc<HiZPyramid>
            hiz,
        skr::Arc<Image>
            hizFallbackImage) :
        mDevice(device), mCommandPool(commandPool), mMeshPool(meshPool),
        mMaterials(materials), mMaterialPool(materialPool),
        mFrameCount(std::max(1u, frameCount)), mCullPipeline(cullPipeline),
        mCullPipelineLayout(cullPipelineLayout), mCullSetLayout(cullSetLayout),
        mCullDescriptorPool(cullDescriptorPool),
        mCullDescriptorSets(std::move(cullDescriptorSets)),
        mHiZ(std::move(hiz)), mHizFallbackImage(std::move(hizFallbackImage))
    {
        mFrames.resize(mFrameCount);
        mFrameCullDescVersion.assign(mFrameCount, 0);
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
            ensureCapacityForFrame(f, kInitialInstanceCapacity);
        SyncMeshInfo();
    }

    IndirectDrawSystem::~IndirectDrawSystem()
    {
        auto& vkDevice = mDevice->Get();
        vkDevice.destroyPipeline(mCullPipeline);
        vkDevice.destroyPipelineLayout(mCullPipelineLayout);
        vkDevice.destroyDescriptorPool(mCullDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mCullSetLayout);
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
        frame.main = makeDrawList();
        for (auto& tech : frame.techniques)
            tech = makeDrawList();
        frame.capacity = capacity;

        bumpCullDescVersion();
        updateCullDescriptors(frameIndex);
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

    void IndirectDrawSystem::zeroDrawCount(const std::uint32_t techniqueFilter)
    {
        auto& list = drawListFor(currentFrame(), techniqueFilter);
        if (!list.drawCount)
            return;
        mCommandPool->GetCommandBuffer().fillBuffer(
            list.drawCount->Get(), 0, sizeof(std::uint32_t), 0);
    }

    void IndirectDrawSystem::UploadSceneInstances(
        const std::span<const SceneInstanceUpload> uploads,
        const std::uint32_t                        frameIndex)
    {
        mFrameIndex                 = frameIndex % mFrameCount;
        mCullDescRefreshedThisFrame = false;

        if (uploads.empty())
        {
            mInstanceCount = 0;
            mSceneInstances.clear();
            mInstanceTransforms.clear();
            mUsedTechniqueMask = 0;
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
    }

    void IndirectDrawSystem::DispatchCull(
        const glm::mat4& viewProj, const CullMode mode, const bool reverseZ,
        const std::uint32_t techniqueFilter)
    {
        if (mInstanceCount == 0)
            return;

        refreshCullDescriptorsIfNeeded();

        zeroDrawCount(techniqueFilter);

        CullPushConstants pc {};
        pc.viewProj      = viewProj;
        pc.cameraPos     = glm::vec4(mCameraPos, 0.0f);
        pc.screenSize    = glm::vec2(static_cast<float>(mScreenSize.width),
                                     static_cast<float>(mScreenSize.height));
        pc.instanceCount = mInstanceCount;
        pc.cullMode      = static_cast<std::uint32_t>(mode);
        pc.reverseZ      = reverseZ ? 1u : 0u;
        pc.hizEnabled =
            (mode == CullMode::Camera && mHiZ && mHiZ->IsReady()) ? 1u : 0u;
        pc.lodPixelRef     = 256.0f;
        pc.lodStep         = 2.0f;
        pc.techniqueFilter = techniqueFilter;

        auto&      frame = currentFrame();
        auto&      list  = drawListFor(frame, techniqueFilter);
        auto&      cb    = mCommandPool->GetCommandBuffer();
        const auto set   = cullSetFor(techniqueFilter);

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

        const auto groups = (mInstanceCount + 63u) / 64u;
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
