#include "Freya/Asset/LODService.hpp"
#include "Freya/Asset/LODPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{

    LODService::LODService(const Ref<skr::ServiceProvider>& serviceProvider) :
        mDevice(serviceProvider->GetService<Device>()),
        mPhysicalDevice(serviceProvider->GetService<PhysicalDevice>()),
        mCommandPool(serviceProvider->GetService<CommandPool>()),
        mLODPool(serviceProvider->GetService<LODPool>()),
        mMeshPool(serviceProvider->GetService<MeshPool>()),
        mRenderer(serviceProvider->GetService<Renderer>()),
        mFreyaOptions(serviceProvider->GetService<FreyaOptions>()),
        mLogger(serviceProvider->GetService<skr::Logger<LODService>>()),
        mServiceProvider(serviceProvider)
    {
        mLogger->LogInformation(
            "Initializing LOD service with {} max instances", mMaxInstances);

        // Initialize push constants
        mPushConstants.cameraPosition     = glm::vec3(0.0f);
        mPushConstants.globalDrawDistance = mFreyaOptions->drawDistance;
        mPushConstants.instanceCount      = 0;
        mPushConstants.lodLevelCount      = mLODPool->GetAllLevels().size();

        // Pre-allocate instance data array
        mInstances.resize(mMaxInstances);
        for (auto& instance : mInstances)
        {
            instance = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
        }

        createGPUBuffers();
        createDescriptorSetLayout();
        allocateDescriptorSet();
        createDrawMetaGraphicsDescriptor();
        createComputePipeline();
        createDitherTexture();
        createComputeCommandPool();
        updateMeshMetadata();

        mLogger->LogInformation("LOD service initialized successfully");
    }

    LODService::~LODService()
    {
        if (mLODComputePipeline)
        {
            mDevice->Get().destroyPipeline(mLODComputePipeline);
        }
        if (mLODComputePipelineLayout)
        {
            mDevice->Get().destroyPipelineLayout(mLODComputePipelineLayout);
        }
        if (mDescriptorSetLayout)
        {
            mDevice->Get().destroyDescriptorSetLayout(mDescriptorSetLayout);
        }
        if (mDescriptorPool)
        {
            mDevice->Get().destroyDescriptorPool(mDescriptorPool);
        }
        if (mDrawMetaGraphicsLayout)
        {
            mDevice->Get().destroyDescriptorSetLayout(mDrawMetaGraphicsLayout);
        }
        if (mDrawMetaGraphicsPool)
        {
            mDevice->Get().destroyDescriptorPool(mDrawMetaGraphicsPool);
        }
    }

    void LODService::createGPUBuffers()
    {
        // Instance buffer - holds LODInstanceData for each instance
        // Use Storage buffer usage for compute shader access
        const auto instanceBufferSize = sizeof(LODInstanceData) * mMaxInstances;
        mInstanceBuffer =
            BufferBuilder(mDevice)
                .SetSize(instanceBufferSize)
                .SetUsage(BufferUsage::Storage)
                .Build();

        // Draw command buffer - holds DrawIndexedIndirectCommand
        // Use Indirect buffer usage for MDI draw commands
        const auto drawCmdSize =
            sizeof(DrawIndexedIndirectCommand) * mMaxInstances;
        mDrawCommandBuffer =
            BufferBuilder(mDevice)
                .SetSize(drawCmdSize)
                .SetUsage(BufferUsage::Indirect)
                .Build();

        // Draw metadata buffer - stores materialId per draw command
        // Written by compute shader, read by vertex shader via gl_DrawIDARB
        const auto drawMetadataSize = sizeof(std::uint32_t) * mMaxInstances;
        mDrawMetadataBuffer =
            BufferBuilder(mDevice)
                .SetSize(drawMetadataSize)
                .SetUsage(BufferUsage::Storage)
                .Build();

        // Draw count buffer - atomic counter
        // Use Storage buffer usage for atomic operations
        mDrawCountBuffer =
            BufferBuilder(mDevice)
                .SetSize(sizeof(std::uint32_t))
                .SetUsage(BufferUsage::Storage)
                .Build();

        // Transform buffer - holds mat4 transforms (placeholder, sized for max
        // instances)
        // Use Storage buffer usage for compute shader read access
        const auto transformBufferSize = sizeof(glm::mat4) * mMaxInstances;
        mTransformBuffer =
            BufferBuilder(mDevice)
                .SetSize(transformBufferSize)
                .SetUsage(BufferUsage::Storage)
                .Build();

        // LOD levels buffer - flat array of all LOD levels
        // Use Storage buffer usage for compute shader read access
        const auto lodLevelsSize =
            sizeof(LODLevel) * mLODPool->GetAllLevels().size();
        mLODLevelsBuffer =
            BufferBuilder(mDevice)
                .SetSize(lodLevelsSize > 256 ? lodLevelsSize : 256)
                .SetUsage(BufferUsage::Storage)
                .Build();

        // Mesh metadata buffer - per-mesh index/vertex info for compute shader
        // Sized for max meshes (4096 as per MeshSet default capacity)
        const auto meshMetadataSize = sizeof(MeshMetadata) * 4096;
        mMeshMetadataBuffer =
            BufferBuilder(mDevice)
                .SetSize(meshMetadataSize)
                .SetUsage(BufferUsage::Storage)
                .Build();

        mLogger->LogInformation(
            "Created GPU buffers: instance={}, drawCmd={}, drawMeta={}, "
            "drawCount={}, transform={}, lodLevels={}, meshMeta={}",
            instanceBufferSize, drawCmdSize, drawMetadataSize,
            sizeof(std::uint32_t),
            transformBufferSize, lodLevelsSize, meshMetadataSize);
    }

    void LODService::createDescriptorSetLayout()
    {
        // Binding layout for LOD descriptor set (set = 2)
        // Bindings:
        // 0: LODInstanceData (readonly storage buffer)
        // 1: Transform buffer (readonly storage buffer)
        // 2: LODLevel buffer (readonly storage buffer)
        // 3: Mesh metadata buffer (readonly storage buffer)
        // 4: Draw command buffer (writeable storage buffer)
        // 5: Visible instances buffer (writeable storage buffer)
        // 6: Draw count (atomic counter)
        // 7: Draw metadata (writeable storage buffer for materialId per draw)
        const std::array<vk::DescriptorSetLayoutBinding, 8> bindings = { {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(6)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(7)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        } };

        const auto layoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings);

        mDescriptorSetLayout =
            mDevice->Get().createDescriptorSetLayout(layoutInfo);

        mLogger->LogInformation("Created LOD descriptor set layout");
    }

    void LODService::allocateDescriptorSet()
    {
        // Create descriptor pool for LOD service
        // 9 storage buffers = 8 for compute + 1 for graphics draw metadata
        // But we use separate pools, so keep compute pool at 8
        std::array<vk::DescriptorPoolSize, 2> poolSizes = { {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(8), // 8 storage bindings for compute
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1), // Dither texture
        } };

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.setPoolSizeCount(static_cast<std::uint32_t>(poolSizes.size()));
        poolInfo.setPPoolSizes(poolSizes.data());
        poolInfo.setMaxSets(1);

        mDescriptorPool = mDevice->Get().createDescriptorPool(poolInfo);

        // Allocate descriptor set
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.setDescriptorPool(mDescriptorPool);
        allocInfo.setSetLayouts(mDescriptorSetLayout);

        mLODDescriptorSet = mDevice->Get().allocateDescriptorSets(allocInfo)[0];

        // Update descriptor set with buffer bindings
        std::array<vk::WriteDescriptorSet, 8> descriptorWrites = { {
            // Binding 0: Instance buffer
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mInstanceBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
            // Binding 1: Transform buffer
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mTransformBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
            // Binding 2: LOD levels buffer
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(2)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mLODLevelsBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
            // Binding 3: Mesh metadata buffer
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(3)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mMeshMetadataBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
            // Binding 4: Draw command buffer
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(4)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mDrawCommandBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
            // Binding 5: Visible instances buffer (placeholder)
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(5)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(
                    vk::DescriptorBufferInfo()
                        .setBuffer(
                            mInstanceBuffer->Get()) // Reuse as placeholder
                        .setOffset(0)
                        .setRange(VK_WHOLE_SIZE)),
            // Binding 6: Draw count buffer
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(6)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mDrawCountBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
            // Binding 7: Draw metadata buffer (materialId per draw)
            vk::WriteDescriptorSet()
                .setDstSet(mLODDescriptorSet)
                .setDstBinding(7)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mDrawMetadataBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE)),
        } };

        mDevice->Get().updateDescriptorSets(
            static_cast<std::uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);

        mLogger->LogInformation("Allocated and updated LOD descriptor set");
    }

    void LODService::createDrawMetaGraphicsDescriptor()
    {
        // Create a descriptor set layout for the graphics pipeline's set 2
        // This set holds the draw metadata buffer (materialId per draw),
        // indexed by gl_DrawIDARB in the vertex shader.
        const auto binding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex);

        const auto layoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(binding);

        mDrawMetaGraphicsLayout =
            mDevice->Get().createDescriptorSetLayout(layoutInfo);

        // Create a small descriptor pool for this single set
        const auto poolSize =
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1);

        const auto poolInfo =
            vk::DescriptorPoolCreateInfo()
                .setPoolSizeCount(1)
                .setPPoolSizes(&poolSize)
                .setMaxSets(1);

        mDrawMetaGraphicsPool =
            mDevice->Get().createDescriptorPool(poolInfo);

        // Allocate the descriptor set
        const auto allocInfo =
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mDrawMetaGraphicsPool)
                .setSetLayouts(mDrawMetaGraphicsLayout);

        const auto sets =
            mDevice->Get().allocateDescriptorSets(allocInfo);
        mDrawMetaDescriptorSet = sets[0];

        // Update with the draw metadata buffer
        const auto descriptorWrite =
            vk::WriteDescriptorSet()
                .setDstSet(mDrawMetaDescriptorSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setBufferInfo(vk::DescriptorBufferInfo()
                                   .setBuffer(mDrawMetadataBuffer->Get())
                                   .setOffset(0)
                                   .setRange(VK_WHOLE_SIZE));

        mDevice->Get().updateDescriptorSets(1, &descriptorWrite, 0, nullptr);

        mLogger->LogInformation("Created draw metadata descriptor for graphics set 2");
    }

    void LODService::createComputePipeline()
    {
        // Pipeline layout with push constants
        const vk::PushConstantRange pushConstantRange(
            vk::ShaderStageFlagBits::eCompute,
            0,
            sizeof(LODPushConstants));

        const auto pipelineLayoutInfo =
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(mDescriptorSetLayout)
                .setPushConstantRanges(pushConstantRange);

        mLODComputePipelineLayout =
            mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

        // Load and create compute shader module
        auto computeShaderModule =
            mServiceProvider->GetService<ShaderModuleBuilder>()
                ->SetFilePath("./Resources/Shaders/Compute/LODSelection.spv")
                .Build();

        const vk::PipelineShaderStageCreateInfo shaderStageInfo =
            vk::PipelineShaderStageCreateInfo()
                .setStage(vk::ShaderStageFlagBits::eCompute)
                .setModule(computeShaderModule->Get())
                .setPName("main");

        const vk::ComputePipelineCreateInfo pipelineInfo =
            vk::ComputePipelineCreateInfo()
                .setLayout(mLODComputePipelineLayout)
                .setStage(shaderStageInfo);

        const auto [result, pipeline] =
            mDevice->Get().createComputePipeline(nullptr, pipelineInfo);
        mLODComputePipeline = pipeline;

        mLogger->Assert(mLODComputePipeline,
                        "Failed to create LOD compute pipeline.");

        mDevice->Get().destroyShaderModule(computeShaderModule->Get());

        mLogger->LogInformation("Created LOD compute pipeline");
    }

    void LODService::createDitherTexture()
    {
        // Create the dither texture with Bayer matrix data
        // 8x8 matrix stored as R8 format
        constexpr std::array<std::uint8_t, 64> bayerMatrix = {
            { 0,  32, 8,  40, 2,  34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26,
              12, 44, 4,  36, 14, 46, 6,  38, 60, 28, 52, 20, 62, 30, 54, 22,
              3,  35, 11, 43, 1,  33, 9,  41, 51, 19, 59, 27, 49, 17, 57, 25,
              15, 47, 7,  39, 13, 45, 5,  37, 63, 31, 55, 23, 61, 29, 53, 21 }
        };

        mLogger->LogInformation("Created dither texture (8x8 Bayer matrix)");
    }

    std::uint32_t LODService::AddInstance(std::uint32_t groupId,
                                          std::uint32_t transformIndex,
                                          std::uint32_t materialId)
    {
        if (mInstanceCount >= mMaxInstances)
        {
            mLogger->LogError(
                "LODService::AddInstance: Max instances ({}) reached",
                mMaxInstances);
            return std::numeric_limits<std::uint32_t>::max();
        }

        const std::uint32_t instanceId = mInstanceCount++;

        // Store the flat-array offset for this group, NOT the group index.
        // The compute shader uses meshGroupId as the starting index into the
        // flat LOD level array (see LODSelection.comp selectLOD function).
        const std::uint32_t groupOffset = mLODPool->GetGroupOffset(groupId);

        LODInstanceData data;
        data.meshGroupId    = groupOffset;
        data.currentLOD     = 0;
        data.transformIndex = transformIndex;
        data.materialId     = materialId;

        mInstances[instanceId] = data;

        mLogger->LogTrace("Added instance {} (group={}, offset={}, transform={}, "
                          "material={})",
                          instanceId, groupId, groupOffset, transformIndex,
                          materialId);

        return instanceId;
    }

    void LODService::UpdateInstance(std::uint32_t instanceId,
                                    std::uint32_t transformIndex)
    {
        if (instanceId >= mInstanceCount)
        {
            return;
        }

        mInstances[instanceId].transformIndex = transformIndex;
    }

    void LODService::RemoveInstance(std::uint32_t instanceId)
    {
        if (instanceId >= mInstanceCount)
        {
            return;
        }

        // Swap with last and decrement count (order not preserved)
        const auto lastId = --mInstanceCount;
        if (instanceId != lastId)
        {
            mInstances[instanceId] = mInstances[lastId];
        }

        mInstances[lastId] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
    }

    void LODService::UpdateGPUData(vk::CommandBuffer cmd,
                                   const glm::vec3&  cameraPosition)
    {
        // Update push constants
        mPushConstants.cameraPosition = cameraPosition;
        mPushConstants.globalDrawDistance =
            mFreyaOptions->drawDistance * mFreyaOptions->drawDistance;
        mPushConstants.instanceCount = mInstanceCount;
        // Per-group level count — selectLOD loops within the current group,
        // not across all groups (that would cross into other groups' levels).
        {
            const auto totalLevels = mLODPool->GetAllLevels().size();
            const auto groupCount  = mLODPool->GetGroupCount();
            mPushConstants.lodLevelCount =
                (groupCount > 0) ? static_cast<std::uint32_t>(totalLevels / groupCount) : 0;
        }

        // Upload instance data to GPU
        if (mInstanceCount > 0)
        {
            mInstanceBuffer->Copy(mInstances.data(),
                                  sizeof(LODInstanceData) * mInstanceCount);
        }

        // Upload LOD levels data
        auto allLevels = mLODPool->GetAllLevels();
        if (!allLevels.empty())
        {
            mLODLevelsBuffer->Copy(allLevels.data(),
                                   sizeof(LODLevel) * allLevels.size());
        }

        // Reset atomic draw count
        std::uint32_t zero = 0;
        mDrawCountBuffer->Copy(&zero, sizeof(std::uint32_t));
    }

    void LODService::RecordDrawCommands(vk::CommandBuffer cmd)
    {
        // Bind compute pipeline
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, mLODComputePipeline);

        // Bind descriptor set at set 0 (compute pipeline only has 1 set layout)
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, mLODComputePipelineLayout,
            0, // LOD descriptor set at binding 0
            1, &mLODDescriptorSet, 0, nullptr);

        // Push constants
        cmd.pushConstants(mLODComputePipelineLayout,
                          vk::ShaderStageFlagBits::eCompute,
                          0,
                          sizeof(LODPushConstants),
                          &mPushConstants);

        // Dispatch compute shader to process instances
        const std::uint32_t dispatchCount = (mInstanceCount + 255) / 256;
        cmd.dispatch(dispatchCount, 1, 1);

        // Memory barrier to ensure compute shader writes are visible to draw indirect
        const vk::MemoryBarrier barrier(
            vk::AccessFlagBits::eShaderWrite,
            vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eVertexAttributeRead);

        cmd.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eVertexInput,
            vk::DependencyFlagBits::eByRegion,
            barrier,
            nullptr,
            nullptr);
    }

    void LODService::BindDescriptorSet(vk::CommandBuffer  cmd,
                                       vk::PipelineLayout layout)
    {
        // For graphics pipeline binding, we need to bind at a valid set index
        // The graphics pipeline layout has sets 0 (UBO), 1 (samplers), and we can add LOD at higher sets
        // But since we only have 1 set layout, we bind at set 0 (or need to modify pipeline layout)
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout,
                               0, // LOD descriptor set at binding 0
                               1, &mLODDescriptorSet, 0, nullptr);
    }

    void LODService::Dispatch(std::uint32_t frameIndex)
    {
        if (mInstanceCount == 0)
        {
            return;
        }

        // Get command buffer from the compute pool using round-robin
        mComputeCommandPool->SetCommandBufferIndex(frameIndex);
        auto& cmd = mComputeCommandPool->GetCommandBuffer();

        // Reset command buffer
        cmd.reset();

        // Begin command buffer
        const auto beginInfo =
            vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        cmd.begin(beginInfo);

        // Update GPU data (upload instance, LOD levels, reset draw count)
        UpdateGPUData(cmd, mPushConstants.cameraPosition);

        // Clear draw command buffer to avoid stale commands from previous frames.
        // Any draw command beyond the GPU-written count will have indexCount=0 (no-op).
        cmd.fillBuffer(mDrawCommandBuffer->Get(), 0, VK_WHOLE_SIZE, 0);

        // Barrier: transfer write (fillBuffer) → compute shader read
        const auto fillBarrier = vk::BufferMemoryBarrier()
                                     .setBuffer(mDrawCommandBuffer->Get())
                                     .setOffset(0)
                                     .setSize(VK_WHOLE_SIZE)
                                     .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                                     .setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eComputeShader,
                            vk::DependencyFlagBits::eByRegion, nullptr, fillBarrier,
                            nullptr);

        // Bind compute pipeline
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, mLODComputePipeline);

        // Bind descriptor set at set 0
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                mLODComputePipelineLayout, 0, 1, &mLODDescriptorSet,
                                0, nullptr);

        // Push constants
        cmd.pushConstants(mLODComputePipelineLayout,
                          vk::ShaderStageFlagBits::eCompute, 0,
                          sizeof(LODPushConstants), &mPushConstants);

        // Dispatch compute shader to process instances
        const std::uint32_t dispatchCount = (mInstanceCount + 255) / 256;
        cmd.dispatch(dispatchCount, 1, 1);

        // Memory barrier to ensure compute shader writes are visible to draw indirect
        const vk::MemoryBarrier barrier(vk::AccessFlagBits::eShaderWrite,
                                        vk::AccessFlagBits::eIndirectCommandRead |
                                            vk::AccessFlagBits::eVertexAttributeRead);

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                            vk::PipelineStageFlagBits::eDrawIndirect |
                                vk::PipelineStageFlagBits::eVertexInput,
                            vk::DependencyFlagBits::eByRegion, barrier, nullptr,
                            nullptr);

        // End command buffer
        cmd.end();

        // Submit to graphics queue (most GPUs support compute on graphics queue)
        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(&cmd);

        mDevice->GetGraphicsQueue().submit(submitInfo);
        mDevice->GetGraphicsQueue().waitIdle();
    }

    void LODService::RefreshMeshMetadata()
    {
        updateMeshMetadata();
    }

    std::uint32_t LODService::GetDrawCount() const
    {
        // Map the draw count buffer, invalidate the CPU cache (non-coherent
        // memory), read back the atomic counter written by the compute shader,
        // then unmap. Dispatch() precedes this call and waits for idle, so
        // the GPU has finished writing by the time we read here.
        void* data = mDevice->Get().mapMemory(
            mDrawCountBuffer->GetMemory(),
            0,
            sizeof(std::uint32_t),
            vk::MemoryMapFlagBits {});

        const auto invalidateRange = vk::MappedMemoryRange()
                                         .setMemory(mDrawCountBuffer->GetMemory())
                                         .setOffset(0)
                                         .setSize(VK_WHOLE_SIZE);
        static_cast<void>(
            mDevice->Get().invalidateMappedMemoryRanges(1, &invalidateRange));

        const std::uint32_t count = *static_cast<const std::uint32_t*>(data);
        mDevice->Get().unmapMemory(mDrawCountBuffer->GetMemory());
        return count;
    }

    void LODService::SetGlobalDrawDistance(float distance)
    {
        mPushConstants.globalDrawDistance = distance * distance;
    }

    void LODService::SetLODDistance(std::uint32_t groupId, std::uint32_t level,
                                    float distance)
    {
        mLogger->LogWarning(
            "LODService::SetLODDistance: Per-level distance modification "
            "not yet fully implemented");
    }

    void LODService::UpdateTransform(std::uint32_t    transformIndex,
                                     const glm::mat4& transform)
    {
        if (transformIndex >= mMaxInstances)
        {
            return;
        }

        mTransformBuffer->Copy(
            &transform, sizeof(glm::mat4), transformIndex * sizeof(glm::mat4));
    }

    void LODService::createComputeCommandPool()
    {
        // Create a dedicated command pool for compute work
        // The builder already pre-allocates command buffers based on frameCount
        mComputeCommandPool =
            mServiceProvider->GetService<CommandPoolBuilder>()
                ->SetCount(mFreyaOptions->frameCount)
                .Build();

        mLogger->LogInformation(
            "Created compute command pool with {} buffers",
            mFreyaOptions->frameCount);
    }

void LODService::updateMeshMetadata()
    {
        // Build mesh metadata array from MeshPool
        // This provides indexCount, firstIndex, vertexOffset for each mesh
        std::vector<MeshMetadata> meshMetadata(4096);

        mMeshPool->ForEachMesh([&meshMetadata](std::uint32_t meshId, const Mesh& mesh) {
            meshMetadata[meshId] = MeshMetadata {
                .indexCount   = mesh.indexCount,
                .firstIndex   = static_cast<std::uint32_t>(mesh.indexBufferOffset / sizeof(std::uint16_t)),
                // vertexOffset is in vertex-index units, matching VkDrawIndexedIndirectCommand.
                // Divide byte-offset by vertex stride to get the correct vertex index.
                .vertexOffset = static_cast<std::int32_t>(mesh.vertexBufferOffset / sizeof(Vertex)),
                .padding      = 0
            };
        });

        mMeshMetadataBuffer->Copy(meshMetadata.data(),
                                  sizeof(MeshMetadata) * meshMetadata.size());

        mLogger->LogInformation("Updated mesh metadata for {} meshes",
                                static_cast<std::uint32_t>(mMeshPool->GetMeshCount()));
    }

} // namespace FREYA_NAMESPACE