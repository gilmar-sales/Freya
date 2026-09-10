#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/Mesh.hpp"
#include "Freya/Asset/MeshLod.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/Vertex.hpp"
#include "Freya/Scene/AssetHandle.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Buffer;
    class CommandPool;
    class IndirectDrawSystem;
    class MeshPoolGpuAccess;

    class MeshPool
    {
      public:
        MeshPool(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~MeshPool();

        MeshPool(const MeshPool&)            = delete;
        MeshPool& operator=(const MeshPool&) = delete;
        MeshPool(MeshPool&&) noexcept;
        MeshPool& operator=(MeshPool&&) noexcept;

        /// Upload geometry; auto-builds mesh LODs unless `lodOptions.enabled`
        /// is false. Material / UVs are unchanged — only index ranges differ.
        MeshHandle CreateMesh(const std::vector<Vertex>&        vertices,
                              const std::vector<std::uint32_t>& indices,
                              const MeshLodBuildOptions& lodOptions = {});

        /// Explicit LOD index sets (LOD0 first). Empty sets are skipped.
        /// Clamped to `kMaxLodsPerMesh`. Shares one vertex buffer.
        MeshHandle CreateMesh(const std::vector<Vertex>& vertices,
                              std::span<const std::vector<std::uint32_t>>
                                  lodIndexSets);

        /// Static model import; builds GPU mesh LODs via meshoptimizer.
        std::vector<ModelSubmesh> CreateModelFromFile(
            const std::string&         path,
            const MeshLodBuildOptions& lodOptions = {});

        /// Skinned import. Mesh LODs are off by default (opt in via
        /// `lodOptions`); same shared VB / materials as static LODs.
        SkinnedModel CreateSkinnedModelFromFile(
            const std::string&         path,
            const MeshLodBuildOptions& lodOptions = {
                .enabled = false,
            });

        [[nodiscard]] bool Contains(MeshHandle mesh) const;

        [[nodiscard]] const Mesh& GetMesh(MeshHandle mesh) const;

        [[nodiscard]] std::uint32_t GetMeshCount() const;

        void Destroy(MeshHandle mesh);

      private:
        friend class IndirectDrawSystem;
        friend class MeshPoolGpuAccess;

        void BindGeometry(const skr::Arc<CommandPool>& commandPool) const;

        [[nodiscard]] const skr::Arc<Buffer>& GetVertexBuffer() const;
        [[nodiscard]] const skr::Arc<Buffer>& GetIndexBuffer() const;

        void Draw(const skr::Arc<CommandPool>& commandPool, MeshHandle mesh);

        void DrawInstanced(const skr::Arc<CommandPool>& commandPool,
                           MeshHandle                   mesh,
                           size_t                       instanceCount,
                           size_t                       firstInstance = 0);

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
