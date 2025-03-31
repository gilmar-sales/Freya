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
                      const Ref<skr::Logger<WindowBuilder>>& logger) :
            mEventManager(eventManager), mFreyaOptions(freyaOptions),
            mLogger(logger)
        {
        }

        Ref<Window> Build();

      private:
        Ref<EventManager>               mEventManager;
        Ref<FreyaOptions>               mFreyaOptions;
        Ref<skr::Logger<WindowBuilder>> mLogger;
    };

} // namespace FREYA_NAMESPACE