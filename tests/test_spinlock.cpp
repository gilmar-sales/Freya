#include <Freya/Core/SpinLock.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("SpinLock serializes concurrent increments", "[spinlock]")
{
    fra::SpinLock       lock;
    std::atomic<int>    ready { 0 };
    int                 counter = 0;
    constexpr int       kThreads = 8;
    constexpr int       kIters   = 1000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back(
            [&]
            {
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

    REQUIRE(counter == kThreads * kIters);
}

TEST_CASE("SpinLock try_lock fails when held", "[spinlock]")
{
    fra::SpinLock lock;
    lock.lock();
    REQUIRE_FALSE(lock.try_lock());
    lock.unlock();
    REQUIRE(lock.try_lock());
    lock.unlock();
}
