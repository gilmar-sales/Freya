#include <Freya/FreyaOptions.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("ScaledExtent never returns a zero axis", "[options]")
{
    const auto e = fra::ScaledExtent({ 1920, 1080 }, 2);
    REQUIRE(e.width == 960);
    REQUIRE(e.height == 540);

    const auto tiny = fra::ScaledExtent({ 1, 1 }, 8);
    REQUIRE(tiny.width == 1);
    REQUIRE(tiny.height == 1);

    const auto zeroDiv = fra::ScaledExtent({ 64, 64 }, 0);
    REQUIRE(zeroDiv.width == 64);
    REQUIRE(zeroDiv.height == 64);
}

TEST_CASE("ApplyShadowQuality Off disables shadows", "[options]")
{
    fra::FreyaOptions o;
    o.enableShadows = true;
    fra::ApplyShadowQuality(o, fra::ShadowQuality::Off);
    REQUIRE_FALSE(o.enableShadows);
}

TEST_CASE("ApplyShadowQuality High sets cascade and map size", "[options]")
{
    fra::FreyaOptions o;
    fra::ApplyShadowQuality(o, fra::ShadowQuality::High);
    REQUIRE(o.enableShadows);
    REQUIRE(o.shadowMapResolution == 2048);
    REQUIRE(o.shadowCascadeCount == 4);
    REQUIRE(o.shadowSampleCount == 16);
}

TEST_CASE("AnimLodTick fires at the requested rate", "[options]")
{
    float accum = 0.f;
    REQUIRE_FALSE(fra::ConsumeAnimLodTick(accum, 0.008f, 30.f));
    REQUIRE(fra::ConsumeAnimLodTick(accum, 0.03f, 30.f));

    float always = 1.f;
    REQUIRE(fra::ConsumeAnimLodTick(always, 0.016f, 1e6f));
    REQUIRE(always == Catch::Approx(0.f));
}

TEST_CASE("AnimLodHz ignores tiers when lod is disabled", "[options]")
{
    fra::FreyaOptions o;
    o.enableAnimLod = false;
    REQUIRE(fra::AnimLodHz(o, 3) >= 1e5f);
}
