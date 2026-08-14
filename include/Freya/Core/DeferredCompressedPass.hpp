#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * Balanced G-buffer (160-bit color with velocity) + HDR Scene Color.
     *
     * Attachments: depth, albedo+matID, normal+flags, PBR, sceneColor HDR,
     * velocity RG16F.
     * Subpasses: depth pre-pass → G-buffer (emissive into scene color).
     * SSAO runs after geometry End(). Lighting is a separate render pass
     * (BeginLighting / EndLighting) with additive fullscreen shading.
     * TAA runs after lighting.
     */
    enum : std::uint32_t
    {
        DefDepthAttachment,
        DefAlbedoAttachment,
        DefNormalAttachment,
        DefPbrAttachment,
        DefSceneColorAttachment,
        DefVelocityAttachment,
    };

    enum : std::uint32_t
    {
        DefDepthPrePass,
        DefGBufferPass,
        DefLightingPass,
    };

    class DeferredCompressedPass
    {
      public:
        DeferredCompressedPass(
            const skr::Arc<Device>&       device,
            const skr::Arc<FreyaOptions>& freyaOptions,
            const skr::Arc<Surface>&      surface,
            const vk::RenderPass          renderPass,
            const vk::PipelineLayout      vertexPipelineLayout,
            const vk::PipelineLayout      fullscreenPipelineLayout,
            const vk::Pipeline            depthPrepassPipeline,
            const vk::Pipeline            gbufferPipeline,
            const vk::Pipeline            lightingPipeline,
            const skr::Arc<Buffer>&       uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>&  descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&        descriptorSets,
            const vk::DescriptorPool                     descriptorPool,
            const std::vector<skr::Arc<Image>>&          gbufferImages,
            const skr::Arc<Image>&                       sceneColorImage,
            const skr::Arc<Image>&                       velocityImage,
            const skr::Arc<Image>&                       depthImage,
            const std::vector<vk::Framebuffer>&          framebuffers,
            const vk::RenderPass                         lightingRenderPass,
            const vk::Framebuffer                        lightingFramebuffer,
            const vk::DescriptorSetLayout                lightingSetLayout,
            const vk::DescriptorPool                     lightingDescriptorPool,
            const std::vector<vk::DescriptorSet>&        lightingSets,
            const skr::Arc<MaterialDescriptorResources>& materialResources,
            const skr::Arc<BoneMatrixResources>&         boneResources,
            const vk::Sampler                            gbufferSampler,
            vk::Extent2D                                 extent);

        ~DeferredCompressedPass();

        vk::RenderPass& GetRenderPass() { return mRenderPass; }

        vk::PipelineLayout& GetVertexPipelineLayout()
        {
            return mVertexPipelineLayout;
        }

        vk::PipelineLayout& GetFullscreenPipelineLayout()
        {
            return mFullscreenPipelineLayout;
        }

        vk::Pipeline& GetPipeline(std::uint32_t subpass);

        /// HDR scene color (emissive + lighting). Used as composite “opaque”.
        skr::Arc<Image> GetOpaqueImage() const { return mSceneColorImage; }
        skr::Arc<Image> GetSceneColorImage() const { return mSceneColorImage; }
        skr::Arc<Image> GetDepthImage() const { return mDepthImage; }
        skr::Arc<Image> GetVelocityImage() const { return mVelocityImage; }
        skr::Arc<Image> GetAlbedoImage() const { return mGBufferImages[0]; }
        skr::Arc<Image> GetNormalImage() const { return mGBufferImages[1]; }
        skr::Arc<Image> GetPbrImage() const { return mGBufferImages[2]; }

        void Begin(const skr::Arc<SwapChain>    swapChain,
                   const skr::Arc<CommandPool>& commandPool) const;

        void NextSubpass(const skr::Arc<CommandPool>& commandPool) const;

        void BindPipeline(std::uint32_t                subpass,
                          const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex) const;

        void AdvanceSubpass(std::uint32_t                subpass,
                            const skr::Arc<CommandPool>& commandPool,
                            std::uint32_t                frameIndex) const;

        void DrawLighting(const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex,
                          std::uint32_t lightingDebug = 0) const;

        void BeginLighting(const skr::Arc<CommandPool>& commandPool,
                           const skr::Arc<Image>&       ssaoImage,
                           std::uint32_t                frameIndex) const;

        void EndLighting(const skr::Arc<CommandPool>& commandPool) const;

        void End(const skr::Arc<CommandPool> commandPool) const;

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        vk::DescriptorSet& GetDescriptorSet(std::uint32_t frameIndex)
        {
            return mDescriptorSets[frameIndex];
        }

        vk::DescriptorSetLayout& GetSamplerLayout()
        {
            return mMaterialResources->GetSamplerLayout();
        }

        vk::DescriptorPool& GetSamplerDescriptorPool()
        {
            return mMaterialResources->GetSamplerDescriptorPool();
        }

        skr::Arc<Buffer> GetUniformBuffer() { return mUniformBuffer; }

        std::size_t GetFramebufferCount() const { return mFramebuffers.size(); }

        vk::Framebuffer& GetFramebuffer(std::size_t index)
        {
            return mFramebuffers[index];
        }

        std::uint32_t GetCurrentSubpass() const { return mCurrentSubpass; }

        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
        skr::Arc<Surface>      mSurface;

        vk::RenderPass mRenderPass;
        vk::RenderPass mLightingRenderPass;

      private:
        vk::PipelineLayout mVertexPipelineLayout;
        vk::PipelineLayout mFullscreenPipelineLayout;

        std::array<vk::Pipeline, 3> mPipelines;

        skr::Arc<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

        std::vector<skr::Arc<Image>> mGBufferImages;
        skr::Arc<Image>              mSceneColorImage;
        skr::Arc<Image>              mVelocityImage;
        skr::Arc<Image>              mDepthImage;

        std::vector<vk::Framebuffer> mFramebuffers;
        vk::Framebuffer              mLightingFramebuffer;

        vk::Extent2D mExtent;

        vk::DescriptorSetLayout        mLightingSetLayout;
        vk::DescriptorPool             mLightingDescriptorPool;
        std::vector<vk::DescriptorSet> mLightingSets;

        skr::Arc<MaterialDescriptorResources> mMaterialResources;
        skr::Arc<BoneMatrixResources>         mBoneResources;
        vk::Sampler                           mGbufferSampler;

        mutable bool                       mLabelActive    = false;
        mutable bool                       mLightingActive = false;
        mutable std::uint32_t              mCurrentSubpass = DefDepthPrePass;
        mutable std::vector<vk::ImageView> mBoundSsaoViews;

        static const char* GetSubpassLabel(std::uint32_t subpass);
        static DebugRegion GetSubpassRegion(std::uint32_t subpass);
    };

} // namespace FREYA_NAMESPACE
