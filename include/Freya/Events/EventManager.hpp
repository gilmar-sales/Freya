#pragma once

#include <unordered_map>

#include "Events.hpp"
#include "Publisher.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Pub/sub event dispatcher for type-safe event handling.
     *
     * Template-based event system using concept constraints (IsEvent).
     * Manages publishers for each event type, creates them on first
     * subscription. Thread-safe via mutex in publishers.
     */
    class EventManager
    {
      public:
        /**
         * @brief Subscribes a listener to an event type.
         *
         * @tparam T    Event type (must satisfy IsEvent concept)
         * @param listener Callback function (auto&& deduced)
         * @note Creates publisher if it doesn't exist
         */
        template <typename T>
            requires IsEvent<T>
        void Subscribe(auto&& listener)
        {
            GetPublisher<T>()->Subscribe(listener);
        }

        /**
         * @brief Sends an event to all subscribers.
         *
         * @tparam T Event type
         * @param event Event to publish
         */
        template <typename T>
            requires IsEvent<T>
        void Send(T event)
        {
            GetPublisher<T>()->Publish(event);
        }

      private:
        /**
         * @brief Gets or creates publisher for event type T.
         * @tparam T Event type
         * @return Publisher pointer for the event type
         */
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

        std::unordered_map<EventId, IPublisher*>
            mPublishers; ///< Event type to publisher mapping
    };
} // namespace FREYA_NAMESPACE
