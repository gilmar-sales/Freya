#include <Freya/Core/BillboardDraw.hpp>
#include <Freya/Core/RibbonEmitter.hpp>
#include <Freya/Core/SplineRope.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

TEST(BillboardDraw, ConcurrentQuadAndSnapshot)
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
            EXPECT_LE(snap.size(), draw.MaxQuads());
            if ((i & 63) == 0)
                draw.Clear();
        }
    });

    for (auto& th : threads)
        th.join();

    std::vector<fra::Billboard> finalSnap;
    draw.Snapshot(finalSnap);
    EXPECT_LE(finalSnap.size(), draw.MaxQuads());
}

TEST(BillboardDraw, HealthBarIsAtomicUnderSnapshot)
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
        EXPECT_EQ(snap.size() % 2, 0u);
        EXPECT_LE(snap.size(), draw.MaxQuads());
    }
    producer.join();

    draw.Snapshot(snap);
    EXPECT_EQ(snap.size(), static_cast<std::size_t>(kBars * 2));
}

namespace
{
    fra::StripStyle testStyle()
    {
        fra::StripStyle s {};
        s.miterLimit = 2.5f;
        return s;
    }

    const glm::vec3 kRight { 1.f, 0.f, 0.f };
    const glm::vec3 kUp { 0.f, 1.f, 0.f };

    fra::StripPoint pt(float x, float y, float z, float width = 0.5f,
                       float u = 0.f)
    {
        fra::StripPoint p {};
        p.pos   = glm::vec3(x, y, z);
        p.width = width;
        p.u     = u;
        return p;
    }
} // namespace

TEST(BillboardDraw, StripNeedsTwoPoints)
{
    fra::BillboardDraw    draw;
    const fra::StripStyle style = testStyle();

    std::vector<fra::StripPoint> empty;
    draw.Strip(empty, style, kRight, kUp);
    EXPECT_TRUE(draw.ConnectedEmpty());

    std::vector<fra::StripPoint> single { pt(0.f, 0.f, 0.f) };
    draw.Strip(single, style, kRight, kUp);
    EXPECT_TRUE(draw.ConnectedEmpty());
}

TEST(BillboardDraw, StripStraightSharesEdgesExactly)
{
    fra::BillboardDraw           draw;
    std::vector<fra::StripPoint> points { pt(0.f, 0.f, 0.f, 0.5f, 0.f),
                                          pt(1.f, 0.f, 0.f, 0.5f, 0.5f),
                                          pt(2.f, 0.f, 0.f, 0.5f, 1.f) };
    draw.Strip(points, testStyle(), kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 2u);

    // Shared edge vertices are bitwise identical: no gaps, no overlaps.
    EXPECT_EQ(snap[0].c2, snap[1].c0);
    EXPECT_EQ(snap[0].c3, snap[1].c1);

    // Constant width: straight ribbon keeps the full half-width.
    EXPECT_FLOAT_EQ(glm::distance(snap[0].c0, snap[0].c1), 0.5f);
    EXPECT_FLOAT_EQ(snap[0].c0.y, 0.25f);
    EXPECT_FLOAT_EQ(snap[0].c1.y, -0.25f);

    // U runs along the strip, V across.
    EXPECT_EQ(snap[0].uvRect, glm::vec4(0.f, 0.f, 0.5f, 1.f));
    EXPECT_EQ(snap[1].uvRect, glm::vec4(0.5f, 0.f, 1.f, 1.f));
}

TEST(BillboardDraw, StripCornerSharesMiteredEdge)
{
    fra::BillboardDraw           draw;
    std::vector<fra::StripPoint> points { pt(0.f, 0.f, 0.f, 0.5f, 0.f),
                                          pt(1.f, 0.f, 0.f, 0.5f, 0.5f),
                                          pt(1.f, 1.f, 0.f, 0.5f, 1.f) };
    draw.Strip(points, testStyle(), kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap[0].c2, snap[1].c0);
    EXPECT_EQ(snap[0].c3, snap[1].c1);

    // 90-degree joint: miter extends by 1/cos(45deg) = sqrt(2).
    const float jointHalf = glm::distance(snap[1].c0, glm::vec3(1.f, 0.f, 0.f));
    EXPECT_NEAR(jointHalf, 0.25f * std::sqrt(2.f), 1e-5f);
}

