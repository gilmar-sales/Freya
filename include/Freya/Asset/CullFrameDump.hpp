#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/InstanceTransform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief One surviving draw after GPU cull compact.
     */
    struct CullSurvivor
    {
        std::uint32_t entityId = 0;
        std::uint32_t meshId   = 0;
        std::uint32_t slot     = 0;
    };

    /**
     * @brief Expected labels for FreyaGpuTests (curated after dump).
     */
    struct CullFrameExpected
    {
        std::vector<std::uint32_t> mustSurviveEntityIds;
        std::vector<std::uint32_t> mustDieEntityIds;
        /// If non-negative, assert exact drawCount.
        int drawCount = -1;
    };

    /**
     * @brief Hi-Z pyramid metadata + optional raw R32F mip stack.
     *
     * Binary sidecar (`hiz.r32f`) layout when written by the dump IO:
     *   u32 magic 'HIZ1', u32 width, u32 height, u32 mipCount,
     *   then contiguous mip0..mipN-1 floats (row-major, each mip
     *   extent = ceil(prev/2)).
     */
    struct CullHiZDump
    {
        bool          present  = false;
        bool          ready    = false;
        bool          enabled  = false;
        std::uint32_t width    = 0;
        std::uint32_t height   = 0;
        std::uint32_t mipCount = 0;
        std::string   file; ///< relative name, typically "hiz.r32f"
        /// Packed mip floats (host); empty when only metadata is known.
        std::vector<float> pixels;
    };

    /**
     * @brief Full cull-frame snapshot for dump / FreyaGpuTests fixtures.
     *
     * Mirrors CullFrustum.comp push constants + input SSBOs + observed
     * survivors. Serialization lives in Examples/Common (CullFrameDumpIo).
     */
    struct CullFrameSnapshot
    {
        std::uint32_t version = 1;
        std::string   example;
        std::string   label;
        std::string   notes;

        CullPushConstants              pushConstants {};
        std::vector<MeshInfo>          meshes;
        std::vector<MeshLodInfo>       lods;
        std::vector<SceneInstance>     instances;
        std::vector<InstanceTransform> sources;

        CullHiZDump hiz {};

        std::uint32_t             observedDrawCount = 0;
        std::vector<CullSurvivor> survivors;

        CullFrameExpected expected {};
    };

} // namespace FREYA_NAMESPACE
