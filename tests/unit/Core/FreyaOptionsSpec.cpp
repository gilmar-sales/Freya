#include <Freya/FreyaOptions.hpp>

#include <gtest/gtest.h>

TEST(FreyaOptions, ScaledExtentNeverReturnsZeroAxis)
{
    const auto e = fra::ScaledExtent({ 1920, 1080 }, 2);
    EXPECT_EQ(e.width, 960u);
    EXPECT_EQ(e.height, 540u);

    const auto tiny = fra::ScaledExtent({ 1, 1 }, 8);
    EXPECT_EQ(tiny.width, 1u);
    EXPECT_EQ(tiny.height, 1u);

    const auto zeroDiv = fra::ScaledExtent({ 64, 64 }, 0);
    EXPECT_EQ(zeroDiv.width, 64u);
    EXPECT_EQ(zeroDiv.height, 64u);
}

TEST(FreyaOptions, ApplyShadowQualityOffDisablesShadows)
{
    fra::FreyaOptions o;
    o.enableShadows = true;
    fra::ApplyShadowQuality(o, fra::ShadowQuality::Off);
    EXPECT_FALSE(o.enableShadows);
}

TEST(FreyaOptions, ApplyShadowQualityHighSetsCascadeAndMapSize)
{
    fra::FreyaOptions o;
    fra::ApplyShadowQuality(o, fra::ShadowQuality::High);
    EXPECT_TRUE(o.enableShadows);
    EXPECT_EQ(o.shadowMapResolution, 2048u);
    EXPECT_EQ(o.shadowCascadeCount, 4u);
    EXPECT_EQ(o.shadowSampleCount, 16u);
    EXPECT_FLOAT_EQ(o.shadowCascadeBlend, 0.05f);
    EXPECT_FLOAT_EQ(o.shadowCascadeDistance, 80.0f);
    EXPECT_EQ(o.shadowPointResolutionDivisor, 1u);
    EXPECT_EQ(o.shadowSpotResolutionDivisor, 1u);
    EXPECT_EQ(o.shadowMaskResolutionDivisor, 1u);
    EXPECT_EQ(o.shadowCascadeUpdatePeriod, 2u);
    EXPECT_FALSE(o.enableShadowMask);
    EXPECT_EQ(fra::ResolveShadowSideResolution(o.shadowMapResolution,
                                               o.shadowPointResolution,
                                               o.shadowPointResolutionDivisor),
              2048u);
}

TEST(FreyaOptions, ApplyShadowQualityUltraExceedsHigh)
{
    fra::FreyaOptions o;
    fra::ApplyShadowQuality(o, fra::ShadowQuality::Ultra);
    EXPECT_TRUE(o.enableShadows);
    EXPECT_EQ(o.shadowMapResolution, 4096u);
    EXPECT_EQ(o.shadowCascadeCount, 4u);
    EXPECT_EQ(o.shadowSampleCount, 16u);
    EXPECT_FLOAT_EQ(o.shadowCascadeBlend, 0.1f);
    EXPECT_FLOAT_EQ(o.shadowCascadeDistance, 120.0f);
    EXPECT_EQ(o.shadowPointResolutionDivisor, 1u);
    EXPECT_EQ(o.shadowSpotResolutionDivisor, 1u);
    EXPECT_EQ(o.shadowMaskResolutionDivisor, 1u);
    EXPECT_EQ(o.shadowCascadeUpdatePeriod, 1u);
    EXPECT_EQ(o.shadowPointUpdatePeriod, 1u);
    EXPECT_FALSE(o.enableShadowMask);
    EXPECT_EQ(fra::ResolveShadowSideResolution(o.shadowMapResolution,
                                               o.shadowPointResolution,
                                               o.shadowPointResolutionDivisor),
              4096u);
}

TEST(FreyaOptions, AnimLodTickFiresAtRequestedRate)
{
    float accum = 0.f;
    EXPECT_FALSE(fra::ConsumeAnimLodTick(accum, 0.008f, 30.f));
    EXPECT_TRUE(fra::ConsumeAnimLodTick(accum, 0.03f, 30.f));

    float always = 1.f;
    EXPECT_TRUE(fra::ConsumeAnimLodTick(always, 0.016f, 1e6f));
    EXPECT_FLOAT_EQ(always, 0.f);
}

TEST(FreyaOptions, AnimLodHzIgnoresTiersWhenLodDisabled)
{
    fra::FreyaOptions o;
    o.enableAnimLod = false;
    EXPECT_GE(fra::AnimLodHz(o, 3), 1e5f);
}