TEST(BillboardDraw, StripSkipsDegenerateRuns)
{
    fra::BillboardDraw           draw;
    std::vector<fra::StripPoint> points {
        pt(0.f, 0.f, 0.f), pt(0.f, 0.f, 0.f), pt(1.f, 0.f, 0.f),
        pt(1.f, 0.f, 0.f), pt(1.f, 0.f, 0.f), pt(2.f, 0.f, 0.f)
    };
    draw.Strip(points, testStyle(), kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 2u);
    for (const auto& q : snap)
    {
        for (const glm::vec3* c : { &q.c0, &q.c1, &q.c2, &q.c3 })
            EXPECT_TRUE(std::isfinite(c->x) && std::isfinite(c->y) &&
                        std::isfinite(c->z));
    }
    EXPECT_EQ(snap[0].c2, snap[1].c0);
    EXPECT_EQ(snap[0].c3, snap[1].c1);
}

TEST(BillboardDraw, StripAllCoincidentIsNoOp)
{
    fra::BillboardDraw           draw;
    std::vector<fra::StripPoint> points { pt(1.f, 2.f, 3.f), pt(1.f, 2.f, 3.f),
                                          pt(1.f, 2.f, 3.f) };
    draw.Strip(points, testStyle(), kRight, kUp);
    EXPECT_TRUE(draw.ConnectedEmpty());
}

TEST(BillboardDraw, StripHairpinStaysBounded)
{
    fra::BillboardDraw draw;
    fra::StripStyle    style = testStyle();
    // Full reversal: bevel fallback keeps the half-width, never spikes.
    std::vector<fra::StripPoint> points { pt(0.f, 0.f, 0.f, 0.4f),
                                          pt(1.f, 0.f, 0.f, 0.4f),
                                          pt(0.f, 0.f, 0.f, 0.4f) };
    draw.Strip(points, style, kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap[0].c2, snap[1].c0);
    for (const auto& q : snap)
    {
        for (const glm::vec3* c : { &q.c0, &q.c1, &q.c2, &q.c3 })
        {
            EXPECT_TRUE(std::isfinite(c->x) && std::isfinite(c->y) &&
                        std::isfinite(c->z));
            EXPECT_LE(glm::distance(*c, glm::vec3(0.5f, 0.f, 0.f)), 1.0f);
        }
    }
}

TEST(BillboardDraw, StripRespectsMaxQuads)
{
    fra::BillboardDraw           draw(4);
    std::vector<fra::StripPoint> points;
    for (int i = 0; i < 10; ++i)
        points.push_back(pt(static_cast<float>(i), 0.f, 0.f));
    draw.Strip(points, testStyle(), kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    EXPECT_LE(snap.size(), 4u);
    EXPECT_EQ(snap.size(), 4u);
}

TEST(BillboardDraw, ConnectedQuadRoundTrip)
{
    fra::BillboardDraw      draw;
    fra::ConnectedBillboard q {};
    q.c0           = glm::vec3(0.f, 0.25f, 0.f);
    q.c1           = glm::vec3(0.f, -0.25f, 0.f);
    q.c2           = glm::vec3(1.f, 0.25f, 0.f);
    q.c3           = glm::vec3(1.f, -0.25f, 0.f);
    q.color0       = glm::vec4(1.f, 0.f, 0.f, 1.f);
    q.color1       = glm::vec4(0.f, 1.f, 0.f, 1.f);
    q.blend        = fra::BillboardBlend::Additive;
    q.textureIndex = 7;
    draw.ConnectedQuad(q);

    EXPECT_FALSE(draw.ConnectedEmpty());
    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].c0, q.c0);
    EXPECT_EQ(snap[0].c3, q.c3);
    EXPECT_EQ(snap[0].color0, q.color0);
    EXPECT_EQ(snap[0].textureIndex, 7u);

    // Regular queue untouched; Empty() covers both queues.
    std::vector<fra::Billboard> quads;
    draw.Snapshot(quads);
    EXPECT_TRUE(quads.empty());
    EXPECT_FALSE(draw.Empty());
    draw.Clear();
    EXPECT_TRUE(draw.Empty());
    EXPECT_TRUE(draw.ConnectedEmpty());
}

