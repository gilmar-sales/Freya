#pragma once

/**
 * @file RendererAdvanced.hpp
 * @brief Advanced Renderer surface (plugins, UI natives, GPU anim, dumps).
 */

#include "Freya/Asset/CullFrameDump.hpp"
#include "Freya/Asset/SceneInstanceUpload.hpp"
#include "Freya/Core/GpuAnimationSystem.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/RendererUi.hpp"

#include <span>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Advanced operations on a Renderer (frame stages, RHI escapes,
     * ImGui, cull dumps, GPU animation, raw instance upload).
     */
    class RendererAdvanced
    {
      public:
        explicit RendererAdvanced(Renderer& renderer) : mRenderer(renderer) {}

        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads);

        /**
         * @brief Append while BeginSceneInstances…EndSceneInstances is open.
         *
         * Prefer Renderer::BeginSceneInstances / Reserve / Upload / End.
         */
        void BeginSceneInstances();
        void ReserveSceneInstances(std::uint32_t count);
        void EndSceneInstances();

        [[deprecated("Use Scene::Upload or Begin/Upload/EndSceneInstances")]]
        void Draw(std::uint32_t meshId,
                  std::uint32_t materialId,
                  std::uint32_t entityId    = kPickMissId,
                  bool          castShadows = true);

        [[deprecated("Use Scene::Upload or Begin/Upload/EndSceneInstances")]]
        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0,
                           bool          castShadows   = true,
                           std::uint32_t entityId      = kPickMissId);

        [[deprecated("Use Scene::Upload or Begin/Upload/EndSceneInstances")]]
        void SetInstanceModels(const glm::mat4* models, std::size_t count);

        void RequestCullFrameDump();
        bool TryConsumeCullFrameDump(CullFrameSnapshot& out);

        bool InsertFrameStage(const char* beforeName, FrameStagePtr stage);
        bool ReplaceFrameStage(const char* name, FrameStagePtr stage);

        [[nodiscard]] void* NativeCommandBuffer();
        [[nodiscard]] void* NativeDevice();

        [[nodiscard]] bool BeginUI();
        void               EndUI();

        [[nodiscard]] ImGuiNativeHandles GetImGuiNativeHandles();
        [[nodiscard]] ImGuiViewportImage GetViewportImage();

        [[nodiscard]] bool SetViewportTarget(std::uint32_t width,
                                             std::uint32_t height);
        void               ClearOutputTarget();

        [[nodiscard]] GpuAnimationSystem&       GpuAnimation();
        [[nodiscard]] const GpuAnimationSystem& GpuAnimation() const;

      private:
        Renderer& mRenderer;
    };

    [[nodiscard]] inline RendererAdvanced Advanced(Renderer& renderer)
    {
        return RendererAdvanced { renderer };
    }

} // namespace FREYA_NAMESPACE
