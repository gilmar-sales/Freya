#include "TaaPassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/ShaderModule.hpp"

#include <algorithm>

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

        const auto frameCount = std::max(1u, mFreyaOptions->frameCount);

        // One ping-pong pair per flight slot: frames overlap on the GPU,
        // so slots must not share history images.
        auto historyImages = std::vector<std::array<skr::Arc<Image>, 2>> {};
        auto depthHistoryImages =
            std::vector<std::array<skr::Arc<Image>, 2>> {};
        historyImages.reserve(frameCount);
        depthHistoryImages.reserve(frameCount);
        for (std::uint32_t i = 0; i < frameCount; ++i)
        {
            historyImages.push_back({ createHistory(), createHistory() });
            depthHistoryImages.push_back(
                { createDepthHistory(), createDepthHistory() });
        }

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

        // frameCount × (2 sets × (5 CIS + 2 storage))
        auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(10 * frameCount),
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageImage)
                .setDescriptorCount(4 * frameCount),
        };
        auto pool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo().setPoolSizes(poolSizes).setMaxSets(
                2 * frameCount));

        auto layouts =
            std::vector<vk::DescriptorSetLayout>(2 * frameCount, setLayout);
        auto sets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(pool)
                .setSetLayouts(layouts));

        auto descriptorSets = std::vector<std::array<vk::DescriptorSet, 2>> {};
        descriptorSets.reserve(frameCount);
        for (std::uint32_t i = 0; i < frameCount; ++i)
            descriptorSets.push_back({ sets[2 * i], sets[2 * i + 1] });

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
            nearestSampler, extent, frameCount);
    }

} // namespace FREYA_NAMESPACE
