#include <Freya/Core/UiDraw.hpp>
#include <Freya/Core/UiTypes.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using Catch::Matchers::WithinAbs;

TEST_CASE("UiRect Contains Expand Intersect", "[ui]")
{
    fra::UiRect a { 10, 10, 50, 40 };
    REQUIRE(a.Contains(20, 20));
    REQUIRE_FALSE(a.Contains(5, 5));
    REQUIRE(a.Contains(10, 10));
    REQUIRE_FALSE(a.Contains(60, 50));

    const auto expanded = a.Expand(2.f);
    REQUIRE_THAT(expanded.x, WithinAbs(8.f, 1e-5f));
    REQUIRE_THAT(expanded.w, WithinAbs(54.f, 1e-5f));

    auto b = a.Intersect({ 40, 30, 40, 40 });
    REQUIRE_THAT(b.w, WithinAbs(20.f, 1e-5f));
    REQUIRE_THAT(b.h, WithinAbs(20.f, 1e-5f));

    auto empty = a.Intersect({ 200, 200, 10, 10 });
    REQUIRE(empty.w <= 0.f);
}

TEST_CASE("UiDraw ProgressBar emits two quads", "[ui]")
{
    fra::UiDraw draw;
    draw.ProgressBar({ 0, 0, 100, 10 }, 0.5f, { 0, 0, 0, 1 }, { 1, 0, 0, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 2);
    REQUIRE_THAT(snap[1].clipMax, WithinAbs(0.5f, 1e-5f));
    REQUIRE((snap[1].flags & fra::kUiFlagClipU) != 0u);
}

TEST_CASE("UiDraw CooldownRadial emits clip-radial quad", "[ui]")
{
    fra::UiDraw draw;
    draw.CooldownRadial({ 0, 0, 64, 64 }, 0.75f, { 0, 0, 0, 0.6f });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 1);
    REQUIRE((snap[0].flags & fra::kUiFlagClipRadial) != 0u);
    REQUIRE_THAT(snap[0].clipMax, WithinAbs(0.75f, 1e-5f));

    draw.Clear();
    draw.CooldownRadial({ 0, 0, 64, 64 }, 0.f);
    draw.Snapshot(snap);
    REQUIRE(snap.empty());
}


TEST_CASE("UiDraw soft-caps at MaxQuads", "[ui]")
{
    fra::UiDraw draw(16);
    for (int i = 0; i < 64; ++i)
        draw.Rect({ static_cast<float>(i), 0, 1, 1 }, { 1, 1, 1, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 16);
    REQUIRE(snap.size() == draw.MaxQuads());
}

TEST_CASE("UiDraw overlay quads append after base", "[ui]")
{
    fra::UiDraw draw;
    draw.Rect({ 0, 0, 10, 10 }, { 1, 0, 0, 1 });
    draw.BeginOverlay();
    draw.Rect({ 20, 20, 10, 10 }, { 0, 1, 0, 1 });
    draw.EndOverlay();
    draw.Rect({ 40, 40, 10, 10 }, { 0, 0, 1, 1 });

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 3);
    REQUIRE_THAT(snap[0].rect.x, WithinAbs(0.f, 1e-5f));
    REQUIRE_THAT(snap[1].rect.x, WithinAbs(40.f, 1e-5f));
    REQUIRE_THAT(snap[2].rect.x, WithinAbs(20.f, 1e-5f));
}

TEST_CASE("UiDraw Clear resets overlay depth", "[ui]")
{
    fra::UiDraw draw;
    draw.BeginOverlay();
    draw.Rect({ 1, 1, 1, 1 }, { 1, 1, 1, 1 });
    draw.Clear();
    REQUIRE(draw.Empty());
    draw.Rect({ 2, 2, 1, 1 }, { 1, 1, 1, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 1);
    REQUIRE_THAT(snap[0].rect.x, WithinAbs(2.f, 1e-5f));
}

TEST_CASE("UiDraw Quads batch append under cap", "[ui]")
{
    fra::UiDraw draw(8);
    std::vector<fra::UiQuad> batch(4);
    for (std::size_t i = 0; i < batch.size(); ++i)
    {
        batch[i].rect  = { static_cast<float>(i), 0, 1, 1 };
        batch[i].color = { 1, 1, 1, 1 };
    }
    draw.Quads(batch);
    draw.Quads(batch);
    draw.Quads(batch);
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() == 8);
}

