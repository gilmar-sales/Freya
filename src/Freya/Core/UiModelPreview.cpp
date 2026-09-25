#include "Freya/Core/UiModelPreview.hpp"

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/SceneInstanceUpload.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/IndirectDrawSystemBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Builders/ShadowMaskPassBuilder.hpp"
#include "Freya/Builders/ShadowPassBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/IndirectDrawSystem.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/RenderTarget.hpp"
#include "Freya/Core/ShadowMaskPass.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/SsaoPass.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/TaaPass.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"
#include "Freya/Internal/VulkanCompat.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace FREYA_NAMESPACE
{
    namespace
    {
        void SetFullViewport(const skr::Arc<CommandPool>& commandPool,
                             const vk::Extent2D           extent)
        {
            const auto commandBuffer = commandPool->GetCommandBuffer();
            auto       viewport =
                vk::Viewport()
                    .setX(0)
                    .setY(0)
                    .setWidth(static_cast<float>(extent.width))
                    .setHeight(static_cast<float>(extent.height))
                    .setMinDepth(0.0f)
                    .setMaxDepth(1.0f);
            auto scissor = vk::Rect2D().setOffset({ 0, 0 }).setExtent(extent);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);
        }

        bool IsBgra(const vk::Format format)
        {
            return format == vk::Format::eB8G8R8A8Unorm ||
                   format == vk::Format::eB8G8R8A8Srgb ||
                   format == vk::Format::eB8G8R8A8Snorm ||
                   format == vk::Format::eB8G8R8A8Uint ||
                   format == vk::Format::eA8B8G8R8UnormPack32;
        }

        glm::mat4 MakePreviewProjection(const float fovRadians,
                                        const float aspect, const float nearP,
                                        const float farP, const bool reverseZ)
        {
            auto projection =
                reverseZ ? glm::perspective(fovRadians, aspect, farP, nearP)
                         : glm::perspective(fovRadians, aspect, nearP, farP);
            projection[1][1] *= -1.f;
            return projection;
        }

        float Halton(std::uint32_t index, const std::uint32_t base)
        {
            float f      = 1.0f;
            float result = 0.0f;
            while (index > 0)
            {
                f /= static_cast<float>(base);
                result += f * static_cast<float>(index % base);
                index /= base;
            }
            return result;
        }

        void ApplyHaltonJitter(glm::mat4&          projection,
                               const std::uint32_t frameIndex,
                               const vk::Extent2D  extent,
                               const std::uint32_t haltonPeriod)
        {
            if (extent.width == 0 || extent.height == 0)
                return;
            const auto  period = std::max(1u, haltonPeriod);
            const auto  sample = (frameIndex % period) + 1;
            const float jx     = (Halton(sample, 2) - 0.5f) * 2.0f /
                                 static_cast<float>(extent.width);
            const float jy     = -(Halton(sample, 3) - 0.5f) * 2.0f /
                                 static_cast<float>(extent.height);
            projection[2][0] += jx;
            projection[2][1] += jy;
        }
    } // namespace

    struct UiModelPreview::Impl
    {
        skr::Arc<skr::ServiceProvider> serviceProvider;
        TexturePool*                   texturePool = nullptr;

        skr::Arc<Device>         device;
        skr::Arc<Surface>        surface;
        skr::Arc<PhysicalDevice> physicalDevice;
        skr::Arc<FreyaOptions>   options;
        skr::Arc<CommandPool>    commandPool;

        skr::Arc<RenderTarget>           renderTarget;
        skr::Arc<SwapChain>              swapChain;
        skr::Arc<LightService>           lights;
        skr::Arc<ShadowPass>             shadow;
        skr::Arc<DeferredCompressedPass> deferred;
        skr::Arc<CompositePass>          composite;
        skr::Arc<IndirectDrawSystem>     indirect;
        skr::Arc<SsaoPass>               ssao;
        skr::Arc<ShadowMaskPass>         shadowMask;
        skr::Arc<TaaPass>                taa;
        skr::Arc<BloomPass>              bloom;
        skr::Arc<Image>                  ssaoFallback;
        skr::Arc<Image>                  bloomStub;
        std::vector<skr::Arc<Image>>     bloomResults;
        vk::Sampler                      bloomSampler {};

        Scene               scene;
        UiModelPreviewOrbit orbit {};
        TextureHandle       liveTexture {};
        glm::uvec2          extent { 512, 512 };

        std::atomic<float> yawDeg { 30.f };
        std::atomic<float> pitchDeg { -10.f };
        std::atomic<float> frameDt { 0.f };
        std::atomic<bool>  active { true };
        bool               dragging = false;

        ProjectionUniformBuffer projection {};
        glm::mat4               prevViewProjection { 1.f };
        std::uint32_t           taaFrameIndex = 0;
        float                   cameraNear    = 0.1f;

        void unregisterLive()
        {
            if (texturePool && liveTexture.IsValid())
            {
                texturePool->UnregisterExternal(liveTexture);
                liveTexture = {};
            }
        }

        void registerLive()
        {
            unregisterLive();
            if (!texturePool || !renderTarget)
                return;
            liveTexture = texturePool->RegisterExternalImage(
                reinterpret_cast<void*>(static_cast<VkImageView>(
                    renderTarget->GetColorImageView())),
                reinterpret_cast<void*>(
                    static_cast<VkSampler>(renderTarget->GetSampler())),
                renderTarget->GetExtent().width,
                renderTarget->GetExtent().height);
        }

        skr::Arc<Image> createWhite1x1() const
        {
            constexpr std::uint8_t kWhite[] = { 255, 255, 255, 255 };
            auto                   staging =
                BufferBuilder(device)
                    .SetUsage(BufferUsage::Staging)
                    .SetSize(sizeof(kWhite))
                    .SetData(const_cast<std::uint8_t*>(kWhite))
                    .Build();
            return serviceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Texture)
                .SetFormat(vk::Format::eR8G8B8A8Unorm)
                .SetWidth(1)
                .SetHeight(1)
                .SetChannels(4)
                .SetStagingBuffer(staging)
                .SetData(const_cast<std::uint8_t*>(kWhite))
                .Build();
        }

        skr::Arc<Image> createBlack1x1() const
        {
            constexpr std::uint8_t kBlack[] = { 0, 0, 0, 0 };
            auto                   staging =
                BufferBuilder(device)
                    .SetUsage(BufferUsage::Staging)
                    .SetSize(sizeof(kBlack))
                    .SetData(const_cast<std::uint8_t*>(kBlack))
                    .Build();
            return serviceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Texture)
                .SetFormat(vk::Format::eR8G8B8A8Unorm)
                .SetWidth(1)
                .SetHeight(1)
                .SetChannels(4)
                .SetStagingBuffer(staging)
                .SetData(const_cast<std::uint8_t*>(kBlack))
                .Build();
        }

        void destroyPasses()
        {
            if (device && bloomSampler)
            {
                device->Get().destroySampler(bloomSampler);
                bloomSampler = nullptr;
            }
            deferred.reset();
            shadow.reset();
            composite.reset();
            indirect.reset();
            ssao.reset();
            shadowMask.reset();
            taa.reset();
            bloom.reset();
            bloomResults.clear();
            ssaoFallback.reset();
            bloomStub.reset();
            taaFrameIndex      = 0;
            prevViewProjection = glm::mat4(1.f);
        }

        void destroyGpu()
        {
            unregisterLive();
            destroyPasses();
            renderTarget.reset();
            swapChain.reset();
        }

        void rebuildTarget(const glm::uvec2 newExtent)
        {
            extent = { std::max(1u, newExtent.x), std::max(1u, newExtent.y) };
            if (device)
                device->Get().waitIdle();

            unregisterLive();
            destroyPasses();
            renderTarget.reset();

            renderTarget = serviceProvider->GetService<RenderTargetBuilder>()
                               ->SetWidth(extent.x)
                               .SetHeight(extent.y)
                               .Build();

            if (!lights)
            {
                lights = skr::MakeArc<LightService>(serviceProvider);
                lights->AddLight(MakeDirectionalLight(
                    glm::normalize(glm::vec3(-0.35f, -1.f, -0.25f)),
                    glm::vec3(1.f, 0.97f, 0.92f), 2.5f));
            }

            projection.ambientLight =
                glm::vec4(options->ambientColor, options->ambientIntensity);
            cameraNear = orbit.nearPlane;
            registerLive();
        }

        void ensurePasses(const skr::Arc<SwapChain>& sc)
        {
            if (!sc || !renderTarget)
                return;
            // Keep passes across window resize (swapchain Arc changes).
            // Rebuild only when missing (first use / after preview Resize).
            if (deferred && composite && shadow && indirect)
            {
                swapChain = sc;
                ensurePostPasses();
                return;
            }

            if (device)
                device->Get().waitIdle();
            destroyPasses();
            swapChain = sc;

            auto bones = serviceProvider->GetService<BoneMatrixResources>();
            auto materials =
                serviceProvider->GetService<MaterialDescriptorResources>();
            auto ibl      = serviceProvider->GetService<IBLService>();
            auto meshPool = serviceProvider->GetService<MeshPool>();

            shadow = ShadowPassBuilder(device, physicalDevice, options,
                                       serviceProvider, bones, materials)
                         .Build();

            deferred =
                DeferredCompressedPassBuilder(
                    device, physicalDevice, surface, options, serviceProvider,
                    lights, ibl, shadow, materials, bones)
                    .Build(sc, vk::Extent2D { extent.x, extent.y });

            composite =
                serviceProvider->GetService<CompositePassBuilder>()->Build(sc);

            indirect = IndirectDrawSystemBuilder(
                           device, physicalDevice, commandPool, meshPool,
                           materials, options, serviceProvider)
                           .Build();
            if (indirect)
                indirect->ResizeHiZ(vk::Extent2D { extent.x, extent.y });

            ssaoFallback = createWhite1x1();
            bloomStub    = createBlack1x1();
            bloomSampler = device->Get().createSampler(
                vk::SamplerCreateInfo()
                    .setMagFilter(vk::Filter::eLinear)
                    .setMinFilter(vk::Filter::eLinear)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                    .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                    .setAddressModeW(vk::SamplerAddressMode::eClampToEdge));

            ensurePostPasses();
        }

        void ensurePostPasses()
        {
            if (!swapChain || !deferred || !options)
                return;

            const vk::Extent2D vkExtent { extent.x, extent.y };

            if (options->enableSsao)
            {
                if (!ssao)
                    ssao =
                        serviceProvider->GetService<SsaoPassBuilder>()->Build(
                            swapChain, vkExtent);
            }
            else
                ssao.reset();

            if (options->enableShadows && options->enableShadowMask)
            {
                if (!shadowMask)
                    shadowMask =
                        serviceProvider->GetService<ShadowMaskPassBuilder>()
                            ->Build(swapChain, vkExtent);
            }
            else
                shadowMask.reset();

            if (options->enableTaa)
            {
                if (!taa)
                {
                    taa = serviceProvider->GetService<TaaPassBuilder>()->Build(
                        swapChain, vkExtent);
                    if (taa)
                        taa->ResetHistory();
                    taaFrameIndex      = 0;
                    prevViewProjection = glm::mat4(1.f);
                }
            }
            else
                taa.reset();

            if (options->enableBloom)
            {
                if (!bloom)
                {
                    bloom =
                        serviceProvider->GetService<BloomPassBuilder>()->Build(
                            swapChain,
                            deferred->GetSceneColorImage(),
                            vkExtent);
                    bloomResults.clear();
                    bloomResults.resize(options->frameCount);
                    for (std::uint32_t i = 0; i < options->frameCount; ++i)
                    {
                        bloomResults[i] =
                            serviceProvider->GetService<ImageBuilder>()
                                ->SetUsage(ImageUsage::Color)
                                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                                .SetWidth(extent.x)
                                .SetHeight(extent.y)
                                .SetSamples(vk::SampleCountFlagBits::e1)
                                .Build();
                    }
                }
            }
            else
            {
                bloom.reset();
                bloomResults.clear();
            }
        }

        void updateOrbit(const float dt)
        {
            if (orbit.autoRotate && !dragging && orbit.enabled)
            {
                const float yaw = yawDeg.load(std::memory_order_relaxed) +
                                  orbit.autoSpeed * dt;
                yawDeg.store(yaw, std::memory_order_relaxed);
                orbit.yawDeg = yaw;
            }
            orbit.yawDeg   = yawDeg.load(std::memory_order_relaxed);
            orbit.pitchDeg = pitchDeg.load(std::memory_order_relaxed);
        }

        void applyCamera()
        {
            const float yawRad =
                glm::radians(yawDeg.load(std::memory_order_relaxed));
            const float pitchRad =
                glm::radians(pitchDeg.load(std::memory_order_relaxed));
            const float dist = std::max(0.05f, orbit.distance);

            const glm::vec3 offset {
                dist * std::cos(pitchRad) * std::sin(yawRad),
                dist * std::sin(pitchRad),
                dist * std::cos(pitchRad) * std::cos(yawRad)
            };
            const glm::vec3 eye = orbit.target + offset;
            const glm::vec3 up  = { 0.f, 1.f, 0.f };
            const float aspect = extent.y > 0 ? static_cast<float>(extent.x) /
                                                    static_cast<float>(extent.y)
                                              : 1.f;

            projection.view       = glm::lookAt(eye, orbit.target, up);
            projection.projection = MakePreviewProjection(
                glm::radians(orbit.fovDegrees), aspect, orbit.nearPlane,
                orbit.farPlane, options->ReverseZ);
            projection.unjitteredProjection = projection.projection;
            projection.prevViewProjection   = prevViewProjection;
            if (options->enableTaa && taa)
            {
                ApplyHaltonJitter(projection.projection, taaFrameIndex,
                                  vk::Extent2D { extent.x, extent.y },
                                  options->taaHaltonPeriod);
            }
            projection.invViewProjection =
                glm::inverse(projection.projection * projection.view);
            cameraNear = orbit.nearPlane;

            const auto forward = glm::normalize(orbit.target - eye);
            lights->Update(0, eye, forward);
        }

        void blitBloomToFullRes(const skr::Arc<CommandPool>& cmdPool,
                                const std::uint32_t          frameIndex)
        {
            if (!bloom || frameIndex >= bloomResults.size() ||
                !bloomResults[frameIndex])
                return;

            auto bloomUp     = bloom->GetBloomUpImage(frameIndex);
            auto bloomResult = bloomResults[frameIndex];
            if (!bloomUp || !bloomResult)
                return;

            auto       commandBuffer = cmdPool->GetCommandBuffer();
            const auto vkExtent      = vk::Extent2D { extent.x, extent.y };
            const auto bloomExtent =
                ScaledExtent(vkExtent, options->bloomResolutionDivisor);
            const auto srcW = static_cast<std::int32_t>(bloomExtent.width);
            const auto srcH = static_cast<std::int32_t>(bloomExtent.height);

            const auto range =
                vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1);

            auto srcBarrier =
                vk::ImageMemoryBarrier()
                    .setImage(bloomUp->GetImage())
                    .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                    .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                    .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                    .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                    .setSubresourceRange(range);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr,
                srcBarrier);

            auto dstBarrier =
                vk::ImageMemoryBarrier()
                    .setImage(bloomResult->GetImage())
                    .setSrcAccessMask({})
                    .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                    .setOldLayout(vk::ImageLayout::eUndefined)
                    .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                    .setSubresourceRange(range);
            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                          vk::PipelineStageFlagBits::eTransfer,
                                          {}, nullptr, nullptr, dstBarrier);

            auto blit =
                vk::ImageBlit {}
                    .setSrcSubresource(
                        vk::ImageSubresourceLayers {}
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setMipLevel(0)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1))
                    .setDstSubresource(
                        vk::ImageSubresourceLayers {}
                            .setAspectMask(vk::ImageAspectFlagBits::eColor)
                            .setMipLevel(0)
                            .setBaseArrayLayer(0)
                            .setLayerCount(1));
            blit.setSrcOffsets(
                { vk::Offset3D { 0, 0, 0 }, vk::Offset3D { srcW, srcH, 1 } });
            blit.setDstOffsets(
                { vk::Offset3D { 0, 0, 0 },
                  vk::Offset3D { static_cast<std::int32_t>(extent.x),
                                 static_cast<std::int32_t>(extent.y), 1 } });

            commandBuffer.blitImage(
                bloomUp->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                bloomResult->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                1, &blit, vk::Filter::eLinear);

            srcBarrier.setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr,
                nullptr, srcBarrier);

            dstBarrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead);
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr,
                nullptr, dstBarrier);
        }

        void uploadScene(const std::uint32_t frameIndex)
        {
            if (!indirect)
                return;

            std::vector<SceneInstanceUpload> uploads;
            uploads.reserve(scene.Size());
            scene.ForEach([&](Scene::InstanceId, const Scene::Instance& inst) {
                uploads.push_back(SceneInstanceUpload {
                    .transform   = inst.transform,
                    .mesh        = inst.mesh,
                    .material    = inst.material,
                    .entityId    = inst.entityId,
                    .techniqueId = inst.techniqueId,
                    .flags       = inst.flags,
                    .boneOffset  = inst.boneOffset,
                    .boneCount   = inst.boneCount,
                });
            });

            if (uploads.empty())
            {
                indirect->CommitSceneFrame(frameIndex);
                return;
            }

            indirect->BeginSceneInstances();
            indirect->ReserveSceneInstances(
                static_cast<std::uint32_t>(uploads.size()));
            indirect->UploadSceneInstances(uploads);
            indirect->EndSceneInstances(frameIndex);
            indirect->SyncMeshInfo();
        }

        void clearOnly(const skr::Arc<CommandPool>& cmdPool)
        {
            if (!renderTarget || !composite)
                return;
            const auto clear =
                ToVkClearValue(glm::vec4(0.12f, 0.13f, 0.15f, 1.f));
            composite->Begin(renderTarget->GetRenderPass(),
                             renderTarget->GetFramebuffer(),
                             renderTarget->GetExtent(), cmdPool, clear);
            composite->End(cmdPool);
        }

        void recordDeferred(const skr::Arc<CommandPool>& cmdPool,
                            const skr::Arc<SwapChain>&   sc,
                            const std::uint32_t          frameIndex)
        {
            ensurePasses(sc);
            if (!deferred || !shadow || !indirect || !composite ||
                !renderTarget || !ssaoFallback || !bloomStub)
            {
                clearOnly(cmdPool);
                return;
            }

            const vk::Extent2D vkExtent { extent.x, extent.y };
            const glm::mat4    viewProj =
                projection.unjitteredProjection * projection.view;
            const glm::vec3 cameraPos =
                glm::vec3(glm::inverse(projection.view)[3]);

            deferred->UpdateProjection(projection, frameIndex);
            lights->Update(frameIndex, cameraPos,
                           glm::normalize(orbit.target - cameraPos));

            uploadScene(frameIndex);

            if (options->enableShadows)
            {
                shadow->Update(*lights, projection.view,
                               projection.unjitteredProjection, cameraPos,
                               cameraNear, orbit.farPlane, frameIndex);
                shadow->Render(
                    cmdPool,
                    [&](const glm::mat4& lightVP) {
                        indirect->SetCullView(cameraPos, vkExtent);
                        indirect->DispatchCull(
                            lightVP, CullMode::Shadow, options->ReverseZ,
                            kTechniqueFilterAll);
                    },
                    [&]() {
                        indirect->ExecuteDraws(
                            true, shadow->GetPipelineLayout(),
                            kTechniqueFilterAll);
                    });
            }

            const std::uint32_t usedMask = indirect->UsedTechniqueMask();

            indirect->SetCullView(cameraPos, vkExtent);
            indirect->DispatchCull(viewProj, CullMode::Camera,
                                   options->ReverseZ, kTechniqueFilterAll);
            for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
            {
                if ((usedMask & (1u << t)) == 0)
                    continue;
                indirect->DispatchCull(
                    viewProj, CullMode::Camera, options->ReverseZ, t);
            }

            deferred->Begin(sc, cmdPool);
            SetFullViewport(cmdPool, vkExtent);

            auto currentSubpass = deferred->GetCurrentSubpass();
            if (currentSubpass == DefDepthPrePass)
            {
                indirect->ExecuteDraws(
                    false, deferred->GetVertexPipelineLayout(),
                    kTechniqueFilterAll);
                deferred->NextSubpass(cmdPool);

                bool drew = false;
                for (std::uint32_t t = 0; t < kMaxMaterialTechniques; ++t)
                {
                    if ((usedMask & (1u << t)) == 0)
                        continue;
                    deferred->BindGBufferTechnique(t, cmdPool, frameIndex);
                    indirect->ExecuteDraws(
                        true, deferred->GetVertexPipelineLayout(), t);
                    drew = true;
                }
                if (!drew)
                    deferred->BindGBufferTechnique(0, cmdPool, frameIndex);
            }
            deferred->End(cmdPool);

            indirect->BuildHiZ(deferred->GetDepthImage(), options->ReverseZ);

            if (ssao)
            {
                ssao->Dispatch(
                    cmdPool, deferred->GetDepthImage(),
                    deferred->GetNormalImage(), projection.view,
                    projection.unjitteredProjection, options->ReverseZ,
                    options->ssaoRadius, options->ssaoBias, options->ssaoPower,
                    options->ssaoIntensity);
            }

            if (shadowMask && options->enableShadows &&
                options->enableShadowMask)
            {
                shadowMask->Dispatch(
                    cmdPool, deferred->GetDepthImage(),
                    deferred->GetNormalImage(), shadow, *lights,
                    projection.view, projection.unjitteredProjection,
                    options->ReverseZ, frameIndex);
            }

            auto ssaoImage = ssao ? ssao->GetOutputImage() : ssaoFallback;
            auto maskImage =
                shadowMask ? shadowMask->GetOutputImage() : ssaoFallback;
            if (!ssaoImage)
                ssaoImage = ssaoFallback;
            if (!maskImage)
                maskImage = ssaoFallback;

            deferred->BeginLighting(cmdPool, ssaoImage, maskImage, frameIndex);
            SetFullViewport(cmdPool, vkExtent);
            deferred->DrawLighting(cmdPool, frameIndex, 0);
            deferred->EndLighting(cmdPool);

            if (taa)
            {
                taa->Dispatch(cmdPool, deferred->GetSceneColorImage(),
                              deferred->GetVelocityImage(),
                              deferred->GetDepthImage());
            }

            if (bloom)
            {
                // Bloom samples pre-TAA scene color (same as main path).
                const auto bloomExtent =
                    ScaledExtent(vkExtent, options->bloomResolutionDivisor);
                auto commandBuffer = cmdPool->GetCommandBuffer();
                auto bloomViewport =
                    vk::Viewport()
                        .setX(0)
                        .setY(0)
                        .setWidth(static_cast<float>(bloomExtent.width))
                        .setHeight(static_cast<float>(bloomExtent.height))
                        .setMinDepth(0.0f)
                        .setMaxDepth(1.0f);
                auto bloomScissor =
                    vk::Rect2D().setOffset({ 0, 0 }).setExtent(bloomExtent);
                commandBuffer.setViewport(0, 1, &bloomViewport);
                commandBuffer.setScissor(0, 1, &bloomScissor);

                bloom->Begin(cmdPool, frameIndex);
                bloom->DrawFullscreenTriangle(cmdPool);
                bloom->AdvanceSubpass(BloomDownsampleSubpass, cmdPool,
                                      frameIndex);
                bloom->DrawFullscreenTriangle(cmdPool);
                bloom->AdvanceSubpass(BloomUpsampleSubpass, cmdPool,
                                      frameIndex);
                bloom->DrawFullscreenTriangle(cmdPool);
                bloom->End(cmdPool);
                blitBloomToFullRes(cmdPool, frameIndex);
            }

            skr::Arc<Image> sceneColor =
                taa ? taa->GetOutputImage() : deferred->GetSceneColorImage();
            skr::Arc<Image> bloomColor = bloomStub;
            if (bloom && frameIndex < bloomResults.size() &&
                bloomResults[frameIndex])
                bloomColor = bloomResults[frameIndex];

            composite->UpdateDescriptorSet(
                frameIndex, sceneColor, bloomColor, bloomSampler);
            SetFullViewport(cmdPool, vkExtent);
            composite->Begin(
                renderTarget->GetRenderPass(), renderTarget->GetFramebuffer(),
                renderTarget->GetExtent(), cmdPool,
                ToVkClearValue(options->clearColor));
            composite->BindPipeline(cmdPool, frameIndex);
            composite->DrawFullscreenTriangle(cmdPool, 1.0f);
            composite->End(cmdPool);

            prevViewProjection = projection.projection * projection.view;
            if (options->enableTaa && taa)
                ++taaFrameIndex;
        }
    };

    UiModelPreview::UiModelPreview(
        const skr::Arc<skr::ServiceProvider>& serviceProvider,
        TexturePool& texturePool, const glm::uvec2 extent) :
        mImpl(std::make_unique<Impl>())
    {
        mImpl->serviceProvider = serviceProvider;
        mImpl->texturePool     = &texturePool;
        mImpl->device          = serviceProvider->GetService<Device>();
        mImpl->surface         = serviceProvider->GetService<Surface>();
        mImpl->physicalDevice  = serviceProvider->GetService<PhysicalDevice>();
        mImpl->options         = serviceProvider->GetService<FreyaOptions>();
        mImpl->commandPool     = serviceProvider->GetService<CommandPool>();
        mImpl->yawDeg.store(mImpl->orbit.yawDeg, std::memory_order_relaxed);
        mImpl->pitchDeg.store(mImpl->orbit.pitchDeg, std::memory_order_relaxed);
        mImpl->rebuildTarget(extent);
    }

    UiModelPreview::~UiModelPreview()
    {
        if (mImpl && mImpl->device)
            mImpl->device->Get().waitIdle();
        if (mImpl)
            mImpl->destroyGpu();
    }

    void UiModelPreview::Resize(const glm::uvec2 extent)
    {
        if (extent.x == mImpl->extent.x && extent.y == mImpl->extent.y)
            return;
        const auto sc = mImpl->swapChain;
        mImpl->rebuildTarget(extent);
        if (sc)
            mImpl->ensurePasses(sc);
    }

    TextureHandle UiModelPreview::Texture() const
    {
        return mImpl->liveTexture;
    }

    glm::uvec2 UiModelPreview::Extent() const
    {
        return mImpl->extent;
    }

    UiModelPreviewOrbit& UiModelPreview::Orbit()
    {
        return mImpl->orbit;
    }

    const UiModelPreviewOrbit& UiModelPreview::Orbit() const
    {
        return mImpl->orbit;
    }

    void UiModelPreview::SetOrbit(const UiModelPreviewOrbit& orbit)
    {
        mImpl->orbit = orbit;
        mImpl->yawDeg.store(orbit.yawDeg, std::memory_order_relaxed);
        mImpl->pitchDeg.store(orbit.ClampPitch(orbit.pitchDeg),
                              std::memory_order_relaxed);
    }

    Scene& UiModelPreview::PreviewScene()
    {
        return mImpl->scene;
    }

    const Scene& UiModelPreview::PreviewScene() const
    {
        return mImpl->scene;
    }

    LightService& UiModelPreview::Lights()
    {
        return *mImpl->lights;
    }

    const LightService& UiModelPreview::Lights() const
    {
        return *mImpl->lights;
    }

    void UiModelPreview::FeedMouseMove(const float deltaX, const float deltaY)
    {
        if (!mImpl->orbit.enabled || !mImpl->dragging)
            return;
        const float sens = mImpl->orbit.sensitivity;
        float       yaw =
            mImpl->yawDeg.load(std::memory_order_relaxed) + deltaX * sens;
        float pitch =
            mImpl->pitchDeg.load(std::memory_order_relaxed) - deltaY * sens;
        pitch = mImpl->orbit.ClampPitch(pitch);
        mImpl->yawDeg.store(yaw, std::memory_order_relaxed);
        mImpl->pitchDeg.store(pitch, std::memory_order_relaxed);
        mImpl->orbit.yawDeg   = yaw;
        mImpl->orbit.pitchDeg = pitch;
    }

    void UiModelPreview::FeedMouseButton(const MouseButton button,
                                         const bool        down)
    {
        if (!mImpl->orbit.enabled)
            return;
        if (button != mImpl->orbit.dragButton)
            return;
        mImpl->dragging = down;
    }

    void UiModelPreview::SetFrameDelta(const float dt)
    {
        mImpl->frameDt.store(dt, std::memory_order_relaxed);
    }

    void UiModelPreview::SetActive(const bool active)
    {
        const bool was =
            mImpl->active.exchange(active, std::memory_order_relaxed);
        if (was && !active)
        {
            // Drop the mini deferred/shadow stack while hidden so maximize
            // does not compete for VRAM with a second full GPU path.
            if (mImpl->device)
                mImpl->device->Get().waitIdle();
            mImpl->destroyPasses();
            mImpl->swapChain.reset();
        }
    }

    bool UiModelPreview::IsActive() const
    {
        return mImpl->active.load(std::memory_order_relaxed);
    }

    void UiModelPreview::Record(const skr::Arc<CommandPool>& commandPool,
                                const skr::Arc<SwapChain>&   swapChain,
                                const std::uint32_t          frameIndex)
    {
        if (!mImpl->renderTarget)
            return;
        if (!mImpl->active.load(std::memory_order_relaxed))
            return;

        const float dt =
            mImpl->frameDt.exchange(0.f, std::memory_order_relaxed);
        mImpl->updateOrbit(dt);
        mImpl->applyCamera();
        mImpl->recordDeferred(commandPool, swapChain, frameIndex);
    }

    TextureHandle UiModelPreview::CaptureSnapshot(TexturePool&     pool,
                                                  const glm::uvec2 size)
    {
        if (!mImpl->renderTarget || !mImpl->device || !mImpl->commandPool)
            return {};

        const auto color = mImpl->renderTarget->GetColorImage();
        if (!color)
            return {};

        const auto srcW = mImpl->renderTarget->GetExtent().width;
        const auto srcH = mImpl->renderTarget->GetExtent().height;
        const auto fmt  = mImpl->renderTarget->GetFormat();
        const auto dstW = size.x > 0 ? size.x : srcW;
        const auto dstH = size.y > 0 ? size.y : srcH;

        mImpl->device->Get().waitIdle();

        const std::uint64_t byteSize =
            static_cast<std::uint64_t>(srcW) * srcH * 4u;
        auto staging = BufferBuilder(mImpl->device)
                           .SetUsage(BufferUsage::Readback)
                           .SetSize(byteSize)
                           .Build();

        auto cmd = mImpl->commandPool->CreateCommandBuffer();
        cmd.begin(vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

        const auto range =
            vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0)
                .setLevelCount(1)
                .setBaseArrayLayer(0)
                .setLayerCount(1);

        auto toSrc =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setImage(color->GetImage())
                .setSubresourceRange(range);
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                            vk::PipelineStageFlagBits::eTransfer, {}, nullptr,
                            nullptr, toSrc);

        const auto region =
            vk::BufferImageCopy()
                .setBufferOffset(0)
                .setImageSubresource(
                    vk::ImageSubresourceLayers()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setMipLevel(0)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1))
                .setImageExtent(vk::Extent3D { srcW, srcH, 1 });
        cmd.copyImageToBuffer(
            color->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
            staging->Get(), 1, &region);

        auto toSample =
            vk::ImageMemoryBarrier()
                .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
                .setImage(color->GetImage())
                .setSubresourceRange(range);
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                            vk::PipelineStageFlagBits::eFragmentShader, {},
                            nullptr, nullptr, toSample);

        cmd.end();

        vk::SubmitInfo submit;
        submit.setCommandBuffers(cmd);
        mImpl->device->SubmitAndWait(mImpl->device->GetGraphicsQueue(), submit);
        mImpl->commandPool->FreeCommandBuffer(cmd);

        const auto* src =
            static_cast<const std::uint8_t*>(staging->GetMapped());
        if (!src)
            return {};

        std::vector<std::uint8_t> rgba(
            static_cast<std::size_t>(srcW) * srcH * 4u);
        const bool bgra = IsBgra(fmt);
        for (std::uint32_t i = 0; i < srcW * srcH; ++i)
        {
            const auto* p = src + i * 4;
            if (bgra)
            {
                rgba[i * 4 + 0] = p[2];
                rgba[i * 4 + 1] = p[1];
                rgba[i * 4 + 2] = p[0];
                rgba[i * 4 + 3] = p[3];
            }
            else
            {
                std::memcpy(rgba.data() + i * 4, p, 4);
            }
        }

        if (dstW == srcW && dstH == srcH)
        {
            return pool.CreateTextureFromMemory(rgba.data(), srcW, srcH, 4, 1);
        }

        std::vector<std::uint8_t> scaled(
            static_cast<std::size_t>(dstW) * dstH * 4u);
        for (std::uint32_t y = 0; y < dstH; ++y)
        {
            const auto sy = y * srcH / dstH;
            for (std::uint32_t x = 0; x < dstW; ++x)
            {
                const auto sx  = x * srcW / dstW;
                const auto si  = (sy * srcW + sx) * 4u;
                const auto di  = (y * dstW + x) * 4u;
                scaled[di + 0] = rgba[si + 0];
                scaled[di + 1] = rgba[si + 1];
                scaled[di + 2] = rgba[si + 2];
                scaled[di + 3] = rgba[si + 3];
            }
        }
        return pool.CreateTextureFromMemory(scaled.data(), dstW, dstH, 4, 1);
    }

} // namespace FREYA_NAMESPACE
