#pragma once

#include "Freya/Events/EventManager.hpp"
#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Platform window, event polling, and timing.
     */
    class Window
    {
      public:
        struct Impl;

        explicit Window(std::unique_ptr<Impl> impl);
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        void Update();

        [[nodiscard]] std::uint32_t GetWidth() const;
        [[nodiscard]] std::uint32_t GetHeight() const;
        [[nodiscard]] float         GetScale() const;

        void Resize(std::uint32_t width, std::uint32_t height);

        [[nodiscard]] bool IsRunning() const;
        void               Close();

        [[nodiscard]] float GetDeltaTime() const;

        [[nodiscard]] bool IsFullscreen() const;
        void               SetFullscreen(bool fullscreen);

        [[nodiscard]] bool IsMouseGrab() const;
        void               SetMouseGrab(bool grab) const;

      private:
        friend class WindowBuilder;
        friend struct WindowNative;

        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
