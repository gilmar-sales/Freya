#pragma once

#include <functional>

#include "Freya/Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Base interface for event publishers.
     */
    class IPublisher
    {
      public:
        virtual ~IPublisher() = default;
    };

    /**
     * @brief Template event publisher with listener management.
     *
     * @tparam TEvent Event type to publish (must satisfy IsEvent concept)
     */
    template <typename TEvent>
        requires IsEvent<TEvent>
    class Publisher final : public IPublisher
    {
      public:
        /**
         * @brief Listener callback type.
         */
        using EventListener = std::function<void(TEvent&)>;

        /**
         * @brief Adds a listener to the publisher.
         * @param listener Callback function to invoke on publish
         */
        void Subscribe(auto&& listener)
        {
            mListeners.emplace_back(std::forward<EventListener>(listener));
        }

        /**
         * @brief Calls all listeners with the event.
         * @param event Event to broadcast
         * @note Iterates through all listeners and calls each one
         */
        void Publish(TEvent event)
        {
            for (auto i = 0; i < mListeners.size(); i++)
            {
                mListeners[i](event);
            }
        }

      private:
        std::vector<EventListener> mListeners; ///< Stored listener callbacks
    };
} // namespace FREYA_NAMESPACE
