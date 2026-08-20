#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/FullscreenEffect.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/RenderFrameContext.hpp"

#include <array>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    struct FullscreenEffect::Impl
    {
        skr::Arc<Device>               device;
        skr::Arc<FreyaOptions>         options;
        skr::Arc<skr::ServiceProvider> serviceProvider;
        std::string                    name;
        std::string                    fragmentRelative;
        std::string                    vertexRelative;
        std::vector<EffectInput>       inputs;
        std::uint32_t                  pushConstantSize = 0;
        std::vector<std::byte>         pushData;
        bool                           enabled = true;
        std::array<std::uint32_t, 32>  materialBits {};
        bool                           maskDirty = true;
        skr::Arc<Buffer>               maskBuffer;
        vk::RenderPass                 renderPass {};
        vk::PipelineLayout             pipelineLayout {};
        vk::Pipeline                   pipeline {};
        vk::DescriptorSetLayout        setLayout {};
        vk::DescriptorSetLayout        maskSetLayout {};
        vk::DescriptorPool             descriptorPool {};
        std::vector<vk::DescriptorSet> descriptorSets;
        std::vector<vk::DescriptorSet> maskDescriptorSets;
        vk::Sampler                    sampler {};
        skr::Arc<Image>                output;
        vk::Framebuffer                framebuffer {};
        vk::Extent2D                   extent {};

        void            uploadMaterialMask();
        void            destroyGpu();
        skr::Arc<Image> resolveHdr(const RenderFrameContext& ctx) const;
        skr::Arc<Image> resolveInput(EffectInput               input,
                                     const RenderFrameContext& ctx,
                                     const skr::Arc<Image>&    hdr) const;
        vk::ImageLayout inputLayout(EffectInput input) const;
    };
} // namespace FREYA_NAMESPACE
