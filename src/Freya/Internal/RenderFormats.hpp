#pragma once

#include <vulkan/vulkan.hpp>

/**
 * Single source of truth for render-target formats shared between image
 * creation (`ImageBuilder::chooseFormat`) and render pass attachments.
 * An image and the attachment description of the pass that draws into it
 * must use the same format, so neither side may hard-code it.
 *
 * Depth is not listed: it is device dependent (`PhysicalDevice::
 * GetDepthFormat`).
 */
namespace FREYA_NAMESPACE::RenderFormats
{
    // ---- Deferred G-buffer (layout: Shaders/Include/gbuffer.inc) ----------

    /// Hardware sRGB for RGB; material ID in A is unaffected.
    inline constexpr vk::Format GBufferAlbedo = vk::Format::eR8G8B8A8Srgb;

    /// Octahedral normal (RG) + roughness|variant (B) + flags (A).
    inline constexpr vk::Format GBufferNormal =
        vk::Format::eA2B10G10R10UnormPack32;

    /// R metalness, G AO (or clearcoat|coat roughness nibbles).
    inline constexpr vk::Format GBufferPbr = vk::Format::eR8G8Unorm;

    /**
     * Emissive HDR, then lighting accumulation. 32 bpp: alpha is unused and
     * HDR range is kept (unsigned). Blend, sampling and blit-src are
     * mandatory; storage image and blit-dst are NOT guaranteed, so nothing
     * may write it that way.
     */
    inline constexpr vk::Format GBufferSceneColor =
        vk::Format::eB10G11R11UfloatPack32;

    /// UV-space motion vectors.
    inline constexpr vk::Format GBufferVelocity = vk::Format::eR16G16Sfloat;

    // ---- Post-lighting HDR -------------------------------------------------

    /// Scene with translucency, WBOIT accumulation (A = weight sum) and the
    /// billboard HDR target. Needs alpha, hence not the G-buffer format.
    inline constexpr vk::Format HdrScene = vk::Format::eR16G16B16A16Sfloat;

    /// WBOIT revealage.
    inline constexpr vk::Format OitReveal = vk::Format::eR8Unorm;
} // namespace FREYA_NAMESPACE::RenderFormats
