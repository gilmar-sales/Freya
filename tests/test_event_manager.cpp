#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Keyboard.hpp>

#include <gtest/gtest.h>

TEST(EventManager, GetEventIdStablePerTypeAndUniqueAcrossTypes)
{
    const auto pressed  = fra::GetEventId<fra::KeyPressedEvent>();
    const auto pressed2 = fra::GetEventId<fra::KeyPressedEvent>();
    const auto released = fra::GetEventId<fra::KeyReleasedEvent>();

    EXPECT_EQ(pressed, pressed2);
    EXPECT_NE(pressed, released);
}

TEST(EventManager, DeliversAndUnsubscribesListeners)
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
    EXPECT_EQ(pressed, 1);
    EXPECT_EQ(released, 1);

    events.Unsubscribe<fra::KeyPressedEvent>(pressSub);
    events.Send(fra::KeyPressedEvent {});
    events.Send(fra::KeyReleasedEvent {});
    EXPECT_EQ(pressed, 1);
    EXPECT_EQ(released, 2);
}

TEST(EventManager, DestructorReleasesPublishers)
{
    int calls = 0;
    {
        fra::EventManager events;
        events.Subscribe<fra::KeyPressedEvent>([&](fra::KeyPressedEvent&) {
            ++calls;
        });
        events.Send(fra::KeyPressedEvent {});
    }
    EXPECT_EQ(calls, 1);
}
