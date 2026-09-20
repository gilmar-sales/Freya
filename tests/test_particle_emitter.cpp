#include <Freya/Core/BillboardDraw.hpp>
#include <Freya/Core/ParticleEmitter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>
#include <vector>

TEST_CASE("ParticleEmitter concurrent Tick on shared BillboardDraw",
          "[particle]")
{
    fra::BillboardDraw draw(8192);
    constexpr int      kEmitters = 4;
    constexpr int      kIters    = 200;

    std::vector<fra::ParticleEmitter> emitters(kEmitters);
    for (auto& e : emitters)
    {
        e.spawnRate    = 120.f;
        e.maxParticles = 128;
        e.lifetime     = 0.4f;
    }

    std::atomic<int> ready { 0 };
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(kEmitters));

    for (int t = 0; t < kEmitters; ++t)
    {
        threads.emplace_back(
            [&, t]
            {
                ready.fetch_add(1, std::memory_order_relaxed);
                while (ready.load(std::memory_order_relaxed) < kEmitters)
                {
                }
                for (int i = 0; i < kIters; ++i)
                    emitters[static_cast<std::size_t>(t)].Tick(1.f / 60.f,
                                                               draw);
            });
    }

    for (auto& th : threads)
        th.join();

    std::vector<fra::Billboard> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() <= draw.MaxQuads());
    REQUIRE_FALSE(snap.empty());
}

TEST_CASE("ParticleEmitter serializes Tick on the same instance", "[particle]")
{
    fra::BillboardDraw   draw(4096);
    fra::ParticleEmitter emitter;
    emitter.spawnRate    = 60.f;
    emitter.maxParticles = 64;
    emitter.lifetime     = 0.5f;

    constexpr int        kThreads = 8;
    constexpr int        kIters   = 100;
    std::atomic<int>     ready { 0 };
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
                    emitter.Tick(1.f / 60.f, draw);
            });
    }

    for (auto& th : threads)
        th.join();

    std::vector<fra::Billboard> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() <= draw.MaxQuads());
    REQUIRE_FALSE(snap.empty());
}
