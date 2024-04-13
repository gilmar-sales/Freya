#pragma once

#include "Core/Renderer.hpp"

#include "Builders/InstanceBuilder.hpp"

#include <SDL_video.h>


namespace FREYA_NAMESPACE
{
    class InstanceBuilder;

    class RendererBuilder
    {
      public:
        RendererBuilder()
            : mInstanceBuilder(InstanceBuilder()), mWindow(nullptr), mWidth(1280), mHeight(720),
              mVSync(true), mFrameCount(4), mSamples(vk::SampleCountFlagBits::e1)
        {
        }

        RendererBuilder &WithInstance(std::function<void(InstanceBuilder&)> instanceBuilderFunc)
        {
            instanceBuilderFunc(mInstanceBuilder);
            return *this;
        }

        RendererBuilder &SetWindow(SDL_Window *window)
        {
            mWindow = window;
            return *this;
        }

        RendererBuilder &SetWidth(std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        RendererBuilder &SetHeight(std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        RendererBuilder &SetVSync(bool vSync)
        {
            mVSync = vSync;
            return *this;
        }

        RendererBuilder &SetFrameCount(std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        RendererBuilder &SetSamples(vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        std::shared_ptr<Renderer> Build();

      private:
        InstanceBuilder mInstanceBuilder;

        SDL_Window *mWindow;
        std::uint32_t mWidth;
        std::uint32_t mHeight;
        bool mVSync;

        std::uint32_t mFrameCount;
        vk::SampleCountFlagBits mSamples;
    };

} // namespace FREYA_NAMESPACE
