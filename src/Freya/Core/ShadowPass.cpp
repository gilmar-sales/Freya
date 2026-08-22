#include "Freya/Core/ShadowPass.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace
{
    constexpr float kMaxDirectionalShadowDistance = 160.0f;
    constexpr float kCascadeSplitLambda           = 0.55f;
    constexpr float kCascadeZPad                  = 25.0f;
    constexpr float kCascadePullEps               = 1.0f;
    constexpr float kCascadeXyPadFrac             = 0.25f;

    struct CameraFrustumParams
    {
        float tanHalfFovY;
        float aspect;
    };

    CameraFrustumParams frustumParamsFromProjection(const glm::mat4& cameraProj)
    {
        return {
            1.0f / std::abs(cameraProj[1][1]),
            (cameraProj[1][1] / cameraProj[0][0]) * -1.0f,
        };
    }

    std::array<float, fra::MAX_SHADOW_CASCADES> computePracticalSplits(
        const std::uint32_t cascadeCount,
        const float         nearPlane,
        const float         cascadeFar)
    {
        std::array<float, fra::MAX_SHADOW_CASCADES> splits {};
        for (std::uint32_t i = 1; i <= cascadeCount; ++i)
        {
            const auto p = static_cast<float>(i) /
                           static_cast<float>(cascadeCount);
            const auto logSplit =
                nearPlane * std::pow(cascadeFar / nearPlane, p);
            const auto uniformSplit =
                nearPlane + (cascadeFar - nearPlane) * p;
            splits[i - 1] = kCascadeSplitLambda * logSplit +
                            (1.0f - kCascadeSplitLambda) * uniformSplit;
        }
        return splits;
    }

    std::array<glm::vec3, 8> worldFrustumSliceCorners(
        const glm::mat4& invView,
        const float      tanHalfFovY,
        const float      aspect,
        const float      splitNear,
        const float      splitFar)
    {
        const auto hNear = tanHalfFovY * splitNear;
        const auto wNear = hNear * aspect;
        const auto hFar  = tanHalfFovY * splitFar;
        const auto wFar  = hFar * aspect;

        const std::array<glm::vec4, 8> viewCorners = {
            glm::vec4(-wNear, hNear, -splitNear, 1.0f),
            glm::vec4(wNear, hNear, -splitNear, 1.0f),
            glm::vec4(-wNear, -hNear, -splitNear, 1.0f),
            glm::vec4(wNear, -hNear, -splitNear, 1.0f),
            glm::vec4(-wFar, hFar, -splitFar, 1.0f),
            glm::vec4(wFar, hFar, -splitFar, 1.0f),
            glm::vec4(-wFar, -hFar, -splitFar, 1.0f),
            glm::vec4(wFar, -hFar, -splitFar, 1.0f),
        };

        std::array<glm::vec3, 8> worldCorners {};
        for (std::size_t c = 0; c < worldCorners.size(); ++c)
            worldCorners[c] = glm::vec3(invView * viewCorners[c]);
        return worldCorners;
    }

    float boundingSphereRadius(const std::array<glm::vec3, 8>& worldCorners,
                               glm::vec3&                      outCenter)
    {
        outCenter = glm::vec3(0.0f);
        for (const auto& corner : worldCorners)
            outCenter += corner;
        outCenter /= 8.0f;

        float radius = 0.0f;
        for (const auto& corner : worldCorners)
            radius = std::max(radius, glm::length(corner - outCenter));
        return std::ceil(radius * 16.0f) / 16.0f;
    }

    glm::mat4 stabilizedLightView(const glm::vec3& center,
                                  const glm::vec3& lightDir,
                                  const glm::vec3& up,
                                  const float      radius,
                                  const float      resolution)
    {
        const float pullBack = radius + kCascadeZPad + kCascadePullEps;
        const float texelSize = (2.0f * radius) / resolution;

        auto lightView =
            glm::lookAt(center - lightDir * pullBack, center, up);
        auto centerLS = glm::vec3(lightView * glm::vec4(center, 1.0f));
        if (texelSize > 1e-6f)
        {
            centerLS.x = std::floor(centerLS.x / texelSize) * texelSize;
            centerLS.y = std::floor(centerLS.y / texelSize) * texelSize;
        }
        const auto snappedCenter =
            glm::vec3(glm::inverse(lightView) * glm::vec4(centerLS, 1.0f));
        return glm::lookAt(snappedCenter - lightDir * pullBack, snappedCenter,
                           up);
    }

    struct LightOrthoBounds
    {
        glm::vec3 minB;
        glm::vec3 maxB;
        float     extentX;
        float     extentY;
    };

    LightOrthoBounds lightSpaceBoundsForSlice(
        const glm::mat4&                lightView,
        const std::array<glm::vec3, 8>& worldCorners)
    {
        auto minB = glm::vec3(std::numeric_limits<float>::max());
        auto maxB = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& worldCorner : worldCorners)
        {
            const auto lightSpace =
                glm::vec3(lightView * glm::vec4(worldCorner, 1.0f));
            minB = glm::min(minB, lightSpace);
            maxB = glm::max(maxB, lightSpace);
        }

        const auto extentX = std::max(maxB.x - minB.x, 1e-3f);
        const auto extentY = std::max(maxB.y - minB.y, 1e-3f);
        minB.x -= extentX * kCascadeXyPadFrac;
        maxB.x += extentX * kCascadeXyPadFrac;
        minB.y -= extentY * kCascadeXyPadFrac;
        maxB.y += extentY * kCascadeXyPadFrac;
        minB.z -= kCascadeZPad;
        maxB.z += kCascadeZPad;

        return { minB, maxB, extentX, extentY };
    }

    glm::mat4 lightOrthoFromBounds(const LightOrthoBounds& bounds,
                                   const bool              reverseZ)
    {
        const auto near = std::max(1e-3f, -bounds.maxB.z);
        const auto far  = std::max(near + 1e-3f, -bounds.minB.z);
        return reverseZ
                   ? glm::ortho(bounds.minB.x, bounds.maxB.x, bounds.minB.y,
                                bounds.maxB.y, far, near)
                   : glm::ortho(bounds.minB.x, bounds.maxB.x, bounds.minB.y,
                                bounds.maxB.y, near, far);
    }
} // namespace

