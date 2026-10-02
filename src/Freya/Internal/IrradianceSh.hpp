#pragma once

#include "Freya/Config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Order-2 (9 coefficient) spherical-harmonic fit of the baked
     * irradiance map, evaluated in the lighting shader instead of sampling
     * the equirect map (no texture fetch, no atan/asin).
     *
     * Coefficients are stored directly as irradiance (already
     * cosine-convolved), so the shader just evaluates
     * `sum(c[i] * basis_i(n))`. The basis order matches
     * `EvalIrradianceSh` in Shaders/Include/pbr_ibl.inc.
     */
    struct IrradianceSh
    {
        std::array<glm::vec4, 9> c {}; ///< rgb per basis function, w unused
    };

    namespace IrradianceShDetail
    {
        inline std::array<float, 9> Basis(const glm::vec3& n)
        {
            return {
                0.282095f,
                0.488603f * n.y,
                0.488603f * n.z,
                0.488603f * n.x,
                1.092548f * n.x * n.y,
                1.092548f * n.y * n.z,
                0.315392f * (3.0f * n.z * n.z - 1.0f),
                1.092548f * n.x * n.z,
                0.546274f * (n.x * n.x - n.y * n.y),
            };
        }
    } // namespace IrradianceShDetail

    /**
     * @brief Projects an RGBA32F irradiance map onto SH9.
     *
     * Texel (u, v) is mapped to the normal that the lighting shader's
     * `SampleSphericalMap` would send to that texel (u = atan(z,x)/2pi + .5,
     * v = asin(y)/pi + .5), so the fit reproduces what the shader used to
     * read from the texture. Weights are the texel solid angles.
     */
    inline IrradianceSh ProjectIrradianceSh(const std::vector<float>& rgba,
                                            const int width, const int height)
    {
        constexpr float kPi = std::numbers::pi_v<float>;

        IrradianceSh sh {};
        for (int y = 0; y < height; ++y)
        {
            const float v   = (static_cast<float>(y) + 0.5f) / height;
            const float lat = (v - 0.5f) * kPi;
            const float dOmega =
                (2.0f * kPi / width) * (kPi / height) * std::cos(lat);

            for (int x = 0; x < width; ++x)
            {
                const float u   = (static_cast<float>(x) + 0.5f) / width;
                const float phi = (u - 0.5f) * 2.0f * kPi;
                const glm::vec3 n(std::cos(lat) * std::cos(phi),
                                  std::sin(lat),
                                  std::cos(lat) * std::sin(phi));

                const auto basis = IrradianceShDetail::Basis(n);

                const std::size_t i =
                    (static_cast<std::size_t>(y) * width + x) * 4;
                const glm::vec3 e(rgba[i], rgba[i + 1], rgba[i + 2]);
                for (std::size_t k = 0; k < sh.c.size(); ++k)
                    sh.c[k] += glm::vec4(e * (basis[k] * dOmega), 0.0f);
            }
        }
        return sh;
    }

    /// CPU mirror of the shader evaluation (used by tests).
    inline glm::vec3 EvaluateIrradianceSh(const IrradianceSh& sh,
                                          const glm::vec3&    n)
    {
        const auto basis = IrradianceShDetail::Basis(n);
        glm::vec3  e(0.0f);
        for (std::size_t k = 0; k < sh.c.size(); ++k)
            e += glm::vec3(sh.c[k]) * basis[k];
        return glm::max(e, glm::vec3(0.0f));
    }
} // namespace FREYA_NAMESPACE
