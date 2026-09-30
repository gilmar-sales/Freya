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

#include <vector>

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

    /**
     * @brief One full G-buffer image set for a single flight slot.
     *
     * Every frame in flight renders into its own slot so concurrently
     * executing command buffers never share attachments.
     */
    struct GBufferSlotImages
    {
        skr::Arc<Image> albedo;
        skr::Arc<Image> normal;
        skr::Arc<Image> pbr;
        skr::Arc<Image> sceneColor;
        skr::Arc<Image> velocity;
        skr::Arc<Image> depth;
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
            std::vector<vk::Pipeline>
                                                         gbufferTechniques,
            const vk::Pipeline                           lightingPipeline,
            const skr::Arc<Buffer>&                      uniformBuffer,
            const std::vector<vk::DescriptorSetLayout>&  descriptorSetLayouts,
            const std::vector<vk::DescriptorSet>&        descriptorSets,
            const vk::DescriptorPool                     descriptorPool,
            const std::vector<GBufferSlotImages>&        slotImages,
            const std::vector<vk::Framebuffer>&          framebuffers,
            const vk::RenderPass                         lightingRenderPass,
            const std::vector<vk::Framebuffer>&          lightingFramebuffers,
             const vk::DescriptorSetLayout                lightingSetLayout,
             const vk::DescriptorPool                     lightingDescriptorPool,
             const std::vector<vk::DescriptorSet>&        lightingSets,
             const vk::Pipeline                           tileCullingPipeline,
             const vk::PipelineLayout                     tileCullingPipelineLayout,
             const vk::DescriptorSetLayout                tileCullingSetLayout,
             const vk::DescriptorPool                     tileCullingDescriptorPool,
             const std::vector<vk::DescriptorSet>&        tileCullingSets,
             const std::vector<vk::DescriptorSet>&        lightSets,
             const std::vector<skr::Arc<Buffer>>&         tileBuffers,
             vk::Extent2D                                 tileCounts,
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
        skr::Arc<Image> GetOpaqueImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).sceneColor;
        }
        skr::Arc<Image> GetSceneColorImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).sceneColor;
        }
        skr::Arc<Image> GetDepthImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).depth;
        }
        skr::Arc<Image> GetVelocityImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).velocity;
        }
        skr::Arc<Image> GetAlbedoImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).albedo;
        }
        skr::Arc<Image> GetNormalImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).normal;
        }
        skr::Arc<Image> GetPbrImage(std::uint32_t frameIndex) const
        {
            return Slot(frameIndex).pbr;
        }

        void Begin(const skr::Arc<SwapChain>    swapChain,
                   const skr::Arc<CommandPool>& commandPool) const;

        void NextSubpass(const skr::Arc<CommandPool>& commandPool) const;

        void BindPipeline(std::uint32_t                subpass,
                          const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex) const;

        void BindGBufferTechnique(std::uint32_t                techniqueId,
                                  const skr::Arc<CommandPool>& commandPool,
                                  std::uint32_t frameIndex) const;

        void AdvanceSubpass(std::uint32_t                subpass,
                            const skr::Arc<CommandPool>& commandPool,
                            std::uint32_t                frameIndex) const;

        void DrawLighting(const skr::Arc<CommandPool>& commandPool,
                          std::uint32_t                frameIndex,
                          std::uint32_t                lightingDebug = 0) const;

        void BeginLighting(const skr::Arc<CommandPool>& commandPool,
                           const skr::Arc<Image>&       ssaoImage,
                           const skr::Arc<Image>&       shadowMaskImage,
                           std::uint32_t                frameIndex) const;

        void EndLighting(const skr::Arc<CommandPool>& commandPool) const;

        void DispatchLightCulling(
            const skr::Arc<CommandPool>& commandPool,
            std::uint32_t                frameIndex) const;

        void End(const skr::Arc<CommandPool> commandPool) const;

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        vk::DescriptorSet& GetDescriptorSet(std::uint32_t frameIndex)
        {
            return mDescriptorSets[frameIndex];
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
        std::vector<vk::Pipeline>   mGBufferTechniques;

        skr::Arc<Buffer> mUniformBuffer;

        std::vector<vk::DescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<vk::DescriptorSet>       mDescriptorSets;
        vk::DescriptorPool                   mDescriptorPool;

        std::vector<GBufferSlotImages> mSlotImages;

        std::vector<vk::Framebuffer> mFramebuffers;
        std::vector<vk::Framebuffer> mLightingFramebuffers;

        const GBufferSlotImages& Slot(std::uint32_t frameIndex) const
        {
            static const GBufferSlotImages kEmpty {};
            if (mSlotImages.empty())
                return kEmpty;
            return mSlotImages[frameIndex % mSlotImages.size()];
        }

        vk::Extent2D mExtent;

        vk::DescriptorSetLayout        mLightingSetLayout;
        vk::DescriptorPool             mLightingDescriptorPool;
        std::vector<vk::DescriptorSet> mLightingSets;

        vk::Pipeline            mTileCullingPipeline;
        vk::PipelineLayout      mTileCullingPipelineLayout;
        vk::DescriptorSetLayout mTileCullingSetLayout;
        vk::DescriptorPool      mTileCullingDescriptorPool;
        std::vector<vk::DescriptorSet> mTileCullingSets;
        std::vector<vk::DescriptorSet> mLightSets;
        std::vector<skr::Arc<Buffer>>  mTileBuffers;
        vk::Extent2D                    mTileCounts;

        skr::Arc<MaterialDescriptorResources> mMaterialResources;
        skr::Arc<BoneMatrixResources>         mBoneResources;
        vk::Sampler                           mGbufferSampler;

        mutable bool                       mLabelActive    = false;
        mutable bool                       mLightingActive = false;
        mutable bool                       mGeometryDescriptorsBound = false;
        mutable std::uint32_t              mCurrentSubpass = DefDepthPrePass;
        mutable std::vector<vk::ImageView> mBoundSsaoViews;
        mutable std::vector<vk::ImageView> mBoundShadowMaskViews;

        static const char* GetSubpassLabel(std::uint32_t subpass);
        static DebugRegion GetSubpassRegion(std::uint32_t subpass);

        void BindGeometryDescriptors(
            const skr::Arc<CommandPool>& commandPool,
            std::uint32_t                  frameIndex) const;
    };

} // namespace FREYA_NAMESPACE
