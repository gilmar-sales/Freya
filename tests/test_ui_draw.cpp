#include <Freya/Core/UiDraw.hpp>
#include <Freya/Core/UiTypes.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("UiDraw concurrent Rect and Snapshot", "[ui]")
{
    fra::UiDraw         draw(4096);
    std::atomic<int>    ready { 0 };
    constexpr int       kWorkers = 8;
    constexpr int       kIters   = 400;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kWorkers) + 1);

    for (int t = 0; t < kWorkers; ++t)
    {
        threads.emplace_back(
            [&]
            {
                ready.fetch_add(1, std::memory_order_relaxed);
                while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
                {
                }
                for (int i = 0; i < kIters; ++i)
                {
                    draw.Rect({ static_cast<float>(i), 0.f, 10.f, 10.f },
                              { 1.f, 1.f, 1.f, 1.f });
                }
            });
    }

    threads.emplace_back(
        [&]
        {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
            {
            }
            std::vector<fra::UiQuad> snap;
            for (int i = 0; i < kIters; ++i)
            {
                draw.Snapshot(snap);
                REQUIRE(snap.size() <= draw.MaxQuads());
                if ((i & 63) == 0)
                    draw.Clear();
            }
        });

    for (auto& th : threads)
        th.join();

    std::vector<fra::UiQuad> finalSnap;
    draw.Snapshot(finalSnap);
    REQUIRE(finalSnap.size() <= draw.MaxQuads());
}

TEST_CASE("UiDraw ProgressBar emits two quads", "[ui]")
{
    fra::UiDraw draw;
    draw.ProgressBar({ 0, 0, 100, 10 }, 0.5f, { 0, 0, 0, 1 }, { 1, 0, 0, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 2);
    REQUIRE(snap[1].clipMax == Catch::Approx(0.5f));
}

TEST_CASE("UiRect Contains and Intersect", "[ui]")
{
    fra::UiRect a { 10, 10, 50, 40 };
    REQUIRE(a.Contains(20, 20));
    REQUIRE_FALSE(a.Contains(5, 5));
    auto b = a.Intersect({ 40, 30, 40, 40 });
    REQUIRE(b.w == Catch::Approx(20.f));
    REQUIRE(b.h == Catch::Approx(20.f));
}
