#include "Freya/Asset/MeshLod.hpp"

#include <cmath>
#include <cstddef>

#include <meshoptimizer.h>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr std::size_t kAttrFloats = 5; // uv.xy + normal.xyz

        void packAttributes(std::span<const Vertex> vertices,
                            std::vector<float>&     outAttrs)
        {
            outAttrs.resize(vertices.size() * kAttrFloats);
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                const auto& v = vertices[i];
                float*      a = outAttrs.data() + i * kAttrFloats;
                a[0]          = v.texCoord.x;
                a[1]          = v.texCoord.y;
                a[2]          = v.normal.x;
                a[3]          = v.normal.y;
                a[4]          = v.normal.z;
            }
        }
    } // namespace

    std::vector<std::vector<std::uint32_t>> BuildMeshLodIndexSets(
        std::span<const Vertex> vertices,
        std::span<const std::uint32_t>
                                   indices,
        const MeshLodBuildOptions& options)
    {
        std::vector<std::vector<std::uint32_t>> lods;
        lods.reserve(kMaxLodsPerMesh);
        lods.emplace_back(indices.begin(), indices.end());

        if (!options.enabled || vertices.empty() || indices.size() < 3 ||
            indices.size() < options.minSourceIndices)
            return lods;

        std::vector<float> positions(vertices.size() * 3);
        for (std::size_t i = 0; i < vertices.size(); ++i)
        {
            positions[i * 3 + 0] = vertices[i].position.x;
            positions[i * 3 + 1] = vertices[i].position.y;
            positions[i * 3 + 2] = vertices[i].position.z;
        }

        std::vector<float> attrs;
        packAttributes(vertices, attrs);

        const float attrWeights[kAttrFloats] = {
            options.uvWeight,     options.uvWeight,     options.normalWeight,
            options.normalWeight, options.normalWeight,
        };

        const auto& lod0 = lods[0];

        for (std::size_t level = 0;
             level < options.ratios.size() && lods.size() < kMaxLodsPerMesh;
             ++level)
        {
            const float ratio = options.ratios[level];
            if (!(ratio > 0.0f && ratio < 1.0f))
                continue;

            auto targetCount = static_cast<std::size_t>(std::floor(
                static_cast<double>(lod0.size()) * static_cast<double>(ratio)));
            targetCount      = (targetCount / 3u) * 3u;
            if (targetCount < 3 || targetCount >= lod0.size())
                continue;
            if (!lods.empty() && targetCount >= lods.back().size())
                continue;

            std::vector<std::uint32_t> simplified(lod0.size());
            float                      resultError = 0.0f;
            const std::size_t          written = meshopt_simplifyWithAttributes(
                simplified.data(), lod0.data(), lod0.size(), positions.data(),
                vertices.size(), sizeof(float) * 3, attrs.data(),
                sizeof(float) * kAttrFloats, attrWeights, kAttrFloats,
                /*vertex_lock=*/nullptr, targetCount, options.targetError,
                /*options=*/0, &resultError);

            if (written < 3 || written >= lod0.size())
                break;
            if (!lods.empty() && written >= lods.back().size())
                break;

            simplified.resize(written);
            meshopt_optimizeVertexCache(simplified.data(), simplified.data(),
                                        simplified.size(), vertices.size());

            lods.push_back(std::move(simplified));
        }

        return lods;
    }

} // namespace FREYA_NAMESPACE
