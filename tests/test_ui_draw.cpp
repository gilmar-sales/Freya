#include <Freya/Core/UiDraw.hpp>
#include <Freya/Core/UiModelPreview.hpp>
#include <Freya/Core/UiTypes.hpp>
#include <Freya/Scene/AssetHandle.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

TEST(UiDraw, RectContainsExpandIntersect)
{
    fra::UiRect a { 10, 10, 50, 40 };
    EXPECT_TRUE(a.Contains(20, 20));
    EXPECT_FALSE(a.Contains(5, 5));
    EXPECT_TRUE(a.Contains(10, 10));
    EXPECT_FALSE(a.Contains(60, 50));

    const auto expanded = a.Expand(2.f);
    EXPECT_NEAR(expanded.x, 8.f, 1e-5f);
    EXPECT_NEAR(expanded.w, 54.f, 1e-5f);

    auto b = a.Intersect({ 40, 30, 40, 40 });
    EXPECT_NEAR(b.w, 20.f, 1e-5f);
    EXPECT_NEAR(b.h, 20.f, 1e-5f);

    auto empty = a.Intersect({ 200, 200, 10, 10 });
    EXPECT_LE(empty.w, 0.f);
}

TEST(UiDraw, ProgressBarEmitsTwoQuads)
{
    fra::UiDraw draw;
    draw.ProgressBar({ 0, 0, 100, 10 }, 0.5f, { 0, 0, 0, 1 }, { 1, 0, 0, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), 2u);
    EXPECT_NEAR(snap[1].clipMax, 0.5f, 1e-5f);
    EXPECT_NE(snap[1].flags & fra::kUiFlagClipU, 0u);
}

TEST(UiDraw, CooldownRadialEmitsClipRadialQuad)
{
    fra::UiDraw draw;
    draw.CooldownRadial({ 0, 0, 64, 64 }, 0.75f, { 0, 0, 0, 0.6f });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), 1u);
    EXPECT_NE(snap[0].flags & fra::kUiFlagClipRadial, 0u);
    EXPECT_NEAR(snap[0].clipMax, 0.75f, 1e-5f);

    draw.Clear();
    draw.CooldownRadial({ 0, 0, 64, 64 }, 0.f);
    draw.Snapshot(snap);
    EXPECT_TRUE(snap.empty());
}

TEST(UiDraw, SoftCapsAtMaxQuads)
{
    fra::UiDraw draw(16);
    for (int i = 0; i < 64; ++i)
        draw.Rect({ static_cast<float>(i), 0, 1, 1 }, { 1, 1, 1, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), 16u);
    EXPECT_EQ(snap.size(), draw.MaxQuads());
}

TEST(UiDraw, OverlayQuadsAppendAfterBase)
{
    fra::UiDraw draw;
    draw.Rect({ 0, 0, 10, 10 }, { 1, 0, 0, 1 });
    draw.BeginOverlay();
    draw.Rect({ 20, 20, 10, 10 }, { 0, 1, 0, 1 });
    draw.EndOverlay();
    draw.Rect({ 40, 40, 10, 10 }, { 0, 0, 1, 1 });

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), 3u);
    EXPECT_NEAR(snap[0].rect.x, 0.f, 1e-5f);
    EXPECT_NEAR(snap[1].rect.x, 40.f, 1e-5f);
    EXPECT_NEAR(snap[2].rect.x, 20.f, 1e-5f);
}

TEST(UiDraw, ClearResetsOverlayDepth)
{
    fra::UiDraw draw;
    draw.BeginOverlay();
    draw.Rect({ 1, 1, 1, 1 }, { 1, 1, 1, 1 });
    draw.Clear();
    EXPECT_TRUE(draw.Empty());
    draw.Rect({ 2, 2, 1, 1 }, { 1, 1, 1, 1 });
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), 1u);
    EXPECT_NEAR(snap[0].rect.x, 2.f, 1e-5f);
}

TEST(UiDraw, QuadsBatchAppendUnderCap)
{
    fra::UiDraw              draw(8);
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
    EXPECT_EQ(snap.size(), 8u);
}

