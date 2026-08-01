#pragma once

#include "Freya/Core/Window.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for creating Window objects with SDL3.
     *
     * Initializes SDL3 video and gamepad subsystems, loads Vulkan library,
     * and creates window with configured flags.
     *
     * @param eventManager Event manager reference
     * @param freyaOptions Freya options reference
     * @param logger       Logger for window builder
     * @param windowLogger Logger for window operations
     */
    class WindowBuilder
    {
      public:
        WindowBuilder(const skr::Arc<EventManager>&               eventManager,
                      const skr::Arc<FreyaOptions>&               freyaOptions,
                      const skr::Arc<skr::Logger<WindowBuilder>>& logger,
                      const skr::Arc<skr::Logger<Window>>&        windowLogger) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mLogger(logger), mWindowLogger(windowLogger)
        {
        }

        /**
         * @brief Builds and returns the Window object.
         * @return Shared pointer to created Window
         */
        skr::Arc<Window> Build();

      private:
        skr::Arc<EventManager> mEventManager;         ///< Event manager reference
        skr::Arc<FreyaOptions> mFreyaOptions;         ///< Freya options reference
        skr::Arc<skr::Logger<WindowBuilder>> mLogger; ///< Logger reference
        skr::Arc<skr::Logger<Window>> mWindowLogger;  ///< Window operation logger
    };

} // namespace FREYA_NAMESPACE