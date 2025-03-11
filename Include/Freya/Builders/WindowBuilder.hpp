#pragma once

#include "Freya/Core/Window.hpp"

namespace FREYA_NAMESPACE
{

    class WindowBuilder
    {
      public:
        WindowBuilder(const Ref<skr::Logger<WindowBuilder>>& logger) :
            mTitle("Freya Window"), mWidth(800), mHeight(600), mVSync(false),
            mLogger(logger)
        {
        }

        WindowBuilder& SetTitle(const std::string& title)
        {
            mTitle = title;
            return *this;
        }

        WindowBuilder& SetWidth(const std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        WindowBuilder& SetHeight(const std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        WindowBuilder& SetVSync(const bool vSync)
        {
            mVSync = vSync;
            return *this;
        }

        Ref<Window> Build();

      private:
        friend class ApplicationBuilder;

        WindowBuilder& SetEventManager(const Ref<EventManager>& eventManager)
        {
            mEventManager = eventManager;
            return *this;
        };

        Ref<EventManager> mEventManager;

        std::string   mTitle;
        std::uint32_t mWidth;
        std::uint32_t mHeight;
        bool          mVSync;

        Ref<skr::Logger<WindowBuilder>> mLogger;
    };

} // namespace FREYA_NAMESPACE