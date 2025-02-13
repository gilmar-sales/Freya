#pragma once

#include <functional>

#include "Freya/Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    class IPublisher
    {
      public:
        virtual ~IPublisher() = default;
    };

    template <typename TEvent>
        requires IsEvent<TEvent>
    class Publisher final : public IPublisher
    {
      public:
        using EventListener = std::move_only_function<void(TEvent&)>;

        void Subscribe(auto&& listener)
        {
            mListeners.emplace_back(std::forward<EventListener>(listener));
        }

        void Publish(TEvent event)
        {
            for (auto i = 0; i < mListeners.size(); i++)
            {
                mListeners[i](event);
            }
        }

      private:
        std::vector<EventListener> mListeners;
    };
} // namespace FREYA_NAMESPACE
