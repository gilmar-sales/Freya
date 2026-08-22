#pragma once

#include "Freya/Core/IFrameStage.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Device;
    class PostProcessBuilder;

    enum class PostProcessInput
    {
        SceneColor,
        Depth,
        Albedo,
        Normal,
        Pbr,
        Velocity,
    };

    struct PostProcessMaterialMask
    {
        glm::uvec4    bits[8] {};
        std::uint32_t count = 0;
        std::uint32_t pad[3] {};
    };

    static_assert(sizeof(PostProcessMaterialMask) == 144,
                  "PostProcessMaterialMask must match GLSL std140");

    class PostProcess : public skr::enable_arc_from_this<PostProcess>
    {
      public:
        class Impl;

        explicit PostProcess(std::unique_ptr<Impl> impl);
        ~PostProcess();

        PostProcess(const PostProcess&)            = delete;
        PostProcess& operator=(const PostProcess&) = delete;

        [[nodiscard]] const char* Name() const;
        void                      SetEnabled(bool enabled);
        [[nodiscard]] bool        Enabled() const;

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
        friend class PostProcessBuilder;
        friend class PostProcessStage;

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