TEST(UiDraw, ConcurrentRectAndSnapshot)
{
    fra::UiDraw      draw(4096);
    std::atomic<int> ready { 0 };
    constexpr int    kWorkers = 8;
    constexpr int    kIters   = 400;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kWorkers) + 1);

    for (int t = 0; t < kWorkers; ++t)
    {
        threads.emplace_back([&] {
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

    threads.emplace_back([&] {
        ready.fetch_add(1, std::memory_order_relaxed);
        while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
        {
        }
        std::vector<fra::UiQuad> snap;
        for (int i = 0; i < kIters; ++i)
        {
            draw.Snapshot(snap);
            EXPECT_LE(snap.size(), draw.MaxQuads());
            if ((i & 63) == 0)
                draw.Clear();
        }
    });

    for (auto& th : threads)
        th.join();

    std::vector<fra::UiQuad> finalSnap;
    draw.Snapshot(finalSnap);
    EXPECT_LE(finalSnap.size(), draw.MaxQuads());
}

TEST(UiDraw, ConcurrentMixedProducersWithSnapshot)
{
    fra::UiDraw      draw(8192);
    std::atomic<int> ready { 0 };
    constexpr int    kProducers = 6;
    constexpr int    kIters     = 300;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kProducers) + 2);

    for (int t = 0; t < kProducers; ++t)
    {
        threads.emplace_back([&, t] {
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

    threads.emplace_back([&] {
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

    threads.emplace_back([&] {
        ready.fetch_add(1, std::memory_order_relaxed);
        while (ready.load(std::memory_order_relaxed) < kProducers + 2)
        {
        }
        std::vector<fra::UiQuad> snap;
        for (int i = 0; i < kIters * 2; ++i)
        {
            draw.Snapshot(snap);
            EXPECT_LE(snap.size(), draw.MaxQuads());
            for (const auto& q : snap)
            {
                EXPECT_GE(q.rect.z, 0.f);
                EXPECT_GE(q.rect.w, 0.f);
            }
            if ((i & 31) == 0)
                draw.Clear();
        }
    });

    for (auto& th : threads)
        th.join();

    std::vector<fra::UiQuad> finalSnap;
    draw.Snapshot(finalSnap);
    EXPECT_LE(finalSnap.size(), draw.MaxQuads());
}

TEST(UiDraw, ImageByBindlessIndexIsAtomicUnderSnapshot)
{
    fra::UiDraw   draw(2048);
    constexpr int kImages = 250;

    std::thread producer([&] {
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
        EXPECT_LE(snap.size(), draw.MaxQuads());
    }
    producer.join();

    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), static_cast<std::size_t>(kImages));
}

TEST(UiDraw, ConcurrentOverlayAndBaseDoNotTearSnapshot)
{
    fra::UiDraw       draw(4096);
    std::atomic<bool> stop { false };
    std::atomic<int>  ready { 0 };

    std::thread baseWriter([&] {
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

    std::thread overlayWriter([&] {
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
        EXPECT_LE(snap.size(), draw.MaxQuads());
        if ((i & 15) == 0)
            draw.Clear();
    }
    stop.store(true, std::memory_order_relaxed);
    baseWriter.join();
    overlayWriter.join();
}

TEST(UiModelPreview, ClampPitchRespectsMinMax)
{
    fra::UiModelPreviewOrbit orbit {};
    orbit.minPitch = -45.f;
    orbit.maxPitch = 30.f;
    EXPECT_FLOAT_EQ(orbit.ClampPitch(-90.f), -45.f);
    EXPECT_FLOAT_EQ(orbit.ClampPitch(90.f), 30.f);
    EXPECT_FLOAT_EQ(orbit.ClampPitch(10.f), 10.f);
}

TEST(UiDraw, ImageTextureHandleConcurrentWithSnapshot)
{
    // Static / snapshot handles share the same bindless Image path as live
    // RTs — workers may enqueue while Snapshot runs.
    fra::UiDraw              draw(4096);
    const fra::TextureHandle portrait { 7 }; // pretend snapshot id

    std::atomic<bool> stop { false };
    std::thread       producer([&] {
        fra::UiImageOpts opts {};
        while (!stop.load(std::memory_order_relaxed))
        {
            draw.Image({ 8.f, 8.f, 64.f, 64.f }, portrait, opts);
            draw.Image({ 80.f, 8.f, 32.f, 32.f }, static_cast<std::uint32_t>(9),
                       opts);
        }
    });

    std::vector<fra::UiQuad> snap;
    for (int i = 0; i < 600; ++i)
    {
        draw.Snapshot(snap);
        EXPECT_LE(snap.size(), draw.MaxQuads());
        if ((i & 31) == 0)
            draw.Clear();
    }
    stop.store(true, std::memory_order_relaxed);
    producer.join();
}
