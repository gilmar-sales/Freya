#pragma once

#include <unordered_map>

#include "Events.hpp"
#include "Publisher.hpp"

namespace FREYA_NAMESPACE
{
    class EventManager
    {
      public:
        template <typename T>
            requires IsEvent<T>
        void Subscribe(auto&& listener)
        {
            GetPublisher<T>()->Subscribe(listener);
        }

        template <typename T>
            requires IsEvent<T>
        void Send(T event)
        {
            GetPublisher<T>()->Publish(event);
        }

      private:
        template <typename T>
            requires IsEvent<T>
        Publisher<T>* GetPublisher()
        {
            if (!mPublishers.contains(GetEventId<T>()))
            {
                mPublishers[GetEventId<T>()] = new Publisher<T>();
            }

            return static_cast<Publisher<T>*>(mPublishers[GetEventId<T>()]);
        }

        std::unordered_map<EventId, IPublisher*> mPublishers;
    };
} // namespace FREYA_NAMESPACE
