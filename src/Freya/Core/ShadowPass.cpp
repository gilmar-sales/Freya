#include "Freya/Core/ShadowPass.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace FREYA_NAMESPACE
{
    ShadowPass::ShadowPass(
        const skr::Arc<Device>&             device,
        const skr::Arc<PhysicalDevice>&     physicalDevice,
        const skr::Arc<FreyaOptions>&       freyaOptions,
        const vk::RenderPass                renderPass,
        const vk::PipelineLayout            pipelineLayout,
        const vk::Pipeline                  pipeline,
        const vk::Image                     cascadeImage,
        const vk::DeviceMemory              cascadeMemory,
        const vk::ImageView                 cascadeArrayView,
        const std::vector<vk::ImageView>&   cascadeLayerViews,
        const std::vector<vk::Framebuffer>& cascadeFramebuffers,
        const vk::Image                     spotImage,
        const vk::DeviceMemory              spotMemory,
        const vk::ImageView                 spotArrayView,
        const std::vector<vk::ImageView>&   spotLayerViews,
        const std::vector<vk::Framebuffer>& spotFramebuffers,
        const vk::Image                     pointImage,
        const vk::DeviceMemory              pointMemory,
        const vk::ImageView                 pointArrayView,
        const std::vector<vk::ImageView>&   pointFaceViews,
        const std::vector<vk::Framebuffer>& pointFramebuffers,
        const skr::Arc<Buffer>&             uniformBuffer,
        const vk::Sampler                   compareSampler,
        const vk::Sampler                   regularSampler,
        const std::uint32_t                 cascadeCount,
        const std::uint32_t                 maxSpotShadows,
        const std::uint32_t                 maxPointShadows) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mFreyaOptions(freyaOptions), mRenderPass(renderPass),
        mPipelineLayout(pipelineLayout), mPipeline(pipeline),
        mCascadeImage(cascadeImage), mCascadeMemory(cascadeMemory),
        mCascadeArrayView(cascadeArrayView),
        mCascadeLayerViews(cascadeLayerViews),
        mCascadeFramebuffers(cascadeFramebuffers), mSpotImage(spotImage),
        mSpotMemory(spotMemory), mSpotArrayView(spotArrayView),
        mSpotLayerViews(spotLayerViews), mSpotFramebuffers(spotFramebuffers),
        mPointImage(pointImage), mPointMemory(pointMemory),
        mPointArrayView(pointArrayView), mPointFaceViews(pointFaceViews),
        mPointFramebuffers(pointFramebuffers), mUniformBuffer(uniformBuffer),
        mCompareSampler(compareSampler), mSampler(regularSampler),
        mCascadeCount(cascadeCount), mMaxSpotShadows(maxSpotShadows),
        mMaxPointShadows(maxPointShadows),
        mResolution(freyaOptions->shadowMapResolution)
    {
    }

    ShadowPass::~ShadowPass()
    {
        auto& vkDevice = mDevice->Get();

        for (auto& fb : mCascadeFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto& fb : mSpotFramebuffers)
            vkDevice.destroyFramebuffer(fb);
        for (auto& fb : mPointFramebuffers)
            vkDevice.destroyFramebuffer(fb);

        for (auto& view : mCascadeLayerViews)
            vkDevice.destroyImageView(view);
        for (auto& view : mSpotLayerViews)
            vkDevice.destroyImageView(view);
        for (auto& view : mPointFaceViews)
            vkDevice.destroyImageView(view);

        if (mCascadeArrayView)
            vkDevice.destroyImageView(mCascadeArrayView);
        if (mSpotArrayView)
            vkDevice.destroyImageView(mSpotArrayView);
        if (mPointArrayView)
            vkDevice.destroyImageView(mPointArrayView);

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

        vkDevice.destroySampler(mCompareSampler);
        vkDevice.destroySampler(mSampler);

        vkDevice.destroyPipeline(mPipeline);
        vkDevice.destroyPipelineLayout(mPipelineLayout);
        vkDevice.destroyRenderPass(mRenderPass);

        mUniformBuffer.reset();
    }

    void ShadowPass::Update(const LightService& lights,
                            const glm::mat4&    cameraView,
                            const glm::mat4&    cameraProj,
                            const glm::vec3&    cameraPos,
                            const float         nearPlane,
                            const float         drawDistance)
    {
        (void) cameraPos;

        mShadowData = ShadowUniformBuffer {};

        mShadowData.params =
            glm::vec4(mFreyaOptions->shadowBias,
                      0.5f * mFreyaOptions->shadowBias,
                      static_cast<float>(mCascadeCount),
                      1.5f);

        // ------------------------------------------------------------------
        // Directional CSM: first shadow-casting directional light wins.
        // ------------------------------------------------------------------
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

        if (sun != nullptr)
        {
            computeCascades(*sun, cameraView, cameraProj, nearPlane,
                            drawDistance);
        }

        // ------------------------------------------------------------------
        // Spot shadows: up to mMaxSpotShadows shadow-casting spot lights.
        // ------------------------------------------------------------------
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

        // ------------------------------------------------------------------
        // Point shadows: up to mMaxPointShadows shadow-casting point lights.
        // ------------------------------------------------------------------
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

        mUniformBuffer->Copy(&mShadowData, sizeof(ShadowUniformBuffer));
    }

    void ShadowPass::computeCascades(const Light&     sun,
                                     const glm::mat4& cameraView,
                                     const glm::mat4& cameraProj,
                                     const float      nearPlane,
                                     const float      drawDistance)
    {
        constexpr float lambda = 0.8f;

        std::array<float, MAX_SHADOW_CASCADES> splits {};
        for (std::uint32_t i = 1; i <= mCascadeCount; ++i)
        {
            const auto p =
                static_cast<float>(i) / static_cast<float>(mCascadeCount);

            const auto logSplit =
                nearPlane * std::pow(drawDistance / nearPlane, p);
            const auto uniformSplit =
                nearPlane + (drawDistance - nearPlane) * p;

            splits[i - 1] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
        }

        // Recover vertical FOV / aspect from the (Y-flipped) camera
        // projection matrix so cascade frustum slices match the real
        // camera frustum, regardless of caller conventions.
        const auto tanHalfFovY = 1.0f / std::abs(cameraProj[1][1]);
        const auto aspect      = (cameraProj[1][1] / cameraProj[0][0]) * -1.0f;

        const auto invView  = glm::inverse(cameraView);
        const auto lightDir = glm::normalize(sun.direction);

        const auto up = std::abs(lightDir.y) < 0.99f
                            ? glm::vec3(0.0f, 1.0f, 0.0f)
                            : glm::vec3(1.0f, 0.0f, 0.0f);

        for (std::uint32_t i = 0; i < mCascadeCount; ++i)
        {
            const auto splitNear = (i == 0) ? nearPlane : splits[i - 1];
            const auto splitFar  = splits[i];

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

            glm::vec3                center(0.0f);
            std::array<glm::vec3, 8> worldCorners {};
            for (std::size_t c = 0; c < worldCorners.size(); ++c)
            {
                const auto world = invView * viewCorners[c];
                worldCorners[c]  = glm::vec3(world);
                center += worldCorners[c];
            }
            center /= 8.0f;

            const auto lightView =
                glm::lookAt(center - lightDir * drawDistance, center, up);

            auto minB = glm::vec3(std::numeric_limits<float>::max());
            auto maxB = glm::vec3(std::numeric_limits<float>::lowest());
            for (const auto& worldCorner : worldCorners)
            {
                const auto lightSpace =
                    glm::vec3(lightView * glm::vec4(worldCorner, 1.0f));
                minB = glm::min(minB, lightSpace);
                maxB = glm::max(maxB, lightSpace);
            }

            // Extend the near/far range so casters just outside the
            // visible frustum slice still land inside the ortho box.
            constexpr auto zMult = 10.0f;
            auto           minZ  = minB.z;
            auto           maxZ  = maxB.z;
            minZ                 = (minZ < 0.0f) ? minZ * zMult : minZ / zMult;
            maxZ                 = (maxZ < 0.0f) ? maxZ / zMult : maxZ * zMult;

            const auto near = -maxZ;
            const auto far  = -minZ;

            auto lightProj =
                mFreyaOptions->ReverseZ
                    ? glm::ortho(minB.x, maxB.x, minB.y, maxB.y, far, near)
                    : glm::ortho(minB.x, maxB.x, minB.y, maxB.y, near, far);
            lightProj[1][1] *= -1.0f;

            mShadowData.cascadeViewProj[i] = lightProj * lightView;
            mShadowData.cascadeSplits[i]   = splitFar;
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

        constexpr auto near = 0.1f;
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
        proj[1][1] *= -1.0f;

        return proj * view;
    }

    void ShadowPass::Render(const skr::Arc<CommandPool>& commandPool,
                            const std::function<void()>& drawScene) const
    {
        renderCascades(commandPool, drawScene);
        renderSpots(commandPool, drawScene);
        renderPoints(commandPool, drawScene);
    }

    void ShadowPass::renderCascades(
        const skr::Arc<CommandPool>& commandPool,
        const std::function<void()>& drawScene) const
    {
        auto commandBuffer = commandPool->GetCommandBuffer();

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
            commandBuffer.beginRenderPass(
                vk::RenderPassBeginInfo()
                    .setRenderPass(mRenderPass)
                    .setFramebuffer(mCascadeFramebuffers[i])
                    .setRenderArea(scissor)
                    .setClearValues(clearValue),
                vk::SubpassContents::eInline);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       mPipeline);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);

            const auto lightVP = mShadowData.cascadeViewProj[i];
            commandBuffer.pushConstants(
                mPipelineLayout,
                vk::ShaderStageFlagBits::eVertex,
                0,
                sizeof(glm::mat4),
                &lightVP);

            drawScene();

            commandBuffer.endRenderPass();
        }
    }

    void ShadowPass::renderSpots(const skr::Arc<CommandPool>& commandPool,
                                 const std::function<void()>& drawScene) const
    {
        if (mActiveSpotCount == 0)
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();

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

        for (std::uint32_t i = 0; i < mActiveSpotCount; ++i)
        {
            commandBuffer.beginRenderPass(
                vk::RenderPassBeginInfo()
                    .setRenderPass(mRenderPass)
                    .setFramebuffer(mSpotFramebuffers[i])
                    .setRenderArea(scissor)
                    .setClearValues(clearValue),
                vk::SubpassContents::eInline);

            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       mPipeline);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);

            const auto lightVP = mShadowData.spotViewProj[i];
            commandBuffer.pushConstants(
                mPipelineLayout,
                vk::ShaderStageFlagBits::eVertex,
                0,
                sizeof(glm::mat4),
                &lightVP);

            drawScene();

            commandBuffer.endRenderPass();
        }
    }

    void ShadowPass::renderPoints(const skr::Arc<CommandPool>& commandPool,
                                  const std::function<void()>& drawScene) const
    {
        if (mActivePointCount == 0)
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();

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

        for (std::uint32_t p = 0; p < mActivePointCount; ++p)
        {
            const auto      posFar = mShadowData.pointLightPosFar[p];
            const glm::vec3 position(posFar);
            const auto      far = posFar.w;

            for (std::uint32_t face = 0; face < 6; ++face)
            {
                const auto framebufferIndex = p * 6 + face;

                commandBuffer.beginRenderPass(
                    vk::RenderPassBeginInfo()
                        .setRenderPass(mRenderPass)
                        .setFramebuffer(mPointFramebuffers[framebufferIndex])
                        .setRenderArea(scissor)
                        .setClearValues(clearValue),
                    vk::SubpassContents::eInline);

                commandBuffer.bindPipeline(
                    vk::PipelineBindPoint::eGraphics, mPipeline);
                commandBuffer.setViewport(0, 1, &viewport);
                commandBuffer.setScissor(0, 1, &scissor);

                const auto lightVP =
                    computePointFaceViewProj(position, far, face);
                commandBuffer.pushConstants(
                    mPipelineLayout,
                    vk::ShaderStageFlagBits::eVertex,
                    0,
                    sizeof(glm::mat4),
                    &lightVP);

                drawScene();

                commandBuffer.endRenderPass();
            }
        }
    }

} // namespace FREYA_NAMESPACE
