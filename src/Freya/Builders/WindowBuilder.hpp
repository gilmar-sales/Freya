#pragma once

#include "Freya/Core/Window.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    class WindowBuilder
    {
      public:
        WindowBuilder(const Ref<EventManager>&               eventManager,
                      const Ref<FreyaOptions>&               freyaOptions,
                      const Ref<skr::Logger<WindowBuilder>>& logger,
                      const Ref<skr::Logger<Window>>& windowLogger) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mLogger(logger), mWindowLogger(windowLogger)
        {
        }

        Ref<Window> Build();

      private:
        Ref<EventManager>               mEventManager;
        Ref<FreyaOptions>               mFreyaOptions;
        Ref<skr::Logger<WindowBuilder>> mLogger;
        Ref<skr::Logger<Window>> mWindowLogger;
    };

} // namespace FREYA_NAMESPACE