#pragma once

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Surface coverage mode for materials.
     *
     * Opaque/Mask stay in the deferred MDI stream. Blend is culled into the
     * separate WBOIT translucent MDI pass (order-independent).
     */
    enum class AlphaMode : std::uint32_t
    {
        Opaque = 0,
        Mask   = 1,
        Blend  = 2,
    };

    /**
     * @brief Parameters for creating or updating a material.
     *
     * Empty texture optionals use white/black fallbacks (current look).
     * Identity factors (defaults) leave sampled values unchanged.
     *
     * @param albedo           Albedo texture ID (optional)
     * @param normal           Normal map texture ID (optional)
     * @param roughness        Roughness map texture ID (optional)
     * @param emissive         Emissive texture ID (optional)
     * @param metalness        Metalness map texture ID (optional)
     * @param albedoFactor     Multiplies sampled albedo (default white);
     *                         .a multiplies albedo alpha for Mask/Blend
     * @param roughnessFactor  Multiplies sampled roughness (default 1)
     * @param metalnessFactor  Multiplies sampled metalness (default 1;
     *                         black metalness fallback → 0)
     * @param emissiveFactor   Multiplies sampled emissive (default white)
     * @param aoFactor         Constant AO written to G-buffer (default 1);
     *                         coated materials store coat roughness in
     *                         PBR.b instead
     * @param alphaCutoff      Mask discard threshold (albedo.a * factor.a);
     *                         ignored when alphaMode is not Mask
     * @param alphaMode        Opaque, Mask (cutout), or Blend (WBOIT)
     * @param clearcoat        Clearcoat weight 0–1 (deferred GGX layer)
     * @param clearcoatRoughness Clearcoat GGX roughness (glTF default ~0.03)
     */
    struct MaterialCreateInfo
    {
        std::optional<std::uint32_t> albedo;
        std::optional<std::uint32_t> normal;
        std::optional<std::uint32_t> roughness;
        std::optional<std::uint32_t> emissive;
        std::optional<std::uint32_t> metalness;

        glm::vec4 albedoFactor { 1.f, 1.f, 1.f, 1.f };
        float     roughnessFactor = 1.f;
        float     metalnessFactor = 1.f;
        glm::vec3 emissiveFactor { 1.f, 1.f, 1.f };
        float     aoFactor           = 1.f;
        float     alphaCutoff        = 0.f;
        AlphaMode alphaMode          = AlphaMode::Opaque;
        float     clearcoat          = 0.f;
        float     clearcoatRoughness = 0.03f;
    };

    /**
     * @brief CPU material record. GPU state lives in the bindless table.
     */
    struct Material
    {
        operator std::uint32_t() const { return id; }

        MaterialCreateInfo createInfo;
        std::uint32_t      id;
    };

} // namespace FREYA_NAMESPACE
