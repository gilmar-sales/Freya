#pragma once

namespace FREYA_NAMESPACE
{
    /**
     * @brief Type alias for event IDs.
     */
    using EventId = std::uint64_t;

    /**
     * @brief Global event counter for generating unique event IDs.
     */
    inline EventId EventCount = 0;

    /**
     * @brief Base event struct with handled flag.
     *
     * All specific event types inherit from this struct.
     * The handled flag prevents further propagation.
     */
    struct Event
    {
        bool handled; ///< Whether the event was handled (stop propagation)
    };

    /**
     * @brief Concept satisfied by types deriving from Event.
     */
    template <typename T>
    concept IsEvent = std::is_base_of_v<Event, T>;

    /**
     * @brief Generates unique event ID per type at compile time.
     *
     * Each call to GetEventId<T>() for a given T returns the same
     * static ID, providing type-safe event identification.
     *
     * @tparam T Event type (must satisfy IsEvent concept)
     * @return Unique EventId for the event type
     */
    template <typename T>
        requires IsEvent<T>
    constexpr auto GetEventId() -> EventId
    {
        static auto id = EventCount++;

        return id;
    }
} // namespace FREYA_NAMESPACE
