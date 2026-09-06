#pragma once

#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <string>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Opaque description for creating a native OS window.
     */
    struct NativeWindowDesc
    {
        std::string   title;
        std::uint32_t width      = 800;
        std::uint32_t height     = 600;
        bool          fullscreen = false;
    };

    /**
     * @brief Platform abstraction for window lifetime and input pumping.
     *
     * One process-wide instance owns SDL (or another backend) init/shutdown,
     * creates native windows, routes polled events to Freya windows by
     * native window id, and is shared across all window scopes.
     */
    class IPlatform
    {
      public:
        virtual ~IPlatform() = default;

        /**
         * @brief Creates a native window.
         * @param desc   Desired title / size / fullscreen
         * @param options Mutated with the actual pixel size after creation
         * @return Opaque native handle (SDL_Window* for SdlPlatform)
         */
        virtual void* CreateNativeWindow(const NativeWindowDesc& desc,
                                         FreyaOptions&           options) = 0;

        /**
         * @brief Destroys a native window previously returned by
         * CreateNativeWindow. Does not shut down the platform.
         */
        virtual void DestroyNativeWindow(void* nativeWindow) = 0;

        /**
         * @brief Associates a Freya Window::Impl token with a native window
         * for event routing. @p windowToken is owned by Window.
         */
        virtual void AttachWindow(void* nativeWindow, void* windowToken) = 0;

        /**
         * @brief Removes the association created by AttachWindow.
         */
        virtual void DetachWindow(void* nativeWindow) = 0;

        /**
         * @brief Drains the platform event queue once and dispatches to the
         * owning Window by native window id. Call once per app frame.
         */
        virtual void PumpEvents() = 0;

        /**
         * @brief Content scale for the display that hosts @p nativeWindow.
         */
        [[nodiscard]] virtual float GetDisplayContentScale(
            void* nativeWindow) const = 0;
    };

} // namespace FREYA_NAMESPACE
