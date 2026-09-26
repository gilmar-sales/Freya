#include <Freya/Core/SpinLock.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(SpinLock, SerializesConcurrentIncrements)
{
    fra::SpinLock    lock;
    std::atomic<int> ready { 0 };
    int              counter  = 0;
    constexpr int    kThreads = 8;
    constexpr int    kIters   = 1000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&] {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < kThreads)
            {
            }
            for (int i = 0; i < kIters; ++i)
            {
                fra::SpinLockGuard guard(lock);
                ++counter;
            }
        });
    }
    for (auto& th : threads)
        th.join();

    EXPECT_EQ(counter, kThreads * kIters);
}

TEST(SpinLock, TryLockFailsWhenHeld)
{
    fra::SpinLock lock;
    lock.lock();
    EXPECT_FALSE(lock.try_lock());
    lock.unlock();
    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}