TEST_CASE("UiDraw concurrent Rect and Snapshot", "[ui][thread]")
{
    fra::UiDraw      draw(4096);
    std::atomic<int> ready { 0 };
    constexpr int    kWorkers = 8;
    constexpr int    kIters   = 400;

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

TEST_CASE("UiDraw concurrent mixed producers with Snapshot", "[ui][thread]")
{
    fra::UiDraw      draw(8192);
    std::atomic<int> ready { 0 };
    constexpr int    kProducers = 6;
    constexpr int    kIters     = 300;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kProducers) + 2);

    for (int t = 0; t < kProducers; ++t)
    {
        threads.emplace_back(
            [&, t]
            {
                ready.fetch_add(1, std::memory_order_relaxed);
                while (ready.load(std::memory_order_relaxed) < kProducers + 2)
                {
                }
                for (int i = 0; i < kIters; ++i)
                {
                    const float x = static_cast<float>(t * 1000 + i);
                    if ((i & 1) == 0)
                    {
                        draw.Rect({ x, 0.f, 8.f, 8.f }, { 1, 1, 1, 1 });
                    }
                    else
                    {
                        draw.ProgressBar({ x, 10.f, 40.f, 6.f }, 0.25f,
                                         { 0, 0, 0, 1 }, { 1, 0, 0, 1 });
                    }
                    if ((i & 7) == 0)
                    {
                        fra::UiQuad q {};
                        q.rect         = { x, 20.f, 4.f, 4.f };
                        q.color        = { 0.5f, 0.5f, 0.5f, 1.f };
                        q.textureIndex = static_cast<std::uint32_t>(t + 2);
                        draw.Quad(q);
                    }
                }
            });
    }

    threads.emplace_back(
        [&]
        {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < kProducers + 2)
            {
            }
            for (int i = 0; i < kIters; ++i)
            {
                draw.BeginOverlay();
                draw.Rect({ static_cast<float>(i), 100.f, 12.f, 12.f },
                          { 0.2f, 0.8f, 0.2f, 0.9f });
                draw.EndOverlay();
            }
        });

    threads.emplace_back(
        [&]
        {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < kProducers + 2)
            {
            }
            std::vector<fra::UiQuad> snap;
            for (int i = 0; i < kIters * 2; ++i)
            {
                draw.Snapshot(snap);
                REQUIRE(snap.size() <= draw.MaxQuads());
                for (const auto& q : snap)
                {
                    REQUIRE(q.rect.z >= 0.f);
                    REQUIRE(q.rect.w >= 0.f);
                }
                if ((i & 31) == 0)
                    draw.Clear();
            }
        });

    for (auto& th : threads)
        th.join();

    std::vector<fra::UiQuad> finalSnap;
    draw.Snapshot(finalSnap);
    REQUIRE(finalSnap.size() <= draw.MaxQuads());
}

TEST_CASE("UiDraw Image by bindless index is atomic under Snapshot",
          "[ui][thread]")
{
    fra::UiDraw draw(2048);
    constexpr int kImages = 250;

    std::thread producer(
        [&]
        {
            fra::UiImageOpts opts {};
            opts.fit  = fra::UiImageFit::Stretch;
            opts.tint = { 1.f, 1.f, 1.f, 0.8f };
            for (int i = 0; i < kImages; ++i)
            {
                draw.Image({ static_cast<float>(i), 0.f, 16.f, 16.f },
                           static_cast<std::uint32_t>(i % 7) + 2u, opts);
            }
        });

    std::vector<fra::UiQuad> snap;
    for (int i = 0; i < kImages * 3; ++i)
    {
        draw.Snapshot(snap);
        REQUIRE(snap.size() <= draw.MaxQuads());
    }
    producer.join();

    draw.Snapshot(snap);
    REQUIRE(snap.size() == static_cast<std::size_t>(kImages));
}

TEST_CASE("UiDraw concurrent overlay and base do not tear Snapshot",
          "[ui][thread]")
{
    fra::UiDraw      draw(4096);
    std::atomic<bool> stop { false };
    std::atomic<int>  ready { 0 };

    std::thread baseWriter(
        [&]
        {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < 3)
            {
            }
            int i = 0;
            while (!stop.load(std::memory_order_relaxed))
            {
                draw.Rect({ static_cast<float>(i++ % 100), 0, 5, 5 },
                          { 1, 1, 1, 1 });
            }
        });

    std::thread overlayWriter(
        [&]
        {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < 3)
            {
            }
            int i = 0;
            while (!stop.load(std::memory_order_relaxed))
            {
                draw.BeginOverlay();
                draw.Rect({ static_cast<float>(i++ % 100), 50, 5, 5 },
                          { 0, 1, 0, 1 });
                draw.EndOverlay();
            }
        });

    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_relaxed) < 3)
    {
    }

    std::vector<fra::UiQuad> snap;
    for (int i = 0; i < 800; ++i)
    {
        draw.Snapshot(snap);
        REQUIRE(snap.size() <= draw.MaxQuads());
        if ((i & 15) == 0)
            draw.Clear();
    }
    stop.store(true, std::memory_order_relaxed);
    baseWriter.join();
    overlayWriter.join();
}
