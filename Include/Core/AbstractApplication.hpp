#pragma once

#include "Core/Renderer.hpp"
#include "Core/Window.hpp"
#include "Events/EventManager.hpp"

namespace FREYA_NAMESPACE
{
    class AbstractApplication
    {
      public:
        virtual ~AbstractApplication() = default;

        virtual void StartUp() {};

        virtual void Update() = 0;

        virtual void Run()
        {
            StartUp();

            while (mWindow->IsRunning())
            {
                mWindow->Update();
                Update();
            }
        };

      protected:
        friend class ApplicationBuilder;

        float                         mDeltaTime;
        std::shared_ptr<Window>       mWindow;
        std::shared_ptr<Renderer>     mRenderer;
        std::shared_ptr<EventManager> mEventManager;
    };
} // namespace FREYA_NAMESPACE
