#include <Freya/Internal/IrradianceSh.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <numbers>
#include <vector>

namespace
{
    constexpr int kW = 64;
    constexpr int kH = 32;

    // Bakes `fn(n)` into an RGBA32F map using the same texel -> normal
    // convention as the lighting shader's SampleSphericalMap.
    std::vector<float> Bake(const std::function<glm::vec3(glm::vec3)>& fn)
    {
        constexpr float    kPi = std::numbers::pi_v<float>;
        std::vector<float> out(static_cast<std::size_t>(kW) * kH * 4, 1.0f);
        for (int y = 0; y < kH; ++y)
            for (int x = 0; x < kW; ++x)
            {
                const float     phi = ((x + 0.5f) / kW - 0.5f) * 2.0f * kPi;
                const float     lat = ((y + 0.5f) / kH - 0.5f) * kPi;
                const glm::vec3 n(std::cos(lat) * std::cos(phi), std::sin(lat),
                                  std::cos(lat) * std::sin(phi));
                const glm::vec3 e = fn(n);
                const auto      i = (static_cast<std::size_t>(y) * kW + x) * 4;
                out[i]            = e.r;
                out[i + 1]        = e.g;
                out[i + 2]        = e.b;
            }
        return out;
    }
} // namespace

TEST(IrradianceSh, ConstantMapReconstructsConstant)
{
    const auto map =
        Bake([](glm::vec3) { return glm::vec3(2.0f, 1.0f, 0.5f); });
    const auto sh = fra::ProjectIrradianceSh(map, kW, kH);

    for (const auto& n :
         { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, -1),
           glm::normalize(glm::vec3(1, 2, -3)) })
    {
        const auto e = fra::EvaluateIrradianceSh(sh, n);
        EXPECT_NEAR(e.r, 2.0f, 0.02f);
        EXPECT_NEAR(e.g, 1.0f, 0.02f);
        EXPECT_NEAR(e.b, 0.5f, 0.02f);
    }
}

TEST(IrradianceSh, UpDownGradientKeepsOrientation)
{
    // Sky-like: bright for +Y normals, dark for -Y. A flipped or rotated
    // convention would invert this.
    const auto map = Bake([](glm::vec3 n) {
        const float s = 1.0f + 0.5f * n.y;
        return glm::vec3(s);
    });
    const auto sh  = fra::ProjectIrradianceSh(map, kW, kH);

    EXPECT_NEAR(fra::EvaluateIrradianceSh(sh, { 0, 1, 0 }).r, 1.5f, 0.03f);
    EXPECT_NEAR(fra::EvaluateIrradianceSh(sh, { 0, -1, 0 }).r, 0.5f, 0.03f);
    EXPECT_NEAR(fra::EvaluateIrradianceSh(sh, { 1, 0, 0 }).r, 1.0f, 0.03f);
}

TEST(IrradianceSh, HorizontalDirectionIsNotMirrored)
{
    const auto map = Bake([](glm::vec3 n) {
        const float s = 1.0f + 0.5f * n.x;
        return glm::vec3(s);
    });
    const auto sh  = fra::ProjectIrradianceSh(map, kW, kH);

    EXPECT_NEAR(fra::EvaluateIrradianceSh(sh, { 1, 0, 0 }).r, 1.5f, 0.03f);
    EXPECT_NEAR(fra::EvaluateIrradianceSh(sh, { -1, 0, 0 }).r, 0.5f, 0.03f);
}

TEST(IrradianceSh, ClampsNegativeLobesToZero)
{
    // A very sharp lobe makes the order-2 fit ring negative on the far side.
    const auto map = Bake([](glm::vec3 n) {
        const float d = std::max(glm::dot(n, glm::vec3(0, 1, 0)), 0.0f);
        return glm::vec3(std::pow(d, 64.0f) * 50.0f);
    });
    const auto sh  = fra::ProjectIrradianceSh(map, kW, kH);

    for (const auto& n : { glm::vec3(0, -1, 0), glm::vec3(1, -1, 0) })
    {
        const auto e = fra::EvaluateIrradianceSh(sh, glm::normalize(n));
        EXPECT_GE(e.r, 0.0f);
    }
}
