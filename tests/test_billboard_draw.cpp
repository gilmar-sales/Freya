#include <Freya/Core/BillboardDraw.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("BillboardDraw concurrent Quad and Snapshot", "[billboard]")
{
    fra::BillboardDraw draw(4096);
    std::atomic<int>   ready { 0 };
    constexpr int      kWorkers = 8;
    constexpr int      kIters   = 500;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kWorkers) + 1);

    for (int t = 0; t < kWorkers; ++t)
    {
        threads.emplace_back([&] {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
            {
            }
            fra::Billboard b {};
            b.size = { 1.f, 1.f };
            for (int i = 0; i < kIters; ++i)
            {
                b.worldPos.x = static_cast<float>(i);
                draw.Quad(b);
            }
        });
    }

    threads.emplace_back([&] {
        ready.fetch_add(1, std::memory_order_relaxed);
        while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
        {
        }
        std::vector<fra::Billboard> snap;
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

    std::vector<fra::Billboard> finalSnap;
    draw.Snapshot(finalSnap);
    REQUIRE(finalSnap.size() <= draw.MaxQuads());
}

TEST_CASE("BillboardDraw HealthBar is atomic under Snapshot", "[billboard]")
{
    fra::BillboardDraw draw;
    constexpr int      kBars = 200;

    std::thread producer([&] {
        for (int i = 0; i < kBars; ++i)
        {
            draw.HealthBar(glm::vec3(static_cast<float>(i), 0.f, 0.f), 1.f,
                           0.1f, 0.5f, glm::vec4(0.f), glm::vec4(1.f));
        }
    });

    std::vector<fra::Billboard> snap;
    for (int i = 0; i < kBars * 4; ++i)
    {
        draw.Snapshot(snap);
        REQUIRE((snap.size() % 2) == 0);
        REQUIRE(snap.size() <= draw.MaxQuads());
    }
    producer.join();

    draw.Snapshot(snap);
    REQUIRE(snap.size() == static_cast<std::size_t>(kBars * 2));
}
