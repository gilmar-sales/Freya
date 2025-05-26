#pragma once

namespace FREYA_NAMESPACE
{
    using EventId = std::uint64_t;

    inline EventId EventCount = 0;

    struct Event
    {
        bool handled;
    };

    template <typename T>
    concept IsEvent = std::is_base_of_v<Event, T>;

    template <typename T>
        requires IsEvent<T>
    constexpr auto GetEventId() -> EventId
    {
        static auto id = EventCount++;

        return id;
    }
} // namespace FREYA_NAMESPACE
