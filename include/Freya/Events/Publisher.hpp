#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "Freya/Events/Event.hpp"

namespace FREYA_NAMESPACE
{
    using EventSubscription = std::uint64_t;

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
        using EventListener = std::function<void(TEvent&)>;

        EventSubscription Subscribe(auto&& listener)
        {
            std::lock_guard lock { mMutex };
            const auto      id = mNextId++;
            mListeners.push_back({ id, std::forward<EventListener>(listener) });
            return id;
        }

        void Unsubscribe(EventSubscription id)
        {
            std::lock_guard lock { mMutex };
            for (auto it = mListeners.begin(); it != mListeners.end(); ++it)
            {
                if (it->id == id)
                {
                    mListeners.erase(it);
                    return;
                }
            }
        }

        void Publish(TEvent event)
        {
            std::lock_guard lock { mMutex };
            for (const auto& entry : mListeners)
                entry.listener(event);
        }

      private:
        struct ListenerEntry
        {
            EventSubscription id;
            EventListener     listener;
        };

        std::mutex                 mMutex;
        std::vector<ListenerEntry> mListeners;
        EventSubscription          mNextId = 1;
    };
} // namespace FREYA_NAMESPACE
