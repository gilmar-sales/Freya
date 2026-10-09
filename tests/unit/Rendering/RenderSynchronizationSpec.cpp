#include "Freya/Internal/FrameUploadLayout.hpp"
#include "Freya/Internal/SynchronizedState.hpp"
#include "Freya/Internal/UploadStaging.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <future>
#include <thread>

TEST(FrameUploadLayout, AlignsAndSeparatesFlightSlots)
{
    const fra::FrameUploadLayout layout(3, 300, 256);
    EXPECT_EQ(layout.Stride(), 512u);
    EXPECT_EQ(layout.Size(), 1536u);
    EXPECT_EQ(layout.Offset(0), 0u);
    EXPECT_EQ(layout.Offset(1), 512u);
    EXPECT_EQ(layout.Offset(2), 1024u);
    EXPECT_THROW((void) layout.Offset(3), std::out_of_range);
}

TEST(FrameUploadLayout, RejectsInvalidDimensions)
{
    EXPECT_THROW(fra::FrameUploadLayout(0, 256, 256), std::invalid_argument);
    EXPECT_THROW(fra::FrameUploadLayout(2, 0, 256), std::invalid_argument);
    EXPECT_THROW(fra::FrameUploadLayout(2, 256, 0), std::invalid_argument);
}

TEST(FrameUploadLayout, SingleFrameAndAlreadyAlignedSize)
{
    const fra::FrameUploadLayout layout(1, 1024, 256);
    EXPECT_EQ(layout.Stride(), 1024u);
    EXPECT_EQ(layout.Size(), 1024u);
}

TEST(FrameUploadLayout, WritingNextSlotPreservesEarlierFrameData)
{
    const fra::FrameUploadLayout layout(3, 300, 256);
    std::vector<std::byte>       mapped(layout.Size());
    for (unsigned frame = 0; frame < 3; ++frame)
        std::fill_n(mapped.begin() + layout.Offset(frame), 300,
                    static_cast<std::byte>(frame + 1));
    for (unsigned frame = 0; frame < 3; ++frame)
        for (unsigned byte = 0; byte < 300; ++byte)
            EXPECT_EQ(mapped[layout.Offset(frame) + byte],
                      static_cast<std::byte>(frame + 1));
}

TEST(FrameUploadLayout, RejectsDynamicOffsetOverflow)
{
    EXPECT_THROW(fra::FrameUploadLayout(3, 1ull << 31, 256),
                 std::overflow_error);
    EXPECT_THROW(fra::FrameUploadLayout(1, 1ull << 32, 256),
                 std::overflow_error);
}

TEST(UploadStaging, ConcurrentChunksArePublishedCompletely)
{
    fra::UploadStaging<unsigned> staging;
    staging.Begin();
    staging.Reserve(1); // Exercise growth, not only pre-reserved storage.
    constexpr unsigned        workers = 8;
    constexpr unsigned        count   = 128;
    std::barrier              start(workers);
    std::vector<std::jthread> threads;
    for (unsigned w = 0; w < workers; ++w)
    {
        threads.emplace_back([&, w] {
            const std::vector<unsigned> chunk(count, w);
            start.arrive_and_wait();
            staging.Upload(chunk);
        });
    }
    threads.clear(); // Join before finalizing the upload window.
    const auto result = staging.End();
    ASSERT_EQ(result.size(), workers * count);
    for (unsigned w = 0; w < workers; ++w)
        EXPECT_EQ(std::count(result.begin(), result.end(), w), count);
    for (unsigned base = 0; base < result.size(); base += count)
        for (unsigned i = 0; i < count; ++i)
            EXPECT_EQ(result[base + i], result[base]);
}

TEST(UploadStaging, NewWindowDoesNotRetainOldUploads)
{
    fra::UploadStaging<unsigned>  staging;
    const std::array<unsigned, 2> chunk { 1, 2 };
    staging.Begin();
    staging.Upload(chunk);
    EXPECT_EQ(staging.End().size(), 2u);
    staging.Begin();
    EXPECT_TRUE(staging.End().empty());
}

TEST(UploadStaging, RejectsUploadsOutsideWindow)
{
    fra::UploadStaging<unsigned>  staging;
    const std::array<unsigned, 1> chunk { 1 };
    EXPECT_THROW(staging.Upload(chunk), std::logic_error);
    staging.Begin();
    (void) staging.End();
    EXPECT_THROW(staging.Upload(chunk), std::logic_error);
}

