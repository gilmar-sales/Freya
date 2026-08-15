#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/FreyaOptions.hpp"

#include <array>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Buffer;

    /**
     * @brief G-buffer / HDR sources bound to a FullscreenEffect (set 0).
     *
     * Bindings follow SetInputs() order. SceneColor is the current HDR
     * (OIT composite, else TAA, else deferred scene color).
     */
    enum class EffectInput
    {
        SceneColor,
        Depth,
        Albedo,
        Normal,
        Pbr,
        Velocity,
    };

    /**
     * @brief Push constants for Shaders/Cell/cell.frag (std430).
     */
    struct CellPushConstants
    {
        float     bands           = 4.0f;
        float     edgeDepthScale  = 80.0f;
        float     edgeNormalScale = 2.0f;
        float     strength        = 1.0f;
        glm::vec4 edgeColor { 0.02f, 0.02f, 0.04f, 1.0f };
        float     reverseZ = 0.0f;
    };

    /**
     * @brief Set 1 binding 1 (std140): which G-buffer material IDs (albedo.a)
     * receive the effect. count == 0 applies to every pixel.
     *
     * IDs are 0–255 (same packing as MaterialFactorsUniform::materialId).
     */
    struct FullscreenMaterialMask
    {
        glm::uvec4    bits[2] {};
        std::uint32_t count = 0;
        std::uint32_t pad[3] {};
    };

    static_assert(sizeof(FullscreenMaterialMask) == 48,
                  "FullscreenMaterialMask must match GLSL std140");

    /**
     * @brief User fullscreen fragment shader sampled from the G-buffer.
     *
     * Renders into an HDR ping-pong target, then blits back onto the
     * current scene HDR so Bloom / Composite keep their existing inputs.
     *
     * Set 0 is SetInputs() (combined image samplers). Set 1 is always
     * albedo (binding 0, material ID in .a) and FullscreenMaterialMask
     * (binding 1). BindMaterial() limits the effect to those pixels;
     * with no materials bound the shader should treat count == 0 as
     * all pixels.
     */
    class FullscreenEffect : public skr::enable_arc_from_this<FullscreenEffect>
    {
      public:
        FullscreenEffect(skr::Arc<Device> device,
                         skr::Arc<FreyaOptions>
                             options,
                         skr::Arc<skr::ServiceProvider>
                                     serviceProvider,
                         std::string name,
                         std::string fragmentRelative,
                         std::string vertexRelative,
                         std::vector<EffectInput>
                                       inputs,
                         std::uint32_t pushConstantSize);

        ~FullscreenEffect();

        FullscreenEffect(const FullscreenEffect&)            = delete;
        FullscreenEffect& operator=(const FullscreenEffect&) = delete;

        [[nodiscard]] const char* Name() const { return mName.c_str(); }

        void SetEnabled(bool enabled) { mEnabled = enabled; }

        [[nodiscard]] bool Enabled() const { return mEnabled; }

        void BindMaterial(std::uint32_t materialId);
        void UnbindMaterial(std::uint32_t materialId);
        void ClearMaterials();

        void SetPushConstants(const void* data, std::uint32_t size);

        template <typename T>
        void SetPushConstants(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>,
                          "Push constants must be trivially copyable");
            SetPushConstants(&value, static_cast<std::uint32_t>(sizeof(T)));
        }

        FrameStagePtr MakeStage();

        void Rebuild(RenderFrameContext& ctx, skr::ServiceProvider& sp);
        void Execute(RenderFrameContext& ctx);

      private:
        void            destroyGpu();
        void            uploadMaterialMask();
        skr::Arc<Image> resolveHdr(const RenderFrameContext& ctx) const;
        skr::Arc<Image> resolveInput(EffectInput               input,
                                     const RenderFrameContext& ctx,
                                     const skr::Arc<Image>&    hdr) const;
        vk::ImageLayout inputLayout(EffectInput input) const;

        skr::Arc<Device>               mDevice;
        skr::Arc<FreyaOptions>         mOptions;
        skr::Arc<skr::ServiceProvider> mServiceProvider;

        std::string              mName;
        std::string              mFragmentRelative;
        std::string              mVertexRelative;
        std::vector<EffectInput> mInputs;
        std::uint32_t            mPushConstantSize = 0;
        std::vector<std::byte>   mPushData;
        bool                     mEnabled = true;

        std::array<std::uint32_t, 8> mMaterialBits {};
        bool                         mMaskDirty = true;
        skr::Arc<Buffer>             mMaskBuffer;

        vk::RenderPass                 mRenderPass {};
        vk::PipelineLayout             mPipelineLayout {};
        vk::Pipeline                   mPipeline {};
        vk::DescriptorSetLayout        mSetLayout {};
        vk::DescriptorSetLayout        mMaskSetLayout {};
        vk::DescriptorPool             mDescriptorPool {};
        std::vector<vk::DescriptorSet> mDescriptorSets;
        std::vector<vk::DescriptorSet> mMaskDescriptorSets;
        vk::Sampler                    mSampler {};
        skr::Arc<Image>                mOutput;
        vk::Framebuffer                mFramebuffer {};
        vk::Extent2D                   mExtent {};
    };

} // namespace FREYA_NAMESPACE
