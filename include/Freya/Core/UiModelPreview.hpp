#pragma once

#include "Freya/Core/LightService.hpp"
#include "Freya/Events/Mouse.hpp"
#include "Freya/Scene/AssetHandle.hpp"
#include "Freya/Scene/Scene.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class CommandPool;
    class SwapChain;
    class TexturePool;

    /**
     * @brief Orbit / auto-rotate controls for UiModelPreview.
     *
     * Yaw/pitch used by Record may be updated from the UI thread via
     * FeedMouse* / SetOrbit; Record reads them atomically.
     */
    struct UiModelPreviewOrbit
    {
        bool        enabled     = true;
        MouseButton dragButton  = MouseButton::Left;
        float       sensitivity = 0.25f;
        float       yawDeg      = 30.f;
        float       pitchDeg    = -10.f;
        float       minPitch    = -80.f;
        float       maxPitch    = 80.f;
        float       distance    = 2.5f;
        bool        autoRotate  = false;
        float       autoSpeed   = 25.f; ///< deg/s
        glm::vec3   target { 0.f, 0.75f, 0.f };
        float       fovDegrees = 45.f;
        float       nearPlane  = 0.1f;
        float       farPlane   = 50.f;

        [[nodiscard]] float ClampPitch(float pitch) const
        {
            return pitch < minPitch   ? minPitch
                   : pitch > maxPitch ? maxPitch
                                      : pitch;
        }
    };

    /**
     * @brief Mini deferred+shadow preview into a panel-sized RenderTarget.
     *
     * Register with Renderer::AddModelPreview so ModelPreviewFrameStage
     * calls Record before ScreenUi. Texture() is a live bindless handle for
     * UiDraw::Image. CaptureSnapshot copies LDR pixels into a static
     * TexturePool-owned handle (HUD portrait, etc.).
     *
     * Post-process follows FreyaOptions: enableSsao / enableShadowMask /
     * enableTaa / enableBloom (same stack as the main window path).
     *
     * Threading: Record only from the frame stage (render thread). Orbit
     * FeedMouse / SetOrbit from the UI thread. CaptureSnapshot from
     * main/render thread after at least one Record.
     */
    class UiModelPreview
    {
      public:
        UiModelPreview(const skr::Arc<skr::ServiceProvider>& serviceProvider,
                       TexturePool&                          texturePool,
                       glm::uvec2 extent = { 512, 512 });

        ~UiModelPreview();

        UiModelPreview(const UiModelPreview&)            = delete;
        UiModelPreview& operator=(const UiModelPreview&) = delete;

        void Resize(glm::uvec2 extent);

        [[nodiscard]] TextureHandle Texture() const;
        [[nodiscard]] glm::uvec2    Extent() const;

        UiModelPreviewOrbit&                     Orbit();
        [[nodiscard]] const UiModelPreviewOrbit& Orbit() const;
        void SetOrbit(const UiModelPreviewOrbit& orbit);

        Scene&                     PreviewScene();
        [[nodiscard]] const Scene& PreviewScene() const;

        LightService&                     Lights();
        [[nodiscard]] const LightService& Lights() const;

        void FeedMouseMove(float deltaX, float deltaY);
        void FeedMouseButton(MouseButton button, bool down);
        void SetFrameDelta(float dt);

        /**
         * @brief When false, Record is a no-op (keeps last RT contents).
         * Use to avoid running the mini deferred stack while the panel
         * is hidden.
         */
        void               SetActive(bool active);
        [[nodiscard]] bool IsActive() const;

        /**
         * @brief Record shadow + deferred + composite into the preview RT.
         * Called by ModelPreviewFrameStage.
         */
        void Record(const skr::Arc<CommandPool>& commandPool,
                    const skr::Arc<SwapChain>&   swapChain,
                    std::uint32_t                frameIndex);

        /**
         * @brief GPU readback of the LDR RT into a static TextureHandle.
         * @param size Optional downscale (0 = full RT extent).
         */
        TextureHandle CaptureSnapshot(TexturePool& pool, glm::uvec2 size = {});

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
