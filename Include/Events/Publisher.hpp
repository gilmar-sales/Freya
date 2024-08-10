#pragma once

namespace FREYA_NAMESPACE
{
    class IPublisher
    {
      public:
        virtual ~IPublisher() = default;
    };

    template <typename TEvent>
    class Publisher : public IPublisher
    {
      public:
        using EventListener = std::move_only_function<void(TEvent)>;

        void Subscribe(auto&& listener)
        {
            mListeners.emplace_back(std::move(listener));
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
