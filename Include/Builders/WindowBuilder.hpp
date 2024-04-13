#pragma once

#include "Core/Window.hpp"

namespace FREYA_NAMESPACE
{

    class WindowBuilder
    {
      public:
        WindowBuilder() : mTitle("Freya Window"), mWidth(800), mHeight(600) {}

        WindowBuilder &SetTitle(std::string title)
        {
            mTitle = title;
            return *this;
        }

        WindowBuilder &SetWidth(std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        WindowBuilder &SetHeight(std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        WindowBuilder &SetVSync(bool vSync)
        {
            mVSync = vSync;
            return *this;
        }

        std::shared_ptr<Window> Build();

      private:
        std::string mTitle;
        std::uint32_t mWidth;
        std::uint32_t mHeight;
        bool mVSync;
    };

} // namespace FREYA_NAMESPACE