TEST(UploadStaging, FinalizationCannotObserveHalfCopiedChunk)
{
    struct Value
    {
        unsigned              id = 42;
        std::function<void()> copying;
        Value() = default;
        Value(const Value& other) : id(other.id), copying(other.copying)
        {
            if (copying)
                copying();
        }
        Value& operator=(const Value&) = default;
    };
    fra::UploadStaging<Value> staging;
    staging.Begin();
    staging.Reserve(1);
    std::promise<void>   copying;
    std::promise<void>   release;
    auto                 released = release.get_future().share();
    std::array<Value, 1> chunk;
    chunk[0].copying = [&] {
        copying.set_value();
        released.wait();
    };
    std::jthread worker([&] { staging.Upload(chunk); });
    copying.get_future().wait();
    std::promise<unsigned> finalized;
    auto                   result = finalized.get_future();
    std::jthread           coordinator([&] {
        const auto values = staging.End();
        finalized.set_value(values.size() == 1 ? values[0].id : 0);
    });
    EXPECT_EQ(result.wait_for(std::chrono::milliseconds(30)),
              std::future_status::timeout);
    release.set_value();
    worker.join();
    coordinator.join();
    EXPECT_EQ(result.get(), 42u);
}

TEST(QueueHostSynchronization, ConcurrentOperationsAreExclusive)
{
    fra::HostSynchronization  gate;
    unsigned                  completed = 0;
    std::atomic<unsigned>     active { 0 };
    std::atomic<bool>         overlap { false };
    std::vector<std::jthread> threads;
    for (unsigned w = 0; w < 8; ++w)
        threads.emplace_back([&] {
            for (unsigned i = 0; i < 100; ++i)
                gate.WithLock([&] {
                    if (active.fetch_add(1) != 0)
                        overlap.store(true);
                    std::this_thread::yield();
                    ++completed;
                    active.fetch_sub(1);
                });
        });
    threads.clear();
    EXPECT_FALSE(overlap.load());
    EXPECT_EQ(completed, 800u);
}

TEST(SynchronizedState, SnapshotsNeverMixConfigurationVersions)
{
    struct Configuration
    {
        unsigned yaw   = 0;
        unsigned pitch = 0;
    };
    fra::SynchronizedState<Configuration> state;
    std::atomic<bool>                     done { false };
    std::jthread                          writer([&] {
        for (unsigned i = 1; i < 10000; ++i)
            state.WithLock([&](Configuration& value) {
                value.yaw   = i;
                value.pitch = i;
            });
        done.store(true);
    });
    do
    {
        const auto snapshot = state.Snapshot();
        EXPECT_EQ(snapshot.yaw, snapshot.pitch);
    } while (!done.load());
}

TEST(SynchronizedRegistry, RemovalWaitsForAnActiveVisitor)
{
    fra::SynchronizedRegistry<int> registry;
    registry.Add(42);
    registry.Add(42); // Duplicate registration must not double-record.
    std::promise<void>    entered;
    std::promise<void>    release;
    const auto            released = release.get_future().share();
    std::atomic<unsigned> visits { 0 };
    std::jthread          reader([&] {
        registry.ForEach([&](int value) {
            EXPECT_EQ(value, 42);
            visits.fetch_add(1);
            entered.set_value();
            released.wait();
        });
    });
    entered.get_future().wait();
    std::promise<void> removed;
    auto               removal = removed.get_future();
    std::promise<void> removing;
    std::jthread       remover([&] {
        removing.set_value();
        registry.Remove(42);
        removed.set_value();
    });
    removing.get_future().wait();
    EXPECT_EQ(removal.wait_for(std::chrono::milliseconds(30)),
              std::future_status::timeout);
    release.set_value();
    reader.join();
    remover.join();
    EXPECT_EQ(removal.wait_for(std::chrono::milliseconds(0)),
              std::future_status::ready);
    registry.ForEach([&](int) { visits.fetch_add(1); });
    EXPECT_EQ(visits.load(), 1u);
}

TEST(SynchronizedState, ExceptionReleasesTheLock)
{
    fra::SynchronizedState<unsigned> state(0);
    EXPECT_THROW(
        state.WithLock([](unsigned&) { throw std::runtime_error("failure"); }),
        std::runtime_error);
    state.WithLock([](unsigned& value) { ++value; });
    EXPECT_EQ(state.Snapshot(), 1u);
}
