#pragma once

#include "Freya/Core/IPlatform.hpp"
#include "Freya/Core/Window.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for creating Window objects.
     *
     * Creates the native window through IPlatform and wires EventManager /
     * FreyaOptions into Window::Impl.
     */
    class WindowBuilder
    {
      public:
        WindowBuilder(const skr::Arc<EventManager>&               eventManager,
                      const skr::Arc<FreyaOptions>&               freyaOptions,
                      const skr::Arc<IPlatform>&                  platform,
                      const skr::Arc<skr::Logger<WindowBuilder>>& logger,
                      const skr::Arc<skr::Logger<Window>>& windowLogger) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mPlatform(platform), mLogger(logger), mWindowLogger(windowLogger)
        {
        }

        /**
         * @brief Builds and returns the Window object.
         * @return Shared pointer to created Window
         */
        skr::Arc<Window> Build();

      private:
        skr::Arc<EventManager>               mEventManager;
        skr::Arc<FreyaOptions>               mFreyaOptions;
        skr::Arc<IPlatform>                  mPlatform;
        skr::Arc<skr::Logger<WindowBuilder>> mLogger;
        skr::Arc<skr::Logger<Window>>        mWindowLogger;
    };

} // namespace FREYA_NAMESPACE
