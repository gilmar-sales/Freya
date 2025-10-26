#pragma once

#include "Freya/Core/FreyaExtension.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/Events/EventManager.hpp"

namespace FREYA_NAMESPACE
{
    class AbstractApplication : public skr::IApplication
    {
      public:
        explicit AbstractApplication(
            const Ref<skr::ServiceProvider>& serviceProvider) :
            IApplication(serviceProvider), mDeltaTime(0)
        {
            mEventManager = mRootServiceProvider->GetService<EventManager>();
            mWindow       = mRootServiceProvider->GetService<Window>();
            mRenderer     = mRootServiceProvider->GetService<Renderer>();
        }

        virtual ~AbstractApplication() = default;

        virtual void StartUp() {};
        virtual void ShutDown() {};

        virtual void Update() = 0;

        void Run() override;

      protected:
        friend class ApplicationBuilder;

        float             mDeltaTime;
        Ref<Window>       mWindow;
        Ref<Renderer>     mRenderer;
        Ref<EventManager> mEventManager;
    };
} // namespace FREYA_NAMESPACE
