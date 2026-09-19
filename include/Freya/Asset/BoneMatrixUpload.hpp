#pragma once

#include "Freya/Config.hpp"

#include <cstdint>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Host bone-palette upload header (mats via Upload span API).
     *
     * Prefer Renderer::BeginBoneMatrixUploads → UploadBoneMatrixUploads →
     * EndBoneMatrixUploads for parallel ECS packing. @c boneCount is the
     * matrix span length for that job.
     */
    struct BoneMatrixUpload
    {
        std::uint32_t boneOffset = 0;
        std::uint32_t boneCount  = 0;
    };

} // namespace FREYA_NAMESPACE
