#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freya/Core/FreyaExtension.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/Events/EventManager.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Base application class implementing the render loop.
     *
     * Inherits from skr::IApplication, provides StartUp/ShutDown hooks,
     * and implements the Run() method with frame timing.
     */
    class AbstractApplication : public skr::IApplication
    {
      public:
        /**
         * @brief Constructs with service provider, retrieves core services.
         * @param serviceProvider Service provider reference
         */
        explicit AbstractApplication(
            const Ref<skr::ServiceProvider>& serviceProvider) :
            IApplication(serviceProvider), mDeltaTime(0)
        {
            mEventManager = mRootServiceProvider->GetService<EventManager>();
            mWindow       = mRootServiceProvider->GetService<Window>();
            mRenderer     = mRootServiceProvider->GetService<Renderer>();
        }

        virtual ~AbstractApplication() = default;

        /**
         * @brief Called once before the first Update(). Override for setup.
         */
        virtual void StartUp() {};

        /**
         * @brief Called once during shutdown. Override for cleanup.
         */
        virtual void ShutDown() {};

        /**
         * @brief Called every frame. Override to implement game logic.
         */
        virtual void Update() = 0;

        /**
         * @brief Runs the main loop until window closes.
         */
        void Run() override;

      protected:
        friend class ApplicationBuilder;

        float             mDeltaTime;    ///< Time since last frame in seconds
        Ref<Window>       mWindow;       ///< Window service reference
        Ref<Renderer>     mRenderer;     ///< Renderer service reference
        Ref<EventManager> mEventManager; ///< Event manager service reference
    };
} // namespace FREYA_NAMESPACE
