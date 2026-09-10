#include "Freya/Core/ShadowPass.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

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
            const auto p =
                static_cast<float>(i) / static_cast<float>(cascadeCount);
            const auto logSplit =
                nearPlane * std::pow(cascadeFar / nearPlane, p);
            const auto uniformSplit = nearPlane + (cascadeFar - nearPlane) * p;
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
        const float pullBack  = radius + kCascadeZPad + kCascadePullEps;
        const float texelSize = (2.0f * radius) / resolution;

        auto lightView = glm::lookAt(center - lightDir * pullBack, center, up);
        auto centerLS  = glm::vec3(lightView * glm::vec4(center, 1.0f));
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
        return reverseZ ? glm::ortho(bounds.minB.x, bounds.maxB.x,
                                     bounds.minB.y, bounds.maxB.y, far, near)
                        : glm::ortho(bounds.minB.x, bounds.maxB.x,
                                     bounds.minB.y, bounds.maxB.y, near, far);
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
        const vk::RenderPass                 cascadeRenderPass,
        const vk::RenderPass                 pointRenderPass,
        const vk::PipelineLayout             pipelineLayout,
        const vk::Pipeline                   pipeline,
        const vk::Pipeline                   cascadePipeline,
        const vk::Pipeline                   pointPipeline,
        const vk::Image                      cascadeImage,
        const vk::DeviceMemory               cascadeMemory,
        const vk::ImageView                  cascadeArrayView,
        const std::vector<vk::ImageView>&    cascadeLayerViews,
        const vk::Framebuffer                cascadeFramebuffer,
        const std::vector<vk::Framebuffer>&  spotFramebuffers,
        const vk::Image                      spotImage,
        const vk::DeviceMemory               spotMemory,
        const vk::ImageView                  spotArrayView,
        const std::vector<vk::ImageView>&    spotLayerViews,
        const vk::Image                      pointImage,
        const vk::DeviceMemory               pointMemory,
        const vk::ImageView                  pointArrayView,
        const std::vector<vk::ImageView>&    pointSlotViews,
        const std::vector<vk::Framebuffer>&  pointFramebuffers,
        const skr::Arc<Buffer>&              uniformBuffer,
        const vk::Sampler                    compareSampler,
        const vk::DescriptorSetLayout        shadowUboSetLayout,
        const vk::DescriptorPool             shadowUboPool,
        std::vector<vk::DescriptorSet>
                            shadowUboSets,
        const std::uint32_t cascadeCount,
        const std::uint32_t maxSpotShadows,
        const std::uint32_t maxPointShadows) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mFreyaOptions(freyaOptions), mBoneResources(boneResources),
        mRenderPass(renderPass), mCascadeRenderPass(cascadeRenderPass),
        mPointRenderPass(pointRenderPass), mPipelineLayout(pipelineLayout),
        mPipeline(pipeline), mCascadePipeline(cascadePipeline),
        mPointPipeline(pointPipeline), mCascadeImage(cascadeImage),
        mCascadeMemory(cascadeMemory), mCascadeArrayView(cascadeArrayView),
        mCascadeLayerViews(cascadeLayerViews),
        mCascadeFramebuffer(cascadeFramebuffer), mSpotImage(spotImage),
        mSpotMemory(spotMemory), mSpotArrayView(spotArrayView),
        mSpotLayerViews(spotLayerViews), mSpotFramebuffers(spotFramebuffers),
        mPointImage(pointImage), mPointMemory(pointMemory),
        mPointArrayView(pointArrayView), mPointSlotViews(pointSlotViews),
        mPointFramebuffers(pointFramebuffers), mUniformBuffer(uniformBuffer),
        mCompareSampler(compareSampler),
        mShadowUboSetLayout(shadowUboSetLayout), mShadowUboPool(shadowUboPool),
        mShadowUboSets(std::move(shadowUboSets)), mCascadeCount(cascadeCount),
        mMaxSpotShadows(maxSpotShadows), mMaxPointShadows(maxPointShadows),
        mResolution(freyaOptions->shadowMapResolution),
        mSpotResolution(maxSpotShadows == 0
                            ? 1u
                            : ResolveShadowSideResolution(
                                  freyaOptions->shadowMapResolution,
                                  freyaOptions->shadowSpotResolution,
                                  freyaOptions->shadowSpotResolutionDivisor)),
        mPointResolution(maxPointShadows == 0
                             ? 1u
                             : ResolveShadowSideResolution(
                                   freyaOptions->shadowMapResolution,
                                   freyaOptions->shadowPointResolution,
                                   freyaOptions->shadowPointResolutionDivisor))
    {
        mPointNeedRedraw.fill(true);
        mPointNeedClear.fill(false);
        mPointHasContent.fill(false);
        mPointHasLast.fill(false);
        mPointUpdateAge.fill(0);
    }

    ShadowPass::~ShadowPass()
    {
        destroyGpuResources();
    }

    void ShadowPass::destroyGpuResources()
    {
        if (!mDevice)
            return;

        mDevice->Get().waitIdle();
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mSpotFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto& fb : mPointFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        mSpotFramebuffers.clear();
        mPointFramebuffers.clear();

        if (mCascadeFramebuffer)
        {
            vkDevice.destroyFramebuffer(mCascadeFramebuffer);
            mCascadeFramebuffer = VK_NULL_HANDLE;
        }

        for (auto& view : mCascadeLayerViews)
            vkDevice.destroyImageView(view);
        for (auto& view : mSpotLayerViews)
            vkDevice.destroyImageView(view);
        for (auto& view : mPointSlotViews)
            vkDevice.destroyImageView(view);
        mCascadeLayerViews.clear();
        mSpotLayerViews.clear();
        mPointSlotViews.clear();

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
        if (mCascadePipeline)
            vkDevice.destroyPipeline(mCascadePipeline);
        if (mPointPipeline)
            vkDevice.destroyPipeline(mPointPipeline);
        if (mPipelineLayout)
            vkDevice.destroyPipelineLayout(mPipelineLayout);
        if (mCascadeRenderPass)
            vkDevice.destroyRenderPass(mCascadeRenderPass);
        if (mPointRenderPass)
            vkDevice.destroyRenderPass(mPointRenderPass);
        if (mRenderPass)
            vkDevice.destroyRenderPass(mRenderPass);
        if (mShadowUboPool)
            vkDevice.destroyDescriptorPool(mShadowUboPool);
        if (mShadowUboSetLayout)
            vkDevice.destroyDescriptorSetLayout(mShadowUboSetLayout);
        mPipeline           = VK_NULL_HANDLE;
        mCascadePipeline    = VK_NULL_HANDLE;
        mPointPipeline      = VK_NULL_HANDLE;
        mPipelineLayout     = VK_NULL_HANDLE;
        mCascadeRenderPass  = VK_NULL_HANDLE;
        mPointRenderPass    = VK_NULL_HANDLE;
        mRenderPass         = VK_NULL_HANDLE;
        mShadowUboPool      = VK_NULL_HANDLE;
        mShadowUboSetLayout = VK_NULL_HANDLE;
        mShadowUboSets.clear();

        mUniformBuffer.reset();
    }

    void ShadowPass::StealResourcesFrom(ShadowPass& other)
    {
        if (this == &other)
            return;

        destroyGpuResources();

        mRenderPass        = other.mRenderPass;
        mCascadeRenderPass = other.mCascadeRenderPass;
        mPointRenderPass   = other.mPointRenderPass;
        mPipelineLayout    = other.mPipelineLayout;
        mPipeline          = other.mPipeline;
        mCascadePipeline   = other.mCascadePipeline;
        mPointPipeline     = other.mPointPipeline;

        mCascadeImage       = other.mCascadeImage;
        mCascadeMemory      = other.mCascadeMemory;
        mCascadeArrayView   = other.mCascadeArrayView;
        mCascadeLayerViews  = std::move(other.mCascadeLayerViews);
        mCascadeFramebuffer = other.mCascadeFramebuffer;

        mSpotImage        = other.mSpotImage;
        mSpotMemory       = other.mSpotMemory;
        mSpotArrayView    = other.mSpotArrayView;
        mSpotLayerViews   = std::move(other.mSpotLayerViews);
        mSpotFramebuffers = std::move(other.mSpotFramebuffers);

        mPointImage        = other.mPointImage;
        mPointMemory       = other.mPointMemory;
        mPointArrayView    = other.mPointArrayView;
        mPointSlotViews    = std::move(other.mPointSlotViews);
        mPointFramebuffers = std::move(other.mPointFramebuffers);

        mUniformBuffer      = std::move(other.mUniformBuffer);
        mCompareSampler     = other.mCompareSampler;
        mBoneResources      = other.mBoneResources;
        mShadowUboSetLayout = other.mShadowUboSetLayout;
        mShadowUboPool      = other.mShadowUboPool;
        mShadowUboSets      = std::move(other.mShadowUboSets);

        mCascadeCount    = other.mCascadeCount;
        mMaxSpotShadows  = other.mMaxSpotShadows;
        mMaxPointShadows = other.mMaxPointShadows;
        mResolution      = other.mResolution;
        mSpotResolution  = other.mSpotResolution;
        mPointResolution = other.mPointResolution;
        mFrameIndex      = other.mFrameIndex;

        mShadowData           = {};
        mCascadeCullViewProj  = glm::mat4(1.0f);
        mHasDirectionalShadow = false;
        mActiveSpotCount      = 0;
        mActivePointCount     = 0;
        mCascadesNeedRedraw   = true;
        mCascadeUpdateAge     = 0;
        mHasLastCascadeMotion = false;
        mPointNeedRedraw.fill(true);
        mPointNeedClear.fill(false);
        mPointHasContent.fill(false);
        mPointHasLast.fill(false);
        mPointUpdateAge.fill(0);

        other.mRenderPass         = VK_NULL_HANDLE;
        other.mCascadeRenderPass  = VK_NULL_HANDLE;
        other.mPointRenderPass    = VK_NULL_HANDLE;
        other.mPipelineLayout     = VK_NULL_HANDLE;
        other.mPipeline           = VK_NULL_HANDLE;
        other.mCascadePipeline    = VK_NULL_HANDLE;
        other.mPointPipeline      = VK_NULL_HANDLE;
        other.mCascadeImage       = VK_NULL_HANDLE;
        other.mCascadeMemory      = VK_NULL_HANDLE;
        other.mCascadeArrayView   = VK_NULL_HANDLE;
        other.mCascadeFramebuffer = VK_NULL_HANDLE;
        other.mSpotImage          = VK_NULL_HANDLE;
        other.mSpotMemory         = VK_NULL_HANDLE;
        other.mSpotArrayView      = VK_NULL_HANDLE;
        other.mPointImage         = VK_NULL_HANDLE;
        other.mPointMemory        = VK_NULL_HANDLE;
        other.mPointArrayView     = VK_NULL_HANDLE;
        other.mCompareSampler     = VK_NULL_HANDLE;
        other.mShadowUboSetLayout = VK_NULL_HANDLE;
        other.mShadowUboPool      = VK_NULL_HANDLE;
        other.mShadowUboSets.clear();
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
        const float cascadeBlend =
            std::clamp(mFreyaOptions->shadowCascadeBlend, 0.0f, 0.5f);
        const bool useMask =
            mFreyaOptions->enableShadows && mFreyaOptions->enableShadowMask;
        mShadowData.reverseZ =
            glm::vec4(mFreyaOptions->ReverseZ ? 1.0f : 0.0f,
                      static_cast<float>(std::max(mResolution, 1u)),
                      cascadeBlend,
                      useMask ? 1.0f : 0.0f);
        mShadowData.pcss = glm::vec4(
            std::max(0.0f, mFreyaOptions->shadowLightSize),
            std::max(1.0f, mFreyaOptions->shadowMaxSoftness),
            std::clamp(mFreyaOptions->shadowMinVisibility, 0.0f, 0.95f),
            static_cast<float>(
                std::clamp(mFreyaOptions->shadowSampleCount, 1u, 16u)));

        const Light* sun = nullptr;
        for (std::uint32_t i = 0; i < lights.GetLightCount(); ++i)
        {
            const auto* light = lights.GetLight(LightHandle { i });
            if (light != nullptr && light->type == LightType::Directional &&
                light->castShadows)
            {
                sun = light;
                break;
            }
        }

        mHasDirectionalShadow = sun != nullptr;
        if (mHasDirectionalShadow)
        {
            const auto sunDir = glm::normalize(sun->direction);
            bool       motion = !mHasLastCascadeMotion;
            if (!motion)
            {
                for (int c = 0; c < 4 && !motion; ++c)
                    for (int r = 0; r < 4; ++r)
                    {
                        motion |= std::abs(cameraView[c][r] -
                                           mLastCameraView[c][r]) > 1e-5f;
                        motion |= std::abs(cameraProj[c][r] -
                                           mLastCameraProj[c][r]) > 1e-5f;
                    }
                motion |= glm::length(sunDir - mLastSunDir) > 1e-4f;
            }

            mLastCameraView       = cameraView;
            mLastCameraProj       = cameraProj;
            mLastSunDir           = sunDir;
            mHasLastCascadeMotion = true;

            const auto period =
                std::max(1u, mFreyaOptions->shadowCascadeUpdatePeriod);
            if (motion)
            {
                mCascadesNeedRedraw = true;
                mCascadeUpdateAge   = 0;
            }
            else
            {
                ++mCascadeUpdateAge;
                if (mCascadeUpdateAge >= period)
                {
                    mCascadesNeedRedraw = true;
                    mCascadeUpdateAge   = 0;
                }
                else
                    mCascadesNeedRedraw = false;
            }

            mShadowData.params.z = static_cast<float>(mCascadeCount);
            computeCascades(*sun, cameraView, cameraProj, nearPlane,
                            drawDistance);
        }
        else
        {
            mCascadesNeedRedraw   = true;
            mHasLastCascadeMotion = false;
        }

        mActiveSpotCount           = 0;
        mShadowData.spotLightIndex = glm::vec4(-1.0f);

        for (std::uint32_t i = 0;
             i < lights.GetLightCount() && mActiveSpotCount < mMaxSpotShadows;
             ++i)
        {
            const auto* light = lights.GetLight(LightHandle { i });
            if (light == nullptr || light->type != LightType::Spot ||
                !light->castShadows || light->intensity <= 1e-4f ||
                light->radius <= 1e-4f)
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
            const auto* light = lights.GetLight(LightHandle { i });
            if (light == nullptr || light->type != LightType::Point ||
                !light->castShadows || light->intensity <= 1e-4f ||
                light->radius <= 1e-4f)
                continue;

            const auto slot = mActivePointCount++;
            mShadowData.pointLightPosFar[slot] =
                glm::vec4(light->position, light->radius);
            mShadowData.pointLightIndex[slot] = static_cast<float>(i);

            for (std::uint32_t face = 0; face < 6; ++face)
            {
                mShadowData.pointFaceViewProj[slot * 6 + face] =
                    computePointFaceViewProj(
                        light->position, light->radius, face);
            }
        }

        const auto pointPeriod =
            std::max(1u, mFreyaOptions->shadowPointUpdatePeriod);
        for (std::uint32_t slot = 0; slot < mMaxPointShadows; ++slot)
        {
            if (slot < mActivePointCount)
            {
                const auto posFar = mShadowData.pointLightPosFar[slot];
                bool       motion = !mPointHasLast[slot];
                if (!motion)
                {
                    const auto delta = posFar - mLastPointPosFar[slot];
                    motion           = glm::length(glm::vec3(delta)) > 1e-3f ||
                                       std::abs(delta.w) > 1e-3f;
                }
                mLastPointPosFar[slot] = posFar;
                mPointHasLast[slot]    = true;
                mPointNeedClear[slot]  = false;

                if (motion)
                {
                    mPointNeedRedraw[slot] = true;
                    mPointUpdateAge[slot]  = 0;
                }
                else
                {
                    ++mPointUpdateAge[slot];
                    if (mPointUpdateAge[slot] >= pointPeriod)
                    {
                        mPointNeedRedraw[slot] = true;
                        mPointUpdateAge[slot]  = 0;
                    }
                    else
                        mPointNeedRedraw[slot] = false;
                }
            }
            else
            {
                if (mPointHasContent[slot])
                    mPointNeedClear[slot] = true;
                mPointNeedRedraw[slot] = false;
                mPointHasLast[slot]    = false;
                mPointUpdateAge[slot]  = 0;
            }
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

        const auto invView    = glm::inverse(cameraView);
        const auto lightDir   = glm::normalize(sun.direction);
        const auto up         = std::abs(lightDir.y) < 0.99f
                                    ? glm::vec3(0.0f, 1.0f, 0.0f)
                                    : glm::vec3(1.0f, 0.0f, 0.0f);
        const auto resolution = static_cast<float>(std::max(mResolution, 1u));

        std::vector<glm::vec3> allCorners;
        allCorners.reserve(static_cast<std::size_t>(mCascadeCount) * 8u);

        for (std::uint32_t i = 0; i < mCascadeCount; ++i)
        {
            const auto splitNear = (i == 0) ? nearPlane : splits[i - 1];
            const auto splitFar  = splits[i];

            const auto worldCorners =
                worldFrustumSliceCorners(invView, frustum.tanHalfFovY,
                                         frustum.aspect, splitNear, splitFar);
            for (const auto& c : worldCorners)
                allCorners.push_back(c);

            glm::vec3  center {};
            const auto radius = boundingSphereRadius(worldCorners, center);

            const auto lightView =
                stabilizedLightView(center, lightDir, up, radius, resolution);
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

        // Conservative cull VP covering every cascade frustum slice.
        if (!allCorners.empty())
        {
            glm::vec3 center = glm::vec3(0.0f);
            float     radius = 0.0f;
            for (const auto& c : allCorners)
                center += c;
            center /= static_cast<float>(allCorners.size());
            for (const auto& c : allCorners)
                radius = std::max(radius, glm::length(c - center));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const auto lightView =
                stabilizedLightView(center, lightDir, up, radius, resolution);
            auto minB = glm::vec3(std::numeric_limits<float>::max());
            auto maxB = glm::vec3(std::numeric_limits<float>::lowest());
            for (const auto& worldCorner : allCorners)
            {
                const auto ls =
                    glm::vec3(lightView * glm::vec4(worldCorner, 1.0f));
                minB = glm::min(minB, ls);
                maxB = glm::max(maxB, ls);
            }
            const auto extentX = std::max(maxB.x - minB.x, 1e-3f);
            const auto extentY = std::max(maxB.y - minB.y, 1e-3f);
            minB.x -= extentX * kCascadeXyPadFrac;
            maxB.x += extentX * kCascadeXyPadFrac;
            minB.y -= extentY * kCascadeXyPadFrac;
            maxB.y += extentY * kCascadeXyPadFrac;
            minB.z -= kCascadeZPad;
            maxB.z += kCascadeZPad;
            const LightOrthoBounds unionBounds { minB, maxB, extentX, extentY };
            mCascadeCullViewProj =
                lightOrthoFromBounds(unionBounds, mFreyaOptions->ReverseZ) *
                lightView;
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

    glm::mat4 ShadowPass::computePointCullViewProj(const glm::vec3& position,
                                                   const float      far) const
    {
        // Axis-aligned cube around the light range — over-includes corners
        // of the sphere but keeps a single DispatchCull per point slot.
        const float r    = std::max(far, 0.05f);
        const auto  view = glm::translate(glm::mat4(1.0f), -position);
        const auto  proj = mFreyaOptions->ReverseZ
                               ? glm::ortho(-r, r, -r, r, r, -r)
                               : glm::ortho(-r, r, -r, r, -r, r);
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

    void ShadowPass::bindShadowUboSet(
        const vk::CommandBuffer commandBuffer) const
    {
        if (mShadowUboSets.empty() || mFrameIndex >= mShadowUboSets.size())
            return;
        const auto set = mShadowUboSets[mFrameIndex];
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, mPipelineLayout, 2, 1, &set, 0,
            nullptr);
    }

    void ShadowPass::renderCascades(
        const skr::Arc<CommandPool>&                 commandPool,
        const std::function<void(const glm::mat4&)>& prepareCull,
        const std::function<void()>&                 drawScene) const
    {
        if (!mCascadeFramebuffer || !mCascadeRenderPass)
            return;

        // Temporal skip: keep previous cascade maps when camera/sun stable.
        if (mHasDirectionalShadow && !mCascadesNeedRedraw)
            return;

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

        if (mHasDirectionalShadow && prepareCull)
            prepareCull(mCascadeCullViewProj);

        commandBuffer.beginRenderPass(
            vk::RenderPassBeginInfo()
                .setRenderPass(mCascadeRenderPass)
                .setFramebuffer(mCascadeFramebuffer)
                .setRenderArea(scissor)
                .setClearValues(clearValue),
            vk::SubpassContents::eInline);

        if (mHasDirectionalShadow)
        {
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       mCascadePipeline);
            bindBoneDescriptorSet(commandBuffer);
            bindShadowUboSet(commandBuffer);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);

            ShadowPushConstant pc {};
            pc.lightVP     = mCascadeCullViewProj;
            pc.lightPosFar = glm::vec4(0.0f);
            pc.reverseZAndPad =
                glm::vec4(mShadowData.reverseZ.x, 0.0f, 0.0f, 0.0f);
            commandBuffer.pushConstants(mPipelineLayout,
                                        vk::ShaderStageFlagBits::eVertex |
                                            vk::ShaderStageFlagBits::eFragment,
                                        0, sizeof(ShadowPushConstant), &pc);

            drawScene();
        }

        commandBuffer.endRenderPass();
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
                bindShadowUboSet(commandBuffer);
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
        if (mPointFramebuffers.empty() || !mPointRenderPass)
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
            static_cast<std::uint32_t>(mPointFramebuffers.size());

        for (std::uint32_t p = 0; p < pointSlotCount; ++p)
        {
            const bool active    = p < mActivePointCount;
            const bool redraw    = active && mPointNeedRedraw[p];
            const bool clearOnly = mPointNeedClear[p];

            if (!redraw && !clearOnly)
                continue;

            const auto      posFar = mShadowData.pointLightPosFar[p];
            const glm::vec3 position(posFar);
            const auto      far = posFar.w;

            char label[64];
            std::snprintf(label, sizeof(label), "Point Shadow %u%s", p,
                          redraw ? "" : " (clear)");
            mDevice->BeginDebugLabel(
                commandBuffer, label, DebugLabel::ShadowColor);

            if (redraw && prepareCull)
                prepareCull(computePointCullViewProj(position, far));

            commandBuffer.beginRenderPass(
                vk::RenderPassBeginInfo()
                    .setRenderPass(mPointRenderPass)
                    .setFramebuffer(mPointFramebuffers[p])
                    .setRenderArea(scissor)
                    .setClearValues(clearValue),
                vk::SubpassContents::eInline);

            if (redraw)
            {
                commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                           mPointPipeline);
                bindBoneDescriptorSet(commandBuffer);
                bindShadowUboSet(commandBuffer);
                commandBuffer.setViewport(0, 1, &viewport);
                commandBuffer.setScissor(0, 1, &scissor);

                ShadowPushConstant pc {};
                pc.lightVP        = computePointCullViewProj(position, far);
                pc.lightPosFar    = posFar;
                pc.reverseZAndPad = glm::vec4(
                    mShadowData.reverseZ.x, static_cast<float>(p), 0.0f, 0.0f);
                commandBuffer.pushConstants(
                    mPipelineLayout,
                    vk::ShaderStageFlagBits::eVertex |
                        vk::ShaderStageFlagBits::eFragment,
                    0,
                    sizeof(ShadowPushConstant),
                    &pc);

                drawScene();
                mPointHasContent[p] = true;
            }
            else
            {
                mPointHasContent[p] = false;
            }

            mPointNeedClear[p] = false;

            commandBuffer.endRenderPass();
            mDevice->EndDebugLabel(commandBuffer);
        }

        mDevice->EndDebugLabel(commandBuffer);
    }
} // namespace FREYA_NAMESPACE
