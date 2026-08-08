#include "TaaPassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

namespace FREYA_NAMESPACE
{
    TaaPassBuilder::TaaPassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<TaaPass> TaaPassBuilder::Build(const skr::Arc<SwapChain>&,
                                            vk::Extent2D extent)
    {
        if (extent.width == 0 || extent.height == 0)
            extent = mSurface->QueryExtent();

        auto shader = mServiceProvider->GetService<ShaderModuleBuilder>()
                          ->SetFilePath(mFreyaOptions->shaderRoot +
                                        "/DeferredCompressed/taa.comp.spv")
                          .Build();

        auto createHistory = [&]() {
            return mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::TaaHistory)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();
        };
        auto createDepthHistory = [&]() {
            return mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::TaaDepthHistory)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();
        };

        std::array historyImages      = { createHistory(), createHistory() };
        std::array depthHistoryImages = { createDepthHistory(),
                                          createDepthHistory() };

        auto makeSampler = [&](vk::Filter filter) {
            return mDevice->Get().createSampler(
                vk::SamplerCreateInfo()
                    .setMagFilter(filter)
                    .setMinFilter(filter)
                    .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                    .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                    .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                    .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));
        };

        auto colorSampler   = makeSampler(vk::Filter::eLinear);
        auto nearestSampler = makeSampler(vk::Filter::eNearest);

        auto bindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(2)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(3)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(4)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(5)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
            vk::DescriptorSetLayoutBinding()
                .setBinding(6)
                .setDescriptorType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eCompute),
        };

        auto setLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));

        // 2 sets × (5 CIS + 2 storage)
        auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(10),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(4),
        };
        auto pool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo().setPoolSizes(poolSizes).setMaxSets(
                2));

        auto layouts = std::vector { setLayout, setLayout };
        auto sets    = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(pool)
                .setSetLayouts(layouts));

        std::array<vk::DescriptorSet, 2> descriptorSets = { sets[0], sets[1] };

        auto pushRange = vk::PushConstantRange()
                             .setStageFlags(vk::ShaderStageFlagBits::eCompute)
                             .setOffset(0)
                             .setSize(sizeof(float) * 12);

        auto pipelineLayout = mDevice->Get().createPipelineLayout(
            vk::PipelineLayoutCreateInfo()
                .setSetLayouts(setLayout)
                .setPushConstantRanges(pushRange));

        auto stage = vk::PipelineShaderStageCreateInfo()
                         .setStage(vk::ShaderStageFlagBits::eCompute)
                         .setModule(shader->Get())
                         .setPName("main");

        auto pipeline =
            mDevice->Get()
                .createComputePipeline(
                    nullptr,
                    vk::ComputePipelineCreateInfo().setStage(stage).setLayout(
                        pipelineLayout))
                .value;

        mDevice->Get().destroyShaderModule(shader->Get());

        return skr::MakeArc<TaaPass>(
            mDevice, mFreyaOptions, pipelineLayout, pipeline, setLayout, pool,
            descriptorSets, historyImages, depthHistoryImages, colorSampler,
            nearestSampler, extent);
    }

} // namespace FREYA_NAMESPACE
