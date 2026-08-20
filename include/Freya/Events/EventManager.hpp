#pragma once

#include <unordered_map>

#include "Events.hpp"
#include "Publisher.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Pub/sub event dispatcher for type-safe event handling.
     */
    class EventManager
    {
      public:
        ~EventManager()
        {
            for (auto& entry : mPublishers)
                delete entry.second;
            mPublishers.clear();
        }

        template <typename T>
            requires IsEvent<T>
        EventSubscription Subscribe(auto&& listener)
        {
            return GetPublisher<T>()->Subscribe(listener);
        }

        template <typename T>
            requires IsEvent<T>
        void Unsubscribe(EventSubscription subscription)
        {
            if (mPublishers.contains(GetEventId<T>()))
                GetPublisher<T>()->Unsubscribe(subscription);
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
