#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Weighted Blended OIT: MDI accumulate + fullscreen resolve.
     *
     * Owns per-frame oitAccum (RGBA16F), oitReveal (R8), and
     * sceneWithTranslucency (HDR) so frames in flight do not race. Accumulate
     * uses deferred depth (test on / write off), LightService analytical
     * lighting (set 2), bone SSBO (set 3), and glass env (set 4: opaque HDR
     * + IBL) for transmission / refraction.
     */
    class TranslucentPass
    {
      public:
        TranslucentPass(
            const skr::Arc<Device>&                      device,
            const skr::Arc<FreyaOptions>&                freyaOptions,
            const skr::Arc<MaterialDescriptorResources>& materialResources,
            const skr::Arc<BoneMatrixResources>&         boneResources,
            const skr::Arc<LightService>&                lightService,
            const skr::Arc<IBLService>&                  iblService,
            vk::RenderPass                               accumulateRenderPass,
            vk::RenderPass                               resolveRenderPass,
            vk::PipelineLayout                           accumulateLayout,
            vk::PipelineLayout                           resolveLayout,
            vk::Pipeline                                 accumulatePipeline,
            vk::Pipeline                                 resolvePipeline,
            const skr::Arc<Buffer>&                      uniformBuffer,
            vk::DescriptorSetLayout                      cameraSetLayout,
            vk::DescriptorPool                           cameraDescriptorPool,
            const std::vector<vk::DescriptorSet>&        cameraSets,
            vk::DescriptorSetLayout                      glassSetLayout,
            vk::DescriptorPool                           glassDescriptorPool,
            const std::vector<vk::DescriptorSet>&        glassSets,
            vk::DescriptorSetLayout                      resolveSetLayout,
            vk::DescriptorPool                           resolveDescriptorPool,
            const std::vector<vk::DescriptorSet>&        resolveSets,
            std::vector<skr::Arc<Image>>
                oitAccum,
            std::vector<skr::Arc<Image>>
                oitReveal,
            std::vector<skr::Arc<Image>>
                sceneWithTranslucency,
            std::vector<vk::Framebuffer>
                accumulateFramebuffers,
            std::vector<vk::Framebuffer>
                         resolveFramebuffers,
            vk::Sampler  sampler,
            vk::Format   depthFormat,
            vk::Extent2D extent);

        ~TranslucentPass();

        [[nodiscard]] vk::PipelineLayout GetAccumulatePipelineLayout() const
        {
            return mAccumulateLayout;
        }

        [[nodiscard]] skr::Arc<Image> GetSceneWithTranslucency(
            std::uint32_t frameIndex) const
        {
            if (frameIndex >= mSceneWithTranslucency.size())
                return {};
            return mSceneWithTranslucency[frameIndex];
        }

        void UpdateProjection(const ProjectionUniformBuffer& buffer,
                              std::uint32_t                  frameIndex) const;

        void BeginAccumulate(const skr::Arc<CommandPool>& commandPool,
                             const skr::Arc<Image>&       opaqueImage,
                             std::uint32_t                frameIndex) const;

        void EndAccumulate(const skr::Arc<CommandPool>& commandPool) const;

        void Resolve(const skr::Arc<CommandPool>& commandPool,
                     const skr::Arc<Image>&       opaqueImage,
                     std::uint32_t                frameIndex) const;

      private:
        skr::Arc<Device>                      mDevice;
        skr::Arc<FreyaOptions>                mFreyaOptions;
        skr::Arc<MaterialDescriptorResources> mMaterialResources;
        skr::Arc<BoneMatrixResources>         mBoneResources;
        skr::Arc<LightService>                mLightService;
        skr::Arc<IBLService>                  mIblService;

        vk::RenderPass     mAccumulateRenderPass;
        vk::RenderPass     mResolveRenderPass;
        vk::PipelineLayout mAccumulateLayout;
        vk::PipelineLayout mResolveLayout;
        vk::Pipeline       mAccumulatePipeline;
        vk::Pipeline       mResolvePipeline;

        skr::Arc<Buffer>               mUniformBuffer;
        vk::DescriptorSetLayout        mCameraSetLayout;
        vk::DescriptorPool             mCameraDescriptorPool;
        std::vector<vk::DescriptorSet> mCameraSets;

        vk::DescriptorSetLayout        mGlassSetLayout;
        vk::DescriptorPool             mGlassDescriptorPool;
        std::vector<vk::DescriptorSet> mGlassSets;

        vk::DescriptorSetLayout        mResolveSetLayout;
        vk::DescriptorPool             mResolveDescriptorPool;
        std::vector<vk::DescriptorSet> mResolveSets;

        std::vector<skr::Arc<Image>> mOitAccum;
        std::vector<skr::Arc<Image>> mOitReveal;
        std::vector<skr::Arc<Image>> mSceneWithTranslucency;

        std::vector<vk::Framebuffer> mAccumulateFramebuffers;
        std::vector<vk::Framebuffer> mResolveFramebuffers;
        vk::Sampler                  mSampler;
        vk::Format                   mDepthFormat {};
        vk::Extent2D                 mExtent {};

        mutable std::vector<vk::ImageView> mBoundOpaqueViews;
        mutable std::vector<vk::ImageView> mBoundGlassOpaqueViews;
    };

} // namespace FREYA_NAMESPACE
