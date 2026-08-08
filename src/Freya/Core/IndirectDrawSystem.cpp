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
            std::uint32_t vertexBufferIndex = 0;
            std::uint32_t indexBufferIndex  = 0;
            std::uint32_t meshId            = 0;
            std::uint32_t entityId          = 0;
            std::uint32_t uploadIndex       = 0;
        };

        bool UploadSortKeyLess(const UploadSortKey& a, const UploadSortKey& b)
        {
            if (a.vertexBufferIndex != b.vertexBufferIndex)
                return a.vertexBufferIndex < b.vertexBufferIndex;
            if (a.indexBufferIndex != b.indexBufferIndex)
                return a.indexBufferIndex < b.indexBufferIndex;
            if (a.meshId != b.meshId)
                return a.meshId < b.meshId;
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
            cullDescriptorSets) :
        mDevice(device), mCommandPool(commandPool), mMeshPool(meshPool),
        mMaterials(materials), mFrameCount(std::max(1u, frameCount)),
        mCullPipeline(cullPipeline), mCullPipelineLayout(cullPipelineLayout),
        mCullSetLayout(cullSetLayout), mCullDescriptorPool(cullDescriptorPool),
        mCullDescriptorSets(std::move(cullDescriptorSets))
    {
        mFrames.resize(mFrameCount);
        ensureCapacity(kInitialInstanceCapacity);
        SyncMeshInfo();
    }

    IndirectDrawSystem::~IndirectDrawSystem()
    {
        auto& vkDevice = mDevice->Get();
        vkDevice.destroyPipeline(mCullPipeline);
        vkDevice.destroyPipelineLayout(mCullPipelineLayout);
        vkDevice.destroyDescriptorPool(mCullDescriptorPool);
        vkDevice.destroyDescriptorSetLayout(mCullSetLayout);
    }

    IndirectDrawSystem::FrameResources& IndirectDrawSystem::currentFrame()
    {
        return mFrames[mFrameIndex % mFrameCount];
    }

    void IndirectDrawSystem::ensureCapacity(const std::uint32_t instanceCount)
    {
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
        {
            auto& frame = mFrames[f];
            if (instanceCount <= frame.capacity && frame.sceneInstances &&
                frame.batchIds && frame.sourceTransforms &&
                frame.compactTransforms && frame.indirect)
            {
                continue;
            }

            const auto capacity = std::max(
                instanceCount,
                std::max(frame.capacity * 2, kInitialInstanceCapacity));

            frame.sceneInstances =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(SceneInstance) * capacity)
                    .Build();
            frame.batchIds = BufferBuilder(mDevice)
                                 .SetUsage(BufferUsage::Storage)
                                 .SetSize(sizeof(std::uint32_t) * capacity)
                                 .Build();
            frame.sourceTransforms =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(InstanceTransform) * capacity)
                    .Build();
            frame.compactTransforms =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Instance)
                    .SetSize(sizeof(InstanceTransform) * capacity)
                    .Build();
            // One command per batch; worst case = one mesh per instance.
            frame.indirect =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Indirect)
                    .SetSize(sizeof(vk::DrawIndexedIndirectCommand) * capacity)
                    .Build();
            frame.capacity = capacity;
            updateCullDescriptors(f);
        }

        mSceneInstances.reserve(instanceCount);
        mInstanceTransforms.reserve(instanceCount);
        mInstanceBatchIds.reserve(instanceCount);
        mIndirectCommands.reserve(instanceCount);
    }

    void IndirectDrawSystem::updateCullDescriptors(
        const std::uint32_t frameIndex)
    {
        if (!mMeshInfoBuffer || frameIndex >= mFrames.size() ||
            frameIndex >= mCullDescriptorSets.size())
        {
            return;
        }

        auto& frame = mFrames[frameIndex];
        if (!frame.sceneInstances || !frame.batchIds ||
            !frame.sourceTransforms || !frame.compactTransforms ||
            !frame.indirect)
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
        const auto indirectInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.indirect->Get())
                .setOffset(0)
                .setRange(sizeof(vk::DrawIndexedIndirectCommand) * cap);
        const auto sourceInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.sourceTransforms->Get())
                .setOffset(0)
                .setRange(sizeof(InstanceTransform) * cap);
        const auto compactInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.compactTransforms->Get())
                .setOffset(0)
                .setRange(sizeof(InstanceTransform) * cap);
        const auto batchInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.batchIds->Get())
                .setOffset(0)
                .setRange(sizeof(std::uint32_t) * cap);

        const auto set    = mCullDescriptorSets[frameIndex];
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
                .setBufferInfo(batchInfo),
        };
        mDevice->Get().updateDescriptorSets(writes, nullptr);
    }

    void IndirectDrawSystem::SyncMeshInfo()
    {
        mMeshPool->FillMeshInfos(mMeshInfos);
        const auto count =
            std::max(static_cast<std::uint32_t>(mMeshInfos.size()), 1u);

        if (count > mMeshInfoCapacity || !mMeshInfoBuffer)
        {
            mMeshInfoBuffer =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(MeshInfo) * count)
                    .Build();
            mMeshInfoCapacity = count;
            for (std::uint32_t f = 0; f < mFrameCount; ++f)
                updateCullDescriptors(f);
        }

        if (!mMeshInfos.empty())
        {
            mMeshInfoBuffer->Copy(mMeshInfos.data(),
                                  sizeof(MeshInfo) * mMeshInfos.size());
        }
        mMeshInfoDirty = false;
    }

    void IndirectDrawSystem::buildBatches()
    {
        mIndirectCommands.clear();
        mChunkRuns.clear();
        mInstanceBatchIds.assign(mInstanceCount, 0);

        if (mInstanceCount == 0)
            return;

        std::uint32_t runStart = 0;
        while (runStart < mInstanceCount)
        {
            const auto meshId = mSceneInstances[runStart].meshId;
            MeshInfo   mesh {};
            if (meshId < mMeshInfos.size())
                mesh = mMeshInfos[meshId];

            std::uint32_t runEnd = runStart + 1;
            while (runEnd < mInstanceCount &&
                   mSceneInstances[runEnd].meshId == meshId)
            {
                ++runEnd;
            }

            const auto batchIndex =
                static_cast<std::uint32_t>(mIndirectCommands.size());
            for (std::uint32_t i = runStart; i < runEnd; ++i)
                mInstanceBatchIds[i] = batchIndex;

            mIndirectCommands.push_back(
                vk::DrawIndexedIndirectCommand {}
                    .setIndexCount(mesh.indexCount)
                    .setInstanceCount(0) // filled by cull atomic compact
                    .setFirstIndex(mesh.firstIndex)
                    .setVertexOffset(mesh.vertexOffset)
                    .setFirstInstance(runStart));

            if (mChunkRuns.empty() ||
                mChunkRuns.back().vertexBufferIndex != mesh.vertexBufferIndex ||
                mChunkRuns.back().indexBufferIndex != mesh.indexBufferIndex)
            {
                mChunkRuns.push_back(ChunkRun {
                    .vertexBufferIndex = mesh.vertexBufferIndex,
                    .indexBufferIndex  = mesh.indexBufferIndex,
                    .firstDraw         = batchIndex,
                    .drawCount         = 1,
                });
            }
            else
            {
                ++mChunkRuns.back().drawCount;
            }

            runStart = runEnd;
        }
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
        frame.batchIds->Copy(mInstanceBatchIds.data(),
                             sizeof(std::uint32_t) * mInstanceCount);
        uploadIndirectZeros();
    }

    void IndirectDrawSystem::uploadIndirectZeros()
    {
        auto& frame = currentFrame();
        if (mIndirectCommands.empty() || !frame.indirect)
            return;

        // Ensure instanceCount is 0 before each cull atomic pass.
        for (auto& cmd : mIndirectCommands)
            cmd.setInstanceCount(0);

        frame.indirect->Copy(
            mIndirectCommands.data(),
            sizeof(vk::DrawIndexedIndirectCommand) * mIndirectCommands.size());
    }

    void IndirectDrawSystem::UploadSceneInstances(
        const std::span<const SceneInstanceUpload> uploads,
        const std::uint32_t                        frameIndex)
    {
        mFrameIndex = frameIndex % mFrameCount;

        if (uploads.empty())
        {
            mInstanceCount = 0;
            mSceneInstances.clear();
            mInstanceTransforms.clear();
            mInstanceBatchIds.clear();
            mIndirectCommands.clear();
            mChunkRuns.clear();
            return;
        }

        if (mMeshInfoDirty || mMeshPool->GetMeshCount() != mMeshInfos.size())
            SyncMeshInfo();

        ensureCapacity(static_cast<std::uint32_t>(uploads.size()));

        // Swap history buffers (no deep copy of matrices).
        mPrevTransforms.swap(mInstanceTransforms);
        mPrevModelByEntity.clear();
        mPrevModelByEntity.reserve(mPrevTransforms.size());
        for (const auto& prev : mPrevTransforms)
            mPrevModelByEntity[prev.entityId] = prev.model;

        // Build sort keys with cached mesh buffer indices (one GetMesh each).
        std::vector<UploadSortKey> sortKeys(uploads.size());
        for (std::uint32_t i = 0; i < uploads.size(); ++i)
        {
            const auto& mesh = mMeshPool->GetMesh(uploads[i].meshId);
            sortKeys[i]      = UploadSortKey {
                .vertexBufferIndex = mesh.vertexBufferIndex,
                .indexBufferIndex  = mesh.indexBufferIndex,
                .meshId            = uploads[i].meshId,
                .entityId          = uploads[i].entityId,
                .uploadIndex       = i,
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
            const auto  flags =
                src.castShadows ? kSceneInstanceFlagCastShadows : 0u;

            mSceneInstances[dst] = SceneInstance {
                .model      = src.model,
                .meshId     = src.meshId,
                .materialId = src.materialId,
                .entityId   = src.entityId,
                .flags      = flags,
            };

            glm::mat4 prev = src.model;
            if (const auto it = mPrevModelByEntity.find(src.entityId);
                it != mPrevModelByEntity.end())
            {
                prev = it->second;
            }

            mInstanceTransforms[dst] = InstanceTransform {
                .model      = src.model,
                .prevModel  = prev,
                .materialId = src.materialId,
                .entityId   = src.entityId,
                .flags      = flags,
            };
        }

        buildBatches();
        uploadFrameBuffers();
    }

    void IndirectDrawSystem::DispatchCull(
        const glm::mat4& viewProj, const CullMode mode, const bool reverseZ)
    {
        if (mInstanceCount == 0 || mIndirectCommands.empty())
            return;

        if (mMeshInfoDirty)
            SyncMeshInfo();

        // Reset per-batch visible counts before atomic compaction.
        uploadIndirectZeros();

        CullPushConstants pc {};
        pc.viewProj      = viewProj;
        pc.instanceCount = mInstanceCount;
        pc.cullMode      = static_cast<std::uint32_t>(mode);
        pc.reverseZ      = reverseZ ? 1u : 0u;

        auto&      frame = currentFrame();
        auto&      cb    = mCommandPool->GetCommandBuffer();
        const auto set   = mCullDescriptorSets[mFrameIndex];

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
                .setSrcAccessMask(vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.batchIds->Get())
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
                .setBuffer(frame.indirect->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eVertexAttributeRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.compactTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };

        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eHost |
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
                .setBuffer(frame.indirect->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.compactTransforms->Get())
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
        const bool bindMaterials, const vk::PipelineLayout pipelineLayout)
    {
        if (mInstanceCount == 0 || mChunkRuns.empty())
            return;

        auto& frame = currentFrame();
        auto& cb    = mCommandPool->GetCommandBuffer();

        frame.compactTransforms->Bind(mCommandPool);

        if (bindMaterials && pipelineLayout)
        {
            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, 1,
                &mMaterials->GetBindlessSet(), 0, nullptr);
        }

        for (const auto& run : mChunkRuns)
        {
            mMeshPool->BindChunk(run.vertexBufferIndex, run.indexBufferIndex);
            cb.drawIndexedIndirect(
                frame.indirect->Get(),
                sizeof(vk::DrawIndexedIndirectCommand) * run.firstDraw,
                run.drawCount,
                sizeof(vk::DrawIndexedIndirectCommand));
        }
    }

} // namespace FREYA_NAMESPACE
