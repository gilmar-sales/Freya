#include "FsrUpscalePass.hpp"

#include "Freya/Core/DebugLabels.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace FREYA_NAMESPACE
{
    namespace
    {
        void FfxMessageSink(FfxMsgType type, const wchar_t* message)
        {
            std::fwprintf(stderr, L"[FSR] %ls%ls\n",
                          type == FFX_MESSAGE_TYPE_ERROR ? L"ERROR: " : L"",
                          message ? message : L"(null)");
        }

        /// FFX VK backend loads several promoted KHR entry points by the
        /// extension name. Mesa/Intel often export only the core 1.1+ alias,
        /// leaving the KHR pointer null and crashing in CreateBackendContext.
        PFN_vkVoidFunction VKAPI_PTR FreyaFfxGetDeviceProcAddr(VkDevice device,
                                                               const char* name)
        {
            if (auto* proc = vkGetDeviceProcAddr(device, name))
                return proc;

            if (std::strcmp(name, "vkGetBufferMemoryRequirements2KHR") == 0)
                return vkGetDeviceProcAddr(device,
                                           "vkGetBufferMemoryRequirements2");
            if (std::strcmp(name, "vkGetImageMemoryRequirements2KHR") == 0)
                return vkGetDeviceProcAddr(device,
                                           "vkGetImageMemoryRequirements2");
            if (std::strcmp(name, "vkBindBufferMemory2KHR") == 0)
                return vkGetDeviceProcAddr(device, "vkBindBufferMemory2");
            if (std::strcmp(name, "vkBindImageMemory2KHR") == 0)
                return vkGetDeviceProcAddr(device, "vkBindImageMemory2");
            if (std::strcmp(name, "vkCmdPipelineBarrier2KHR") == 0)
                return vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2");
            if (std::strcmp(name, "vkQueueSubmit2KHR") == 0)
                return vkGetDeviceProcAddr(device, "vkQueueSubmit2");

            return nullptr;
        }

        void Transition(VkCommandBuffer cmd, VkImage image,
                        VkImageLayout oldLayout, VkImageLayout newLayout,
                        VkImageAspectFlags aspect)
        {
            VkImageMemoryBarrier b {};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcAccessMask =
                VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
            b.dstAccessMask =
                VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
            b.oldLayout                       = oldLayout;
            b.newLayout                       = newLayout;
            b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            b.image                           = image;
            b.subresourceRange.aspectMask     = aspect;
            b.subresourceRange.baseMipLevel   = 0;
            b.subresourceRange.levelCount     = 1;
            b.subresourceRange.baseArrayLayer = 0;
            b.subresourceRange.layerCount     = 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                 nullptr, 0, nullptr, 1, &b);
        }
    } // namespace

    FfxFsr3UpscalerQualityMode FsrUpscalePass::ToFfxQuality(FsrQuality quality)
    {
        switch (quality)
        {
            case FsrQuality::NativeAA:
                return FFX_FSR3UPSCALER_QUALITY_MODE_NATIVEAA;
            case FsrQuality::Quality:
                return FFX_FSR3UPSCALER_QUALITY_MODE_QUALITY;
            case FsrQuality::Balanced:
                return FFX_FSR3UPSCALER_QUALITY_MODE_BALANCED;
            case FsrQuality::Performance:
                return FFX_FSR3UPSCALER_QUALITY_MODE_PERFORMANCE;
            case FsrQuality::UltraPerformance:
                return FFX_FSR3UPSCALER_QUALITY_MODE_ULTRA_PERFORMANCE;
            case FsrQuality::Off:
                return FFX_FSR3UPSCALER_QUALITY_MODE_QUALITY;
        }
        return FFX_FSR3UPSCALER_QUALITY_MODE_QUALITY;
    }

    vk::Extent2D FsrUpscalePass::QueryRenderExtent(vk::Extent2D display,
                                                   FsrQuality   quality)
    {
        if (quality == FsrQuality::Off || display.width == 0 ||
            display.height == 0)
            return display;

        uint32_t   renderW = display.width;
        uint32_t   renderH = display.height;
        const auto err     = ffxFsr3UpscalerGetRenderResolutionFromQualityMode(
            &renderW, &renderH, display.width, display.height,
            ToFfxQuality(quality));
        if (err != FFX_OK)
            return display;
        return vk::Extent2D { std::max(1u, renderW), std::max(1u, renderH) };
    }

    FsrUpscalePass::FsrUpscalePass(
        const skr::Arc<Device>&         device,
        const skr::Arc<PhysicalDevice>& physicalDevice,
        const skr::Arc<FreyaOptions>&   freyaOptions,
        skr::Arc<Image>
                     outputImage,
        vk::Extent2D renderExtent,
        vk::Extent2D displayExtent) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mFreyaOptions(freyaOptions), mOutputImage(std::move(outputImage)),
        mRenderExtent(renderExtent), mDisplayExtent(displayExtent)
    {
        if (!createContext())
        {
            std::fprintf(stderr,
                         "[Freya] FSR: context creation failed — pass is "
                         "inert.\n");
        }
    }

    FsrUpscalePass::~FsrUpscalePass()
    {
        destroyContext();
    }

    void FsrUpscalePass::destroyContext()
    {
        if (!mContextReady)
            return;

        if (mBackendInterface.fpDestroyResource)
        {
            mBackendInterface.fpDestroyResource(
                &mBackendInterface, mDilatedDepth, 0);
            mBackendInterface.fpDestroyResource(
                &mBackendInterface, mDilatedMotionVectors, 0);
            mBackendInterface.fpDestroyResource(
                &mBackendInterface, mReconstructedPrevNearestDepth, 0);
        }

        ffxFsr3UpscalerContextDestroy(&mContext);
        std::memset(&mContext, 0, sizeof(mContext));
        std::memset(&mBackendInterface, 0, sizeof(mBackendInterface));
        mScratchBuffer.clear();
        mContextReady = false;
    }

    bool FsrUpscalePass::createContext()
    {
        destroyContext();

        if (mDisplayExtent.width == 0 || mDisplayExtent.height == 0)
            return false;

        // DethRaid/FidelityFX-SDK-Linux routes instance-level queries through
        // VkDeviceContext::instanceFunctions (null → SEGV in scratch size /
        // CreateBackendContext). Populate before ffxGetDeviceVK.
        VkDeviceContext deviceContext {};
        deviceContext.vkDevice         = mDevice->Get();
        deviceContext.vkPhysicalDevice = mPhysicalDevice->Get();
        deviceContext.vkDeviceProcAddr = FreyaFfxGetDeviceProcAddr;
        deviceContext.instanceFunctions.getPhysicalDeviceFeatures2 =
            vkGetPhysicalDeviceFeatures2;
        deviceContext.instanceFunctions.enumerateDeviceExtensionProperties =
            vkEnumerateDeviceExtensionProperties;
        deviceContext.instanceFunctions.getPhysicalDeviceMemoryProperties =
            vkGetPhysicalDeviceMemoryProperties;
        deviceContext.instanceFunctions.getPhysicalDeviceProperties2 =
            vkGetPhysicalDeviceProperties2;

        // Scratch sizing reads sVkDeviceContext.instanceFunctions — require
        // ffxGetDeviceVK first (same order as ffx-api CreateBackend).
        const FfxDevice ffxDevice = ffxGetDeviceVK(&deviceContext);
        const auto      scratchSize =
            ffxGetScratchMemorySizeVK(deviceContext.vkPhysicalDevice,
                                      FFX_FSR3UPSCALER_CONTEXT_COUNT);
        mScratchBuffer.assign(scratchSize, 0);

        if (ffxGetInterfaceVK(&mBackendInterface, ffxDevice,
                              mScratchBuffer.data(), mScratchBuffer.size(),
                              FFX_FSR3UPSCALER_CONTEXT_COUNT) != FFX_OK)
            return false;

        FfxFsr3UpscalerContextDescription desc {};
        desc.flags = FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE |
                     FFX_FSR3UPSCALER_ENABLE_AUTO_EXPOSURE |
                     FFX_FSR3UPSCALER_ENABLE_DYNAMIC_RESOLUTION;
        if (mFreyaOptions->ReverseZ)
            desc.flags |= FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED;
        desc.maxRenderSize    = { mDisplayExtent.width, mDisplayExtent.height };
        desc.maxUpscaleSize   = { mDisplayExtent.width, mDisplayExtent.height };
        desc.fpMessage        = FfxMessageSink;
        desc.backendInterface = mBackendInterface;

        if (ffxFsr3UpscalerContextCreate(&mContext, &desc) != FFX_OK)
            return false;

        FfxFsr3UpscalerSharedResourceDescriptions shared {};
        if (ffxFsr3UpscalerGetSharedResourceDescriptions(&mContext, &shared) !=
            FFX_OK)
        {
            ffxFsr3UpscalerContextDestroy(&mContext);
            return false;
        }

        if (mBackendInterface.fpCreateResource(
                &mBackendInterface, &shared.dilatedDepth, 0, &mDilatedDepth) !=
                FFX_OK ||
            mBackendInterface.fpCreateResource(
                &mBackendInterface, &shared.dilatedMotionVectors, 0,
                &mDilatedMotionVectors) != FFX_OK ||
            mBackendInterface.fpCreateResource(
                &mBackendInterface, &shared.reconstructedPrevNearestDepth, 0,
                &mReconstructedPrevNearestDepth) != FFX_OK)
        {
            ffxFsr3UpscalerContextDestroy(&mContext);
            return false;
        }

        mContextReady      = true;
        mResetNextDispatch = true;
        return true;
    }

    FfxResource FsrUpscalePass::wrapColor(VkImage image, VkFormat format,
                                          uint32_t w, uint32_t h, bool storage,
                                          FfxResourceStates state) const
    {
        VkImageCreateInfo ci {};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = format;
        ci.extent      = { w, h, 1 };
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.usage =
            storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT;
        const auto ffxDesc = ffxGetImageResourceDescriptionVK(
            image, ci,
            storage ? FFX_RESOURCE_USAGE_UAV : FFX_RESOURCE_USAGE_READ_ONLY);
        return ffxGetResourceVK(
            reinterpret_cast<void*>(image), ffxDesc, L"FreyaFsr", state);
    }

    int FsrUpscalePass::GetJitterPhaseCount() const
    {
        return ffxFsr3UpscalerGetJitterPhaseCount(mRenderExtent.width,
                                                  mDisplayExtent.width);
    }

    void FsrUpscalePass::GetJitterOffset(std::uint32_t frameIndex, float& outX,
                                         float& outY) const
    {
        outX = outY          = 0.f;
        const int phaseCount = GetJitterPhaseCount();
        if (phaseCount <= 0)
            return;
        const int index = static_cast<int>(
            frameIndex % static_cast<std::uint32_t>(phaseCount));
        ffxFsr3UpscalerGetJitterOffset(&outX, &outY, index, phaseCount);
    }

    void FsrUpscalePass::Dispatch(
        const skr::Arc<CommandPool>& commandPool,
        const skr::Arc<Image>& sceneColor, const skr::Arc<Image>& velocity,
        const skr::Arc<Image>& depth, float frameTimeDeltaMs, float cameraNear,
        float cameraFar, float cameraFovYRadians)
    {
        if (!mContextReady || !mOutputImage)
            return;

        auto commandBuffer = commandPool->GetCommandBuffer();
        mDevice->BeginDebugLabel(commandBuffer, DebugLabel::Fsr);

        const VkCommandBuffer cmd = commandBuffer;

        // After deferred + SSAO/lighting: color/velocity = SHADER_READ,
        // depth = DEPTH_STENCIL_READ_ONLY. FFX borrows into compute-read
        // layouts and we restore afterwards.
        Transition(cmd, depth->GetImage(),
                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_IMAGE_ASPECT_DEPTH_BIT);
        Transition(cmd, mOutputImage->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);

        float jitterX = mLastJitterX;
        float jitterY = mLastJitterY;

        FfxFsr3UpscalerDispatchDescription d {};
        d.commandList = ffxGetCommandListVK(cmd);
        d.color       = wrapColor(
            sceneColor->GetImage(),
            static_cast<VkFormat>(sceneColor->GetFormat()), mRenderExtent.width,
            mRenderExtent.height, false, FFX_RESOURCE_STATE_COMPUTE_READ);
        d.depth = wrapColor(
            depth->GetImage(), static_cast<VkFormat>(depth->GetFormat()),
            mRenderExtent.width, mRenderExtent.height, false,
            FFX_RESOURCE_STATE_COMPUTE_READ);
        d.motionVectors = wrapColor(
            velocity->GetImage(), static_cast<VkFormat>(velocity->GetFormat()),
            mRenderExtent.width, mRenderExtent.height, false,
            FFX_RESOURCE_STATE_COMPUTE_READ);
        d.exposure                   = {};
        d.reactive                   = {};
        d.transparencyAndComposition = {};
        d.output = wrapColor(mOutputImage->GetImage(),
                             static_cast<VkFormat>(mOutputImage->GetFormat()),
                             mDisplayExtent.width, mDisplayExtent.height, true,
                             FFX_RESOURCE_STATE_UNORDERED_ACCESS);

        d.dilatedDepth =
            mBackendInterface.fpGetResource(&mBackendInterface, mDilatedDepth);
        d.dilatedMotionVectors = mBackendInterface.fpGetResource(
            &mBackendInterface, mDilatedMotionVectors);
        d.reconstructedPrevNearestDepth = mBackendInterface.fpGetResource(
            &mBackendInterface, mReconstructedPrevNearestDepth);

        // Freya velocity is UV delta (curr−prev)×0.5. Scale to pixel space
        // with Y flip for Vulkan/clip conventions (see FSR integration notes).
        d.jitterOffset      = { -jitterX, jitterY };
        d.motionVectorScale = { static_cast<float>(mRenderExtent.width),
                                -static_cast<float>(mRenderExtent.height) };
        d.renderSize        = { mRenderExtent.width, mRenderExtent.height };
        d.upscaleSize       = { mDisplayExtent.width, mDisplayExtent.height };
        d.enableSharpening  = false;
        d.sharpness         = 0.f;
        d.frameTimeDelta    = frameTimeDeltaMs > 0.f ? frameTimeDeltaMs : 16.6f;
        d.preExposure       = 1.f;
        d.reset             = mResetNextDispatch;
        d.cameraNear        = cameraNear;
        d.cameraFar         = cameraFar;
        d.cameraFovAngleVertical  = cameraFovYRadians;
        d.viewSpaceToMetersFactor = 1.f;
        d.flags                   = 0;

        const auto err = ffxFsr3UpscalerContextDispatch(&mContext, &d);
        if (err != FFX_OK)
        {
            std::fprintf(stderr, "[Freya] FSR dispatch failed (%d)\n",
                         static_cast<int>(err));
        }

        mResetNextDispatch = false;

        Transition(cmd, mOutputImage->GetImage(), VK_IMAGE_LAYOUT_GENERAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_IMAGE_ASPECT_COLOR_BIT);
        Transition(cmd, depth->GetImage(),
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                   VK_IMAGE_ASPECT_DEPTH_BIT);

        mDevice->EndDebugLabel(commandBuffer);
    }
} // namespace FREYA_NAMESPACE