namespace FREYA_NAMESPACE
{
    ShadowPass::ShadowPass(
        const skr::Arc<Device>&              device,
        const skr::Arc<PhysicalDevice>&      physicalDevice,
        const skr::Arc<FreyaOptions>&        freyaOptions,
        const skr::Arc<BoneMatrixResources>& boneResources,
        const vk::RenderPass                 renderPass,
        const vk::PipelineLayout             pipelineLayout,
        const vk::Pipeline                   pipeline,
        const vk::Pipeline                   pointPipeline,
        const vk::Image                      cascadeImage,
        const vk::DeviceMemory               cascadeMemory,
        const vk::ImageView                  cascadeArrayView,
        const std::vector<vk::ImageView>&    cascadeLayerViews,
        const std::vector<vk::Framebuffer>&  cascadeFramebuffers,
        const vk::Image                      spotImage,
        const vk::DeviceMemory               spotMemory,
        const vk::ImageView                  spotArrayView,
        const std::vector<vk::ImageView>&    spotLayerViews,
        const std::vector<vk::Framebuffer>&  spotFramebuffers,
        const vk::Image                      pointImage,
        const vk::DeviceMemory               pointMemory,
        const vk::ImageView                  pointArrayView,
        const std::vector<vk::ImageView>&    pointFaceViews,
        const std::vector<vk::Framebuffer>&  pointFramebuffers,
        const skr::Arc<Buffer>&              uniformBuffer,
        const vk::Sampler                    compareSampler,
        const std::uint32_t                  cascadeCount,
        const std::uint32_t                  maxSpotShadows,
        const std::uint32_t                  maxPointShadows) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mFreyaOptions(freyaOptions), mBoneResources(boneResources),
        mRenderPass(renderPass), mPipelineLayout(pipelineLayout),
        mPipeline(pipeline), mPointPipeline(pointPipeline),
        mCascadeImage(cascadeImage), mCascadeMemory(cascadeMemory),
        mCascadeArrayView(cascadeArrayView),
        mCascadeLayerViews(cascadeLayerViews),
        mCascadeFramebuffers(cascadeFramebuffers), mSpotImage(spotImage),
        mSpotMemory(spotMemory), mSpotArrayView(spotArrayView),
        mSpotLayerViews(spotLayerViews), mSpotFramebuffers(spotFramebuffers),
        mPointImage(pointImage), mPointMemory(pointMemory),
        mPointArrayView(pointArrayView), mPointFaceViews(pointFaceViews),
        mPointFramebuffers(pointFramebuffers), mUniformBuffer(uniformBuffer),
        mCompareSampler(compareSampler), mCascadeCount(cascadeCount),
        mMaxSpotShadows(maxSpotShadows), mMaxPointShadows(maxPointShadows),
        mResolution(freyaOptions->shadowMapResolution),
        mSpotResolution(
            maxSpotShadows == 0 ? 1u : freyaOptions->shadowMapResolution),
        mPointResolution(
            maxPointShadows == 0 ? 1u : freyaOptions->shadowMapResolution)
    {
    }

