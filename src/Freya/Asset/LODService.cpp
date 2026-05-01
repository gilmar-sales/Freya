#include "Freya/Asset/LODService.hpp"
#include "Freya/Asset/LODPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/Renderer.hpp"

namespace FREYA_NAMESPACE
{

LODService::LODService(const Ref<Device>&               device,
                         const Ref<PhysicalDevice>&        physicalDevice,
                         const Ref<CommandPool>&           commandPool,
                         const Ref<LODPool>&               lodPool,
                         const Ref<MeshPool>&              meshPool,
                         const Ref<Renderer>&              renderer,
                         const Ref<FreyaOptions>&         freyaOptions,
                         const Ref<skr::ServiceProvider>& serviceProvider) :
    mDevice(device), mPhysicalDevice(physicalDevice),
    mCommandPool(commandPool), mLODPool(lodPool), mMeshPool(meshPool),
    mRenderer(renderer), mFreyaOptions(freyaOptions),
    mServiceProvider(serviceProvider)
{
    mLogger = serviceProvider->GetService<skr::Logger<LODService>>();

    mLogger->LogInformation(
        "Initializing LOD service with {} max instances", mMaxInstances);

    // Initialize push constants
    mPushConstants.cameraPosition      = glm::vec3(0.0f);
    mPushConstants.globalDrawDistance  = freyaOptions->drawDistance;
    mPushConstants.instanceCount       = 0;
    mPushConstants.lodLevelCount       = lodPool->GetAllLevels().size();

    // Pre-allocate instance data array
    mInstances.resize(mMaxInstances);
    for (auto& instance : mInstances)
    {
        instance = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };
    }

    createGPUBuffers();
    createDescriptorSetLayout();
    createComputePipeline();

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
}

void LODService::createGPUBuffers()
{
    // Instance buffer - holds LODInstanceData for each instance
    const auto instanceBufferSize = sizeof(LODInstanceData) * mMaxInstances;
    mInstanceBuffer =
        BufferBuilder(mDevice)
            .SetSize(instanceBufferSize)
            .SetUsage(BufferUsage::Instance)
            .Build();

    // Draw command buffer - holds DrawIndexedIndirectCommand
    const auto drawCmdSize =
        sizeof(DrawIndexedIndirectCommand) * mMaxInstances;
    mDrawCommandBuffer =
        BufferBuilder(mDevice)
            .SetSize(drawCmdSize)
            .SetUsage(BufferUsage::Instance)
            .Build();

    // Draw count buffer - atomic counter
    mDrawCountBuffer =
        BufferBuilder(mDevice)
            .SetSize(sizeof(std::uint32_t))
            .SetUsage(BufferUsage::Instance)
            .Build();

    mLogger->LogInformation(
        "Created GPU buffers: instance={} bytes, drawCmd={} bytes, "
        "drawCount={} bytes",
        instanceBufferSize, drawCmdSize, sizeof(std::uint32_t));
}

void LODService::createDescriptorSetLayout()
{
    // Binding layout for LOD descriptor set (set = 2)
    const std::array<vk::DescriptorSetLayoutBinding, 7> bindings = { {
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
    } };

    const auto layoutInfo = vk::DescriptorSetLayoutCreateInfo()
        .setBindings(bindings);

    mDescriptorSetLayout = mDevice->Get().createDescriptorSetLayout(layoutInfo);

    mLogger->LogInformation("Created LOD descriptor set layout");
}

void LODService::createComputePipeline()
{
    // Pipeline layout with push constants
    const vk::PushConstantRange pushConstantRange(
        vk::ShaderStageFlagBits::eCompute,
        0,
        sizeof(LODPushConstants));

    const auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
        .setSetLayouts(mDescriptorSetLayout)
        .setPushConstantRanges(pushConstantRange);

    mLODComputePipelineLayout = mDevice->Get().createPipelineLayout(pipelineLayoutInfo);

    // TODO: Create actual pipeline by loading SPIR-V shader
    mLogger->LogInformation("Created LOD compute pipeline layout");
}

std::uint32_t LODService::AddInstance(std::uint32_t groupId,
                                      std::uint32_t transformIndex)
{
    if (mInstanceCount >= mMaxInstances)
    {
        mLogger->LogError(
            "LODService::AddInstance: Max instances ({}) reached",
            mMaxInstances);
        return std::numeric_limits<std::uint32_t>::max();
    }

    const std::uint32_t instanceId = mInstanceCount++;

    LODInstanceData data;
    data.meshGroupId    = groupId;
    data.currentLOD     = 0;
    data.transformIndex = transformIndex;
    data.padding        = 0;

    mInstances[instanceId] = data;

    mLogger->LogTrace("Added instance {} (group={}, transform={})",
                      instanceId, groupId, transformIndex);

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

    mInstances[lastId] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0 };
}

void LODService::UpdateGPUData(vk::CommandBuffer cmd,
                               const glm::vec3&  cameraPosition)
{
    // Update push constants
    mPushConstants.cameraPosition = cameraPosition;
    mPushConstants.globalDrawDistance =
        mFreyaOptions->drawDistance * mFreyaOptions->drawDistance;
    mPushConstants.instanceCount = mInstanceCount;
    mPushConstants.lodLevelCount =
        static_cast<std::uint32_t>(mLODPool->GetAllLevels().size());

    // Upload instance data to GPU
    if (mInstanceCount > 0)
    {
        mInstanceBuffer->Copy(mInstances.data(),
                              sizeof(LODInstanceData) * mInstanceCount);
    }

    // Reset atomic draw count
    std::uint32_t zero = 0;
    mDrawCountBuffer->Copy(&zero, sizeof(std::uint32_t));
}

void LODService::RecordDrawCommands(vk::CommandBuffer cmd)
{
    // Bind compute pipeline
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, mLODComputePipeline);

    // Bind descriptor set
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, mLODComputePipelineLayout,
        2, // LOD descriptor set (set = 2)
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

    // Memory barrier
    const vk::MemoryBarrier barrier(
        vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead,
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndexRead);

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eDrawIndirect |
            vk::PipelineStageFlagBits::eVertexInput,
        vk::DependencyFlagBits::eByRegion,
        barrier,
        nullptr,
        nullptr);
}

void LODService::BindDescriptorSet(vk::CommandBuffer  cmd,
                                   vk::PipelineLayout layout)
{
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout,
                           2, // LOD descriptor set
                           1, &mLODDescriptorSet, 0, nullptr);
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

} // namespace FREYA_NAMESPACE