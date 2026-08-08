#include "Freya/Core/IndirectDrawSystem.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cstring>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kInitialInstanceCapacity = 256;
    }

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
        std::vector<vk::DescriptorSet>               cullDescriptorSets) :
        mDevice(device), mCommandPool(commandPool), mMeshPool(meshPool),
        mMaterials(materials), mFrameCount(std::max(1u, frameCount)),
        mCullPipeline(cullPipeline), mCullPipelineLayout(cullPipelineLayout),
        mCullSetLayout(cullSetLayout),
        mCullDescriptorPool(cullDescriptorPool),
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
                frame.indirect && frame.instanceTransforms)
            {
                continue;
            }

            const auto capacity =
                std::max(instanceCount,
                         std::max(frame.capacity * 2, kInitialInstanceCapacity));

            frame.sceneInstances =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Storage)
                    .SetSize(sizeof(SceneInstance) * capacity)
                    .Build();
            frame.indirect =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Indirect)
                    .SetSize(sizeof(vk::DrawIndexedIndirectCommand) * capacity)
                    .Build();
            frame.instanceTransforms =
                BufferBuilder(mDevice)
                    .SetUsage(BufferUsage::Instance)
                    .SetSize(sizeof(InstanceTransform) * capacity)
                    .Build();
            frame.capacity = capacity;
            updateCullDescriptors(f);
        }

        mSceneInstances.reserve(instanceCount);
        mInstanceTransforms.reserve(instanceCount);
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
        if (!frame.sceneInstances || !frame.indirect)
            return;

        const auto meshInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mMeshInfoBuffer->Get())
                .setOffset(0)
                .setRange(sizeof(MeshInfo) *
                          std::max(mMeshInfoCapacity, 1u));
        const auto sceneInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.sceneInstances->Get())
                .setOffset(0)
                .setRange(sizeof(SceneInstance) *
                          std::max(frame.capacity, 1u));
        const auto indirectInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(frame.indirect->Get())
                .setOffset(0)
                .setRange(sizeof(vk::DrawIndexedIndirectCommand) *
                          std::max(frame.capacity, 1u));

        const auto set = mCullDescriptorSets[frameIndex];
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

    void IndirectDrawSystem::fillIndirectCommandsCpu()
    {
        mIndirectCommands.resize(mInstanceCount);
        mChunkRuns.clear();

        for (std::uint32_t i = 0; i < mInstanceCount; ++i)
        {
            const auto& inst = mSceneInstances[i];
            MeshInfo    mesh {};
            if (inst.meshId < mMeshInfos.size())
                mesh = mMeshInfos[inst.meshId];

            mIndirectCommands[i] = vk::DrawIndexedIndirectCommand {}
                                       .setIndexCount(mesh.indexCount)
                                       .setInstanceCount(1)
                                       .setFirstIndex(mesh.firstIndex)
                                       .setVertexOffset(mesh.vertexOffset)
                                       .setFirstInstance(i);

            if (mChunkRuns.empty() ||
                mChunkRuns.back().vertexBufferIndex !=
                    mesh.vertexBufferIndex ||
                mChunkRuns.back().indexBufferIndex != mesh.indexBufferIndex)
            {
                mChunkRuns.push_back(ChunkRun {
                    .vertexBufferIndex = mesh.vertexBufferIndex,
                    .indexBufferIndex  = mesh.indexBufferIndex,
                    .firstDraw         = i,
                    .drawCount         = 1,
                });
            }
            else
            {
                ++mChunkRuns.back().drawCount;
            }
        }
    }

    void IndirectDrawSystem::uploadFrameBuffers()
    {
        auto& frame = currentFrame();
        if (mInstanceCount == 0 || !frame.sceneInstances)
            return;

        frame.sceneInstances->Copy(mSceneInstances.data(),
                                   sizeof(SceneInstance) * mInstanceCount);
        frame.instanceTransforms->Copy(
            mInstanceTransforms.data(),
            sizeof(InstanceTransform) * mInstanceCount);
        frame.indirect->Copy(
            mIndirectCommands.data(),
            sizeof(vk::DrawIndexedIndirectCommand) * mInstanceCount);
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
            mChunkRuns.clear();
            return;
        }

        if (mMeshInfoDirty || mMeshPool->GetMeshCount() != mMeshInfos.size())
            SyncMeshInfo();

        ensureCapacity(static_cast<std::uint32_t>(uploads.size()));

        std::vector<std::uint32_t> order(uploads.size());
        for (std::uint32_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::stable_sort(
            order.begin(), order.end(),
            [&](const std::uint32_t a, const std::uint32_t b) {
                const auto& ma = mMeshPool->GetMesh(uploads[a].meshId);
                const auto& mb = mMeshPool->GetMesh(uploads[b].meshId);
                if (ma.vertexBufferIndex != mb.vertexBufferIndex)
                    return ma.vertexBufferIndex < mb.vertexBufferIndex;
                if (ma.indexBufferIndex != mb.indexBufferIndex)
                    return ma.indexBufferIndex < mb.indexBufferIndex;
                return uploads[a].meshId < uploads[b].meshId;
            });

        const bool newFrame = frameIndex != mHistoryFrame;
        mHistoryFrame       = frameIndex;

        const auto prevTransforms = mInstanceTransforms;
        mInstanceCount = static_cast<std::uint32_t>(uploads.size());
        mSceneInstances.resize(mInstanceCount);
        mInstanceTransforms.resize(mInstanceCount);

        for (std::uint32_t dst = 0; dst < mInstanceCount; ++dst)
        {
            const auto& src = uploads[order[dst]];
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
            if (newFrame && dst < prevTransforms.size())
                prev = prevTransforms[dst].model;

            mInstanceTransforms[dst] = InstanceTransform {
                .model      = src.model,
                .prevModel  = prev,
                .materialId = src.materialId,
                .entityId   = src.entityId,
                .flags      = flags,
            };
        }

        fillIndirectCommandsCpu();
        uploadFrameBuffers();
    }

    void IndirectDrawSystem::DispatchCull(const glm::mat4& viewProj,
                                          const CullMode   mode,
                                          const bool       reverseZ)
    {
        if (mInstanceCount == 0)
            return;

        if (mMeshInfoDirty)
            SyncMeshInfo();

        CullPushConstants pc {};
        pc.viewProj      = viewProj;
        pc.instanceCount = mInstanceCount;
        pc.cullMode      = static_cast<std::uint32_t>(mode);
        pc.reverseZ      = reverseZ ? 1u : 0u;

        auto&       frame = currentFrame();
        auto&       cb    = mCommandPool->GetCommandBuffer();
        const auto  set   = mCullDescriptorSets[mFrameIndex];

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
                .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.instanceTransforms->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead |
                                  vk::AccessFlagBits::eHostWrite |
                                  vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.indirect->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE),
        };

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eHost |
                               vk::PipelineStageFlagBits::eDrawIndirect |
                               vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eComputeShader |
                               vk::PipelineStageFlagBits::eVertexInput,
                           vk::DependencyFlags {}, 0, nullptr,
                           static_cast<std::uint32_t>(toCompute.size()),
                           toCompute.data(), 0, nullptr);

        cb.bindPipeline(vk::PipelineBindPoint::eCompute, mCullPipeline);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                              mCullPipelineLayout, 0, 1, &set, 0, nullptr);
        cb.pushConstants(mCullPipelineLayout,
                         vk::ShaderStageFlagBits::eCompute, 0,
                         sizeof(CullPushConstants), &pc);

        const auto groups = (mInstanceCount + 63u) / 64u;
        cb.dispatch(groups, 1, 1);

        const auto toDraw =
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderWrite)
                .setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(frame.indirect->Get())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE);

        cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                           vk::PipelineStageFlagBits::eDrawIndirect,
                           vk::DependencyFlags {}, 0, nullptr, 1, &toDraw, 0,
                           nullptr);
    }

    void IndirectDrawSystem::ExecuteDraws(
        const bool bindMaterials, const vk::PipelineLayout pipelineLayout)
    {
        if (mInstanceCount == 0 || mChunkRuns.empty())
            return;

        auto& frame = currentFrame();
        auto& cb    = mCommandPool->GetCommandBuffer();

        frame.instanceTransforms->Bind(mCommandPool);

        if (bindMaterials && pipelineLayout)
        {
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  pipelineLayout, 1, 1,
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
