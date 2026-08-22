#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Config.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <functional>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Non-owning view of a Freya GPU image for frame-stage plugins.
     *
     * Native* accessors return Vulkan handles as void* (VkImage /
     * VkImageView / VkSampler). Cast in app code that includes vulkan.h.
     * Sampler may be null when Freya has no dedicated sampler for the tap.
     */
    class GpuImageRef
    {
      public:
        GpuImageRef() = default;

        GpuImageRef(void*          nativeImage,
                    void*          nativeImageView,
                    void*          nativeSampler,
                    std::uint32_t  width,
                    std::uint32_t  height) :
            mImage(nativeImage), mImageView(nativeImageView),
            mSampler(nativeSampler), mWidth(width), mHeight(height)
        {
        }

        [[nodiscard]] bool valid() const { return mImageView != nullptr; }

        [[nodiscard]] std::uint32_t Width() const { return mWidth; }
        [[nodiscard]] std::uint32_t Height() const { return mHeight; }

        [[nodiscard]] void* NativeImage() const { return mImage; }
        [[nodiscard]] void* NativeImageView() const { return mImageView; }
        [[nodiscard]] void* NativeSampler() const { return mSampler; }

      private:
        void*         mImage     = nullptr;
        void*         mImageView = nullptr;
        void*         mSampler   = nullptr;
        std::uint32_t mWidth     = 0;
        std::uint32_t mHeight    = 0;
    };

    /**
     * @brief Public frame-stage context (Vulkan-free headers).
     *
     * Freya fills this each EndScene / Rebuild. Apps may implement
     * IFrameStage and read taps / native handles. Pass pointers and
     * other engine internals live on the internal RenderFrameContext
     * subclass — use static_cast only inside Freya.
     */
    struct StageContext
    {
        skr::Arc<FreyaOptions> options;
        Extent2D               renderExtent {};
        std::uint32_t          frameIndex = 0;
        float                  cameraNear = 1.0f;

        [[nodiscard]] void* NativeCommandBuffer() const
        {
            return nativeCommandBuffer;
        }

        [[nodiscard]] void* NativeDevice() const { return nativeDevice; }

        [[nodiscard]] GpuImageRef SceneColor() const { return sceneColor; }
        [[nodiscard]] GpuImageRef Depth() const { return depth; }
        [[nodiscard]] GpuImageRef Albedo() const { return albedo; }
        [[nodiscard]] GpuImageRef Normal() const { return normal; }
        [[nodiscard]] GpuImageRef Pbr() const { return pbr; }
        [[nodiscard]] GpuImageRef Velocity() const { return velocity; }
        [[nodiscard]] GpuImageRef Ssao() const { return ssao; }

        std::function<void(const glm::mat4&, CullMode, std::uint32_t)>
            DispatchCull;
        std::function<void(bool bindMaterials, std::uint32_t techniqueFilter)>
            ExecuteDraws;

        // Engine-filled opaque state (treat as read-only from apps).
        void*       nativeCommandBuffer = nullptr;
        void*       nativeDevice        = nullptr;
        GpuImageRef sceneColor;
        GpuImageRef depth;
        GpuImageRef albedo;
        GpuImageRef normal;
        GpuImageRef pbr;
        GpuImageRef velocity;
        GpuImageRef ssao;
    };

} // namespace FREYA_NAMESPACE
