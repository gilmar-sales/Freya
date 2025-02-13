#pragma once

#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/Events/EventManager.hpp"

#include <ServiceProvider.hpp>

namespace FREYA_NAMESPACE
{
    class AbstractApplication
    {
      public:
        explicit AbstractApplication(
            const Ref<ServiceProvider>& serviceProvider) : mDeltaTime(0)
        {
            mServiceProvider = serviceProvider;
            mEventManager    = mServiceProvider->GetService<EventManager>();
            mWindow          = mServiceProvider->GetService<Window>();
            mRenderer        = mServiceProvider->GetService<Renderer>();
        }

        virtual ~AbstractApplication() = default;

        virtual void StartUp() {};
        virtual void ShutDown() {};

        virtual void Update() = 0;

        virtual void Run()
        {
            StartUp();

            while (mWindow->IsRunning())
            {
                mWindow->Update();
                Update();
            }

            ShutDown();
        };

      protected:
        friend class ApplicationBuilder;

        float                mDeltaTime;
        Ref<ServiceProvider> mServiceProvider;
        Ref<Window>          mWindow;
        Ref<Renderer>        mRenderer;
        Ref<EventManager>    mEventManager;
    };
} // namespace FREYA_NAMESPACE
