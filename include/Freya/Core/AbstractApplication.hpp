#pragma once

#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Core/FreyaExtension.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/Events/EventManager.hpp"

#include <Skirnir/Skirnir.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace FREYA_NAMESPACE
{
    class IPlatform;

    /**
     * @brief Base application class implementing the multi-window render loop.
     *
     * Creates an implicit main window scope on construction. Secondary windows
     * are opened via CreateWindow; close them with Window::Close().
     *
     * Device / PhysicalDevice singletons must be resolved for the first time
     * from a window scope (done here) because DeviceBuilder needs Surface.
     */
    class AbstractApplication : public skr::IApplication
    {
      public:
        explicit AbstractApplication(
            const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~AbstractApplication() override;

        AbstractApplication(const AbstractApplication&)            = delete;
        AbstractApplication& operator=(const AbstractApplication&) = delete;

        virtual void StartUp() {};

        virtual void ShutDown() {};

        virtual void Update() = 0;

        /**
         * @brief Optional per-frame hook for each live secondary window.
         */
        virtual void UpdateSecondaryWindow(const skr::Arc<Window>& /*window*/)
        {
        }

        void Run() override;

        /**
         * @brief Opens a secondary window with its own scoped Renderer /
         * lights / cull state. Shared assets (MeshPool / TexturePool /
         * MaterialPool) remain process-wide singletons.
         *
         * Close with @c window->Close(); the app loop tears down the scope.
         */
        skr::Arc<Window> CreateWindow(
            const std::function<void(FreyaOptionsBuilder&)>& configure = {});

        [[nodiscard]] const std::vector<skr::Arc<Window>>& SecondaryWindows()
            const;

        /**
         * @brief Service provider for the main window scope.
         *
         * Use this (not the root provider) to resolve scoped Freya services
         * such as LightService, FreyaOptions, or EventManager.
         */
        [[nodiscard]] skr::Arc<skr::ServiceProvider> GetMainServiceProvider()
            const;

        /**
         * @brief Scoped service provider for @p window (main or secondary).
         */
        [[nodiscard]] skr::Arc<skr::ServiceProvider> GetWindowServices(
            const Window& window) const;

        /**
         * @brief Renderer bound to @p window (main or secondary).
         */
        [[nodiscard]] skr::Arc<Renderer> GetRenderer(
            const Window& window) const;

        [[nodiscard]] const skr::Arc<skr::ServiceScope>& GetMainScope() const
        {
            return mMainScope;
        }

      protected:
        friend class ApplicationBuilder;

        float                  mDeltaTime = 0;
        skr::Arc<Window>       mWindow;
        skr::Arc<Renderer>     mRenderer;
        skr::Arc<EventManager> mEventManager;

        skr::Arc<skr::ServiceScope> mMainScope;
        skr::Arc<IPlatform>         mPlatform;

      private:
        struct MultiWindowState;

        std::unique_ptr<MultiWindowState> mMultiWindow;
    };
} // namespace FREYA_NAMESPACE