    ShadowPass::~ShadowPass()
    {
        destroyGpuResources();
    }

    void ShadowPass::destroyGpuResources()
    {
        if (!mDevice)
            return;

        auto& vkDevice = mDevice->Get();

        for (auto& fb : mCascadeFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto& fb : mSpotFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto& fb : mPointFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        mCascadeFramebuffers.clear();
        mSpotFramebuffers.clear();
        mPointFramebuffers.clear();

        for (auto& view : mCascadeLayerViews)
            vkDevice.destroyImageView(view);
        for (auto& view : mSpotLayerViews)
            vkDevice.destroyImageView(view);
        for (auto& view : mPointFaceViews)
            vkDevice.destroyImageView(view);
        mCascadeLayerViews.clear();
        mSpotLayerViews.clear();
        mPointFaceViews.clear();

        if (mCascadeArrayView)
            vkDevice.destroyImageView(mCascadeArrayView);
        if (mSpotArrayView)
            vkDevice.destroyImageView(mSpotArrayView);
        if (mPointArrayView)
            vkDevice.destroyImageView(mPointArrayView);
        mCascadeArrayView = VK_NULL_HANDLE;
        mSpotArrayView    = VK_NULL_HANDLE;
        mPointArrayView   = VK_NULL_HANDLE;

        if (mCascadeImage)
        {
            vkDevice.destroyImage(mCascadeImage);
            vkDevice.freeMemory(mCascadeMemory);
        }
        if (mSpotImage)
        {
            vkDevice.destroyImage(mSpotImage);
            vkDevice.freeMemory(mSpotMemory);
        }
        if (mPointImage)
        {
            vkDevice.destroyImage(mPointImage);
            vkDevice.freeMemory(mPointMemory);
        }
        mCascadeImage  = VK_NULL_HANDLE;
        mCascadeMemory = VK_NULL_HANDLE;
        mSpotImage     = VK_NULL_HANDLE;
        mSpotMemory    = VK_NULL_HANDLE;
        mPointImage    = VK_NULL_HANDLE;
        mPointMemory   = VK_NULL_HANDLE;

        if (mCompareSampler)
            vkDevice.destroySampler(mCompareSampler);
        mCompareSampler = VK_NULL_HANDLE;

        if (mPipeline)
            vkDevice.destroyPipeline(mPipeline);
        if (mPointPipeline)
            vkDevice.destroyPipeline(mPointPipeline);
        if (mPipelineLayout)
            vkDevice.destroyPipelineLayout(mPipelineLayout);
        if (mRenderPass)
            vkDevice.destroyRenderPass(mRenderPass);
        mPipeline       = VK_NULL_HANDLE;
        mPointPipeline  = VK_NULL_HANDLE;
        mPipelineLayout = VK_NULL_HANDLE;
        mRenderPass     = VK_NULL_HANDLE;

        mUniformBuffer.reset();
    }

    void ShadowPass::StealResourcesFrom(ShadowPass& other)
    {
        if (this == &other)
            return;

        destroyGpuResources();

        mRenderPass     = other.mRenderPass;
        mPipelineLayout = other.mPipelineLayout;
        mPipeline       = other.mPipeline;
        mPointPipeline  = other.mPointPipeline;

        mCascadeImage        = other.mCascadeImage;
        mCascadeMemory       = other.mCascadeMemory;
        mCascadeArrayView    = other.mCascadeArrayView;
        mCascadeLayerViews   = std::move(other.mCascadeLayerViews);
        mCascadeFramebuffers = std::move(other.mCascadeFramebuffers);

        mSpotImage        = other.mSpotImage;
        mSpotMemory       = other.mSpotMemory;
        mSpotArrayView    = other.mSpotArrayView;
        mSpotLayerViews   = std::move(other.mSpotLayerViews);
        mSpotFramebuffers = std::move(other.mSpotFramebuffers);

        mPointImage        = other.mPointImage;
        mPointMemory       = other.mPointMemory;
        mPointArrayView    = other.mPointArrayView;
        mPointFaceViews    = std::move(other.mPointFaceViews);
        mPointFramebuffers = std::move(other.mPointFramebuffers);

        mUniformBuffer  = std::move(other.mUniformBuffer);
        mCompareSampler = other.mCompareSampler;
        mBoneResources  = other.mBoneResources;

        mCascadeCount    = other.mCascadeCount;
        mMaxSpotShadows  = other.mMaxSpotShadows;
        mMaxPointShadows = other.mMaxPointShadows;
        mResolution      = other.mResolution;
        mSpotResolution  = other.mSpotResolution;
        mPointResolution = other.mPointResolution;
        mFrameIndex      = other.mFrameIndex;

        mShadowData           = {};
        mHasDirectionalShadow = false;
        mActiveSpotCount      = 0;
        mActivePointCount     = 0;

        other.mRenderPass       = VK_NULL_HANDLE;
        other.mPipelineLayout   = VK_NULL_HANDLE;
        other.mPipeline         = VK_NULL_HANDLE;
        other.mPointPipeline    = VK_NULL_HANDLE;
        other.mCascadeImage     = VK_NULL_HANDLE;
        other.mCascadeMemory    = VK_NULL_HANDLE;
        other.mCascadeArrayView = VK_NULL_HANDLE;
        other.mSpotImage        = VK_NULL_HANDLE;
        other.mSpotMemory       = VK_NULL_HANDLE;
        other.mSpotArrayView    = VK_NULL_HANDLE;
        other.mPointImage       = VK_NULL_HANDLE;
        other.mPointMemory      = VK_NULL_HANDLE;
        other.mPointArrayView   = VK_NULL_HANDLE;
        other.mCompareSampler   = VK_NULL_HANDLE;
        other.mUniformBuffer.reset();
    }

    void ShadowPass::Update(const LightService& lights,
                            const glm::mat4&    cameraView,
                            const glm::mat4&    cameraProj,
                            const glm::vec3&    cameraPos,
                            const float         nearPlane,
                            const float         drawDistance,
                            const std::uint32_t frameIndex)
    {
        (void) cameraPos;
        mFrameIndex = frameIndex;

        mShadowData = ShadowUniformBuffer {};

        const float softScale = 1.0f;
        // params.x = depth bias in light NDC (receiver).
        // params.y = normal offset in shadow-map texels (receiver).
        mShadowData.params = glm::vec4(
            std::max(0.0005f, mFreyaOptions->shadowBias),
            std::clamp(mFreyaOptions->shadowBias * 1000.0f, 1.25f, 4.0f),
            0.0f,
            // Soft scale magnitude; sign encodes Reverse-Z for shaders.
            mFreyaOptions->ReverseZ ? softScale : -softScale);
        mShadowData.reverseZ =
            glm::vec4(mFreyaOptions->ReverseZ ? 1.0f : 0.0f,
                      static_cast<float>(std::max(mResolution, 1u)),
                      0.0f,
                      0.0f);
        mShadowData.pcss = glm::vec4(
            std::max(0.0f, mFreyaOptions->shadowLightSize),
            std::max(1.0f, mFreyaOptions->shadowMaxSoftness),
            std::clamp(mFreyaOptions->shadowMinVisibility, 0.0f, 0.95f),
            static_cast<float>(
                std::clamp(mFreyaOptions->shadowSampleCount, 1u, 16u)));

        const Light* sun = nullptr;
        for (std::uint32_t i = 0; i < lights.GetLightCount(); ++i)
        {
            const auto* light = lights.GetLight(i);
            if (light != nullptr &&
                light->type == static_cast<float>(LightType::Directional) &&
                light->castShadows)
            {
                sun = light;
                break;
            }
        }

        mHasDirectionalShadow = sun != nullptr;
        if (mHasDirectionalShadow)
        {
            mShadowData.params.z = static_cast<float>(mCascadeCount);
            computeCascades(*sun, cameraView, cameraProj, nearPlane,
                            drawDistance);
        }

        mActiveSpotCount           = 0;
        mShadowData.spotLightIndex = glm::vec4(-1.0f);

        for (std::uint32_t i = 0;
             i < lights.GetLightCount() && mActiveSpotCount < mMaxSpotShadows;
             ++i)
        {
            const auto* light = lights.GetLight(i);
            if (light == nullptr ||
                light->type != static_cast<float>(LightType::Spot) ||
                !light->castShadows)
                continue;

            const auto slot                  = mActiveSpotCount++;
            mShadowData.spotViewProj[slot]   = computeSpotViewProj(*light);
            mShadowData.spotLightIndex[slot] = static_cast<float>(i);
        }

        mActivePointCount           = 0;
        mShadowData.pointLightIndex = glm::vec4(-1.0f);

        for (std::uint32_t i = 0;
             i < lights.GetLightCount() && mActivePointCount < mMaxPointShadows;
             ++i)
        {
            const auto* light = lights.GetLight(i);
            if (light == nullptr ||
                light->type != static_cast<float>(LightType::Point) ||
                !light->castShadows)
                continue;

            const auto slot = mActivePointCount++;
            mShadowData.pointLightPosFar[slot] =
                glm::vec4(light->position, light->radius);
            mShadowData.pointLightIndex[slot] = static_cast<float>(i);
        }

        mUniformBuffer->Copy(&mShadowData, sizeof(ShadowUniformBuffer),
                             GetUniformBufferOffset(frameIndex));
    }

    void ShadowPass::computeCascades(const Light&     sun,
                                     const glm::mat4& cameraView,
                                     const glm::mat4& cameraProj,
                                     const float      nearPlane,
                                     const float      drawDistance)
    {
        const float cascadeFar =
            std::min(drawDistance, kMaxDirectionalShadowDistance);
        const auto splits =
            computePracticalSplits(mCascadeCount, nearPlane, cascadeFar);
        const auto frustum = frustumParamsFromProjection(cameraProj);

        const auto invView  = glm::inverse(cameraView);
        const auto lightDir = glm::normalize(sun.direction);
        const auto up       = std::abs(lightDir.y) < 0.99f
                                  ? glm::vec3(0.0f, 1.0f, 0.0f)
                                  : glm::vec3(1.0f, 0.0f, 0.0f);
        const auto resolution =
            static_cast<float>(std::max(mResolution, 1u));

        for (std::uint32_t i = 0; i < mCascadeCount; ++i)
        {
            const auto splitNear = (i == 0) ? nearPlane : splits[i - 1];
            const auto splitFar  = splits[i];

            const auto worldCorners = worldFrustumSliceCorners(
                invView, frustum.tanHalfFovY, frustum.aspect, splitNear,
                splitFar);

            glm::vec3 center {};
            const auto radius =
                boundingSphereRadius(worldCorners, center);

            const auto lightView = stabilizedLightView(
                center, lightDir, up, radius, resolution);
            const auto bounds =
                lightSpaceBoundsForSlice(lightView, worldCorners);
            const auto lightProj =
                lightOrthoFromBounds(bounds, mFreyaOptions->ReverseZ);

            mShadowData.cascadeViewProj[i] = lightProj * lightView;
            mShadowData.cascadeSplits[i]   = splitFar;
            const auto worldTexel =
                std::max(bounds.extentX * (1.0f + 2.0f * kCascadeXyPadFrac),
                         bounds.extentY * (1.0f + 2.0f * kCascadeXyPadFrac)) /
                resolution;
            mShadowData.cascadeTexelSize[static_cast<int>(i)] = worldTexel;
        }
    }

    glm::mat4 ShadowPass::computeSpotViewProj(const Light& light) const
    {
        const auto direction = glm::normalize(light.direction);
        const auto up        = std::abs(direction.y) < 0.99f
                                   ? glm::vec3(0.0f, 1.0f, 0.0f)
                                   : glm::vec3(1.0f, 0.0f, 0.0f);

        const auto view =
            glm::lookAt(light.position, light.position + direction, up);

        const auto halfAngle =
            std::acos(std::clamp(light.outerCutoff, -1.0f, 1.0f));
        const auto fov =
            std::clamp(halfAngle * 2.0f, 0.01f, glm::pi<float>() - 0.01f);

        constexpr auto near = 0.01f;
        const auto     far  = std::max(light.radius, near + 0.01f);

        auto proj = mFreyaOptions->ReverseZ
                        ? glm::perspective(fov, 1.0f, far, near)
                        : glm::perspective(fov, 1.0f, near, far);
        proj[1][1] *= -1.0f;

        return proj * view;
    }

    glm::mat4 ShadowPass::computePointFaceViewProj(
        const glm::vec3& position, const float far,
        const std::uint32_t face) const
    {
        static constexpr std::array<glm::vec3, 6> directions = {
            glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
        };
        static constexpr std::array<glm::vec3, 6> ups = {
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        };

        const auto view =
            glm::lookAt(position, position + directions[face], ups[face]);

        constexpr auto near       = 0.05f;
        const auto     farClamped = std::max(far, near + 0.01f);

        auto proj = mFreyaOptions->ReverseZ
                        ? glm::perspective(glm::half_pi<float>(), 1.0f,
                                           farClamped, near)
                        : glm::perspective(glm::half_pi<float>(), 1.0f, near,
                                           farClamped);

        return proj * view;
    }

    void ShadowPass::Render(
        const skr::Arc<CommandPool>&                 commandPool,
        const std::function<void(const glm::mat4&)>& prepareCull,
        const std::function<void()>&                 drawScene) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Shadow);

