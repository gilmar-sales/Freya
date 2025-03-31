#pragma once

#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/Events/EventManager.hpp"

namespace FREYA_NAMESPACE
{
    class AbstractApplication
    {
      public:
        explicit AbstractApplication(
            const Ref<skr::ServiceProvider>& serviceProvider) : mDeltaTime(0)
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

        virtual void Run();

      protected:
        friend class ApplicationBuilder;

        float                     mDeltaTime;
        Ref<skr::ServiceProvider> mServiceProvider;
        Ref<Window>               mWindow;
        Ref<Renderer>             mRenderer;
        Ref<EventManager>         mEventManager;
    };
} // namespace FREYA_NAMESPACE
