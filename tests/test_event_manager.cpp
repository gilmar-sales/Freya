#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Keyboard.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("GetEventId is stable per type and unique across types", "[events]")
{
    const auto pressed  = fra::GetEventId<fra::KeyPressedEvent>();
    const auto pressed2 = fra::GetEventId<fra::KeyPressedEvent>();
    const auto released = fra::GetEventId<fra::KeyReleasedEvent>();

    REQUIRE(pressed == pressed2);
    REQUIRE(pressed != released);
}

TEST_CASE("EventManager delivers and unsubscribes listeners", "[events]")
{
    fra::EventManager events;
    int               pressed  = 0;
    int               released = 0;

    const auto pressSub = events.Subscribe<fra::KeyPressedEvent>(
        [&](fra::KeyPressedEvent&) { ++pressed; });
    events.Subscribe<fra::KeyReleasedEvent>([&](fra::KeyReleasedEvent&) {
        ++released;
    });

    events.Send(fra::KeyPressedEvent {});
    events.Send(fra::KeyReleasedEvent {});
    REQUIRE(pressed == 1);
    REQUIRE(released == 1);

    events.Unsubscribe<fra::KeyPressedEvent>(pressSub);
    events.Send(fra::KeyPressedEvent {});
    events.Send(fra::KeyReleasedEvent {});
    REQUIRE(pressed == 1);
    REQUIRE(released == 2);
}

TEST_CASE("EventManager destructor releases publishers", "[events]")
{
    int calls = 0;
    {
        fra::EventManager events;
        events.Subscribe<fra::KeyPressedEvent>([&](fra::KeyPressedEvent&) {
            ++calls;
        });
        events.Send(fra::KeyPressedEvent {});
    }
    REQUIRE(calls == 1);
}