        renderCascades(commandPool, prepareCull, drawScene);
        renderSpots(commandPool, prepareCull, drawScene);
        renderPoints(commandPool, prepareCull, drawScene);

        mDevice->EndDebugLabel(commandBuffer);
    }

    void ShadowPass::bindBoneDescriptorSet(
        const vk::CommandBuffer commandBuffer) const
    {
        if (!mBoneResources)
            return;

        auto boneSet = mBoneResources->GetSet(mFrameIndex);
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            mPipelineLayout,
            0,
            1,
            &boneSet,
            0,
            nullptr);
    }

    void ShadowPass::renderCascades(
        const skr::Arc<CommandPool>&                 commandPool,
        const std::function<void(const glm::mat4&)>& prepareCull,
        const std::function<void()>&                 drawScene) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::ShadowCascades);

        const auto viewport =
            vk::Viewport()
                .setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(mResolution))
                .setHeight(static_cast<float>(mResolution))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        const auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(
            { mResolution, mResolution });

        const auto clearValue = vk::ClearValue().setDepthStencil(
            vk::ClearDepthStencilValue().setDepth(
                mFreyaOptions->ReverseZ ? 0.0f : 1.0f));

        for (std::uint32_t i = 0; i < mCascadeCount; ++i)
        {
            char label[64];
            std::snprintf(label, sizeof(label), "CSM Cascade %u%s", i,
                          mHasDirectionalShadow ? "" : " (clear)");
            mDevice->BeginDebugLabel(
                commandBuffer, label, DebugLabel::ShadowColor);

            const auto lightVP = mShadowData.cascadeViewProj[i];
            if (mHasDirectionalShadow && prepareCull)
                prepareCull(lightVP);

            commandBuffer.beginRenderPass(
                vk::RenderPassBeginInfo()
                    .setRenderPass(mRenderPass)
                    .setFramebuffer(mCascadeFramebuffers[i])
                    .setRenderArea(scissor)
                    .setClearValues(clearValue),
                vk::SubpassContents::eInline);

            if (mHasDirectionalShadow)
            {
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                           mPipeline);
                bindBoneDescriptorSet(commandBuffer);
                commandBuffer.setViewport(0, 1, &viewport);
                commandBuffer.setScissor(0, 1, &scissor);

                ShadowPushConstant pc {};
                pc.lightVP     = lightVP;
                pc.lightPosFar = glm::vec4(0.0f);
                pc.reverseZAndPad =
                    glm::vec4(mShadowData.reverseZ.x, 0.0f, 0.0f, 0.0f);
                commandBuffer.pushConstants(
                    mPipelineLayout,
                    vk::ShaderStageFlagBits::eVertex |
                        vk::ShaderStageFlagBits::eFragment,
                    0,
                    sizeof(ShadowPushConstant),
                    &pc);

                drawScene();
            }

            commandBuffer.endRenderPass();
            mDevice->EndDebugLabel(commandBuffer);
        }

        mDevice->EndDebugLabel(commandBuffer);
    }

    void ShadowPass::renderSpots(
        const skr::Arc<CommandPool>&                 commandPool,
        const std::function<void(const glm::mat4&)>& prepareCull,
        const std::function<void()>&                 drawScene) const
    {
        // Clear every allocated layer every frame so unused slots leave
        // SHADER_READ_ONLY_OPTIMAL (descriptor samples the full array).
        if (mSpotFramebuffers.empty())
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::ShadowSpots);

        const auto viewport =
            vk::Viewport()
                .setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(mSpotResolution))
                .setHeight(static_cast<float>(mSpotResolution))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        const auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(
            { mSpotResolution, mSpotResolution });

        const auto clearValue = vk::ClearValue().setDepthStencil(
            vk::ClearDepthStencilValue().setDepth(
                mFreyaOptions->ReverseZ ? 0.0f : 1.0f));

        for (std::uint32_t i = 0; i < mSpotFramebuffers.size(); ++i)
        {
            char label[64];
            std::snprintf(label, sizeof(label), "Spot Shadow %u%s", i,
                          (i < mActiveSpotCount) ? "" : " (clear)");
            mDevice->BeginDebugLabel(
                commandBuffer, label, DebugLabel::ShadowColor);

            if (i < mActiveSpotCount)
            {
                const auto lightVP = mShadowData.spotViewProj[i];
                if (prepareCull)
                    prepareCull(lightVP);
            }

            commandBuffer.beginRenderPass(
                vk::RenderPassBeginInfo()
                    .setRenderPass(mRenderPass)
                    .setFramebuffer(mSpotFramebuffers[i])
                    .setRenderArea(scissor)
                    .setClearValues(clearValue),
                vk::SubpassContents::eInline);

            if (i < mActiveSpotCount)
            {
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                           mPipeline);
                bindBoneDescriptorSet(commandBuffer);
                commandBuffer.setViewport(0, 1, &viewport);
                commandBuffer.setScissor(0, 1, &scissor);

                const auto         lightVP = mShadowData.spotViewProj[i];
                ShadowPushConstant pc {};
                pc.lightVP     = lightVP;
                pc.lightPosFar = glm::vec4(0.0f);
                pc.reverseZAndPad =
                    glm::vec4(mShadowData.reverseZ.x, 0.0f, 0.0f, 0.0f);
                commandBuffer.pushConstants(
                    mPipelineLayout,
                    vk::ShaderStageFlagBits::eVertex |
                        vk::ShaderStageFlagBits::eFragment,
                    0,
                    sizeof(ShadowPushConstant),
                    &pc);

                drawScene();
            }

            commandBuffer.endRenderPass();
            mDevice->EndDebugLabel(commandBuffer);
        }

        mDevice->EndDebugLabel(commandBuffer);
    }

    void ShadowPass::renderPoints(
        const skr::Arc<CommandPool>&                 commandPool,
        const std::function<void(const glm::mat4&)>& prepareCull,
        const std::function<void()>&                 drawScene) const
    {
        // Same as spots: clear every cube face so unused point slots are
        // not left in UNDEFINED when the cube-array is sampled.
        if (mPointFramebuffers.empty())
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::ShadowPoints);

        const auto viewport =
            vk::Viewport()
                .setX(0.0f)
                .setY(0.0f)
                .setWidth(static_cast<float>(mPointResolution))
                .setHeight(static_cast<float>(mPointResolution))
                .setMinDepth(0.0f)
                .setMaxDepth(1.0f);

        const auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(
            { mPointResolution, mPointResolution });

        const auto clearValue = vk::ClearValue().setDepthStencil(
            vk::ClearDepthStencilValue().setDepth(
                mFreyaOptions->ReverseZ ? 0.0f : 1.0f));

        const auto pointSlotCount =
            static_cast<std::uint32_t>(mPointFramebuffers.size() / 6);

        for (std::uint32_t p = 0; p < pointSlotCount; ++p)
        {
            const auto      posFar = mShadowData.pointLightPosFar[p];
            const glm::vec3 position(posFar);
            const auto      far    = posFar.w;
            const bool      active = p < mActivePointCount;

            for (std::uint32_t face = 0; face < 6; ++face)
            {
                const auto framebufferIndex = p * 6 + face;

                char label[64];
                std::snprintf(label, sizeof(label), "Point Shadow %u Face %u%s",
                              p, face, active ? "" : " (clear)");
                mDevice->BeginDebugLabel(
                    commandBuffer, label, DebugLabel::ShadowColor);

                const auto lightVP =
                    computePointFaceViewProj(position, far, face);
                if (active && prepareCull)
                    prepareCull(lightVP);

                commandBuffer.beginRenderPass(
                    vk::RenderPassBeginInfo()
                        .setRenderPass(mRenderPass)
                        .setFramebuffer(mPointFramebuffers[framebufferIndex])
                        .setRenderArea(scissor)
                        .setClearValues(clearValue),
                    vk::SubpassContents::eInline);

                if (active)
                {
                    commandBuffer.bindPipeline(
                        vk::PipelineBindPoint::eGraphics, mPointPipeline);
                    bindBoneDescriptorSet(commandBuffer);
                    commandBuffer.setViewport(0, 1, &viewport);
                    commandBuffer.setScissor(0, 1, &scissor);

                    ShadowPushConstant pc {};
                    pc.lightVP        = lightVP;
                    pc.lightPosFar    = posFar;
                    pc.reverseZAndPad = mShadowData.reverseZ;
                    commandBuffer.pushConstants(
                        mPipelineLayout,
                        vk::ShaderStageFlagBits::eVertex |
                            vk::ShaderStageFlagBits::eFragment,
                        0,
                        sizeof(ShadowPushConstant),
                        &pc);

                    drawScene();
                }

                commandBuffer.endRenderPass();
                mDevice->EndDebugLabel(commandBuffer);
            }
        }

        mDevice->EndDebugLabel(commandBuffer);
    }
} // namespace FREYA_NAMESPACE
