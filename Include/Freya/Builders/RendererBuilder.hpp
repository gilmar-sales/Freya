#pragma once

#include "Freya/Core/Renderer.hpp"

#include "Freya/Builders/InstanceBuilder.hpp"

#include <SDL3/SDL_video.h>

namespace FREYA_NAMESPACE
{
    class InstanceBuilder;

    class RendererBuilder
    {
      public:
        RendererBuilder(const Ref<skr::ServiceProvider>& serviceProvider) :
            mServiceProvider(serviceProvider), mWindow(nullptr), mWidth(1280),
            mHeight(720), mVSync(true), mFrameCount(4),
            mSamples(vk::SampleCountFlagBits::e1),
            mClearColor(vk::ClearColorValue { 0.0f, 0.0f, 0.0f, 0.0f }),
            mDrawDistance(1000.0f),
            mLogger(serviceProvider->GetService<skr::Logger>())
        {
        }

        RendererBuilder& WithInstance(
            const std::function<void(InstanceBuilder&)>& instanceBuilderFunc)
        {
            mInstanceBuilderFunc = instanceBuilderFunc;
            return *this;
        }

        RendererBuilder& SetWindow(SDL_Window* window)
        {
            mWindow = window;
            return *this;
        }

        RendererBuilder& SetWidth(const std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        RendererBuilder& SetHeight(const std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        RendererBuilder& SetVSync(const bool vSync)
        {
            mVSync = vSync;
            return *this;
        }

        RendererBuilder& SetFrameCount(const std::uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        RendererBuilder& SetSamples(const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        RendererBuilder& SetClearColor(const vk::ClearColorValue clearColor)
        {
            mClearColor = clearColor;
            return *this;
        }

        RendererBuilder& SetDrawDistance(const float drawDistance)
        {

            mDrawDistance = drawDistance;
            return *this;
        }
        Ref<Renderer> Build();

      private:
        friend class ApplicationBuilder;

        RendererBuilder& SetEventManager(const Ref<EventManager>& eventManager)
        {
            mEventManager = eventManager;
            return *this;
        };

        Ref<EventManager>                     mEventManager;
        std::function<void(InstanceBuilder&)> mInstanceBuilderFunc;

        Ref<skr::ServiceProvider> mServiceProvider;
        Ref<skr::Logger>          mLogger;

        SDL_Window*   mWindow;
        std::uint32_t mWidth;
        std::uint32_t mHeight;
        bool          mVSync;

        std::uint32_t           mFrameCount;
        vk::SampleCountFlagBits mSamples;
        vk::ClearColorValue     mClearColor;
        float                   mDrawDistance;
    };

} // namespace FREYA_NAMESPACE
