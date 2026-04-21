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
        WindowBuilder(const Ref<EventManager>&               eventManager,
                      const Ref<FreyaOptions>&               freyaOptions,
                      const Ref<skr::Logger<WindowBuilder>>& logger,
                      const Ref<skr::Logger<Window>>&        windowLogger) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mLogger(logger), mWindowLogger(windowLogger)
        {
        }

        /**
         * @brief Builds and returns the Window object.
         * @return Shared pointer to created Window
         */
        Ref<Window> Build();

      private:
        Ref<EventManager> mEventManager;         ///< Event manager reference
        Ref<FreyaOptions> mFreyaOptions;         ///< Freya options reference
        Ref<skr::Logger<WindowBuilder>> mLogger; ///< Logger reference
        Ref<skr::Logger<Window>> mWindowLogger;  ///< Window operation logger
    };

} // namespace FREYA_NAMESPACE