TEST(BillboardDraw, SplineRopeSubmitsSeamlessStrip)
{
    fra::SplineRope rope;
    rope.controlPoints = { glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f),
                           glm::vec3(2.f, 0.5f, 0.f),
                           glm::vec3(3.f, 0.5f, 0.f) };
    rope.baseRadius    = 0.06f;
    rope.tipRadius     = 0.02f;
    rope.segments      = 8;
    rope.growT         = 1.0f;

    fra::BillboardDraw draw;
    rope.Submit(draw, kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 8u);

    std::vector<fra::Billboard> quads;
    draw.Snapshot(quads);
    EXPECT_TRUE(quads.empty());

    // Every joint shares edge vertices exactly.
    for (std::size_t i = 0; i + 1 < snap.size(); ++i)
    {
        EXPECT_EQ(snap[i].c2, snap[i + 1].c0);
        EXPECT_EQ(snap[i].c3, snap[i + 1].c1);
    }

    // Root width matches the base diameter, tip the tip diameter.
    EXPECT_NEAR(glm::distance(snap.front().c0, snap.front().c1),
                2.0f * rope.baseRadius, 1e-5f);
    EXPECT_NEAR(glm::distance(snap.back().c2, snap.back().c3),
                2.0f * rope.tipRadius, 1e-5f);
}

TEST(BillboardDraw, SplineRopeGrowthTrimsTip)
{
    fra::SplineRope rope;
    rope.controlPoints = { glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f) };
    rope.segments      = 8;

    fra::BillboardDraw draw;
    rope.growT = 0.0f;
    rope.Submit(draw, kRight, kUp);
    EXPECT_TRUE(draw.ConnectedEmpty());

    rope.growT = 0.5f;
    rope.Submit(draw, kRight, kUp);
    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    EXPECT_EQ(snap.size(), 4u);
}

TEST(BillboardDraw, RibbonEmitterSubmitsConnectedStrip)
{
    fra::RibbonEmitter ribbon;
    ribbon.width  = 0.2f;
    ribbon.origin = glm::vec3(0.f);

    fra::BillboardDraw draw;
    ribbon.Tick(1.0f / 60.0f, draw, kRight, kUp);
    ribbon.origin = glm::vec3(1.f, 0.f, 0.f);
    ribbon.Tick(1.0f / 60.0f, draw, kRight, kUp);
    ribbon.origin = glm::vec3(1.f, 1.f, 0.f);
    draw.Clear();
    ribbon.Tick(1.0f / 60.0f, draw, kRight, kUp);

    std::vector<fra::ConnectedBillboard> snap;
    draw.SnapshotConnected(snap);
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap[0].c2, snap[1].c0);
    EXPECT_EQ(snap[0].c3, snap[1].c1);

    // Legacy UV orientation preserved: U across, V along the trail.
    EXPECT_EQ(snap[0].uvRect, glm::vec4(0.f, 0.f, 1.f, 0.5f));
}

TEST(BillboardDraw, ConcurrentStripAndSnapshot)
{
    fra::BillboardDraw draw(512);
    std::atomic<int>   ready { 0 };
    constexpr int      kWorkers = 4;
    constexpr int      kIters   = 200;

    std::vector<fra::StripPoint> points { pt(0.f, 0.f, 0.f, 0.3f, 0.f),
                                          pt(1.f, 0.f, 0.f, 0.3f, 0.5f),
                                          pt(1.f, 1.f, 0.f, 0.3f, 1.f) };
    const fra::StripStyle        style = testStyle();

    std::vector<std::thread> threads;
    for (int t = 0; t < kWorkers; ++t)
    {
        threads.emplace_back([&] {
            ready.fetch_add(1, std::memory_order_relaxed);
            while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
            {
            }
            for (int i = 0; i < kIters; ++i)
                draw.Strip(points, style, kRight, kUp);
        });
    }
    threads.emplace_back([&] {
        ready.fetch_add(1, std::memory_order_relaxed);
        while (ready.load(std::memory_order_relaxed) < kWorkers + 1)
        {
        }
        std::vector<fra::ConnectedBillboard> snap;
        for (int i = 0; i < kIters; ++i)
        {
            draw.SnapshotConnected(snap);
            EXPECT_LE(snap.size(), draw.MaxQuads());
        }
    });

    for (auto& th : threads)
        th.join();

    std::vector<fra::ConnectedBillboard> finalSnap;
    draw.SnapshotConnected(finalSnap);
    EXPECT_LE(finalSnap.size(), draw.MaxQuads());
}
