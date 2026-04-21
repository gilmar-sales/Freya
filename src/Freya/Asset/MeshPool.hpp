#pragma once

#include "Freya/Asset/Vertex.hpp"
#include "Freya/Containers/MeshSet.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Manages mesh buffers and GPU-side mesh storage.
     *
     * Handles vertex/index buffer allocation and management using a pool
     * approach. Creates buffers on demand, queries existing buffers for
     * available space, and uploads mesh data via staging buffers.
     * Uses Assimp for file loading (GLTF, OBJ, etc.).
     *
     * @param device        Device reference
     * @param physicalDevice Physical device for memory queries
     * @param commandPool   Command pool for transfer operations
     * @param logger        Logger reference
     */
    class MeshPool
    {
      public:
        MeshPool(const Ref<Device>&                device,
                 const Ref<PhysicalDevice>&        physicalDevice,
                 const Ref<CommandPool>&           commandPool,
                 const Ref<skr::Logger<MeshPool>>& logger);

        /**
         * @brief Draws a single mesh by ID.
         * @param meshId Mesh identifier returned from CreateMesh
         */
        void Draw(std::uint32_t meshId);

        /**
         * @brief Draws multiple instances of a mesh.
         * @param meshId       Mesh identifier
         * @param instanceCount Number of instances to draw
         * @param firstInstance First instance index (default 0)
         */
        void DrawInstanced(std::uint32_t meshId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0);

        /**
         * @brief Creates a mesh from CPU vertex/index data.
         * @param vertices Vector of Vertex structures
         * @param indices  Vector of 16-bit indices
         * @return Mesh ID for later drawing/binding
         */
        std::uint32_t CreateMesh(const std::vector<Vertex>&   vertices,
                                 const std::vector<uint16_t>& indices);

        /**
         * @brief Creates meshes from a file using Assimp.
         * @param path Path to mesh file (GLTF, OBJ, etc.)
         * @return Vector of mesh IDs created from the file
         */
        std::vector<std::uint32_t> CreateMeshFromFile(const std::string& path);

      protected:
        /**
         * @brief Creates a new vertex buffer of given size.
         * @param size Buffer size in bytes
         * @return Index of created vertex buffer
         */
        std::uint32_t createVertexBuffer(std::uint32_t size);

        /**
         * @brief Finds existing vertex buffer with sufficient space or creates
         * new.
         * @param size Required size in bytes
         * @return Index of found or created vertex buffer
         */
        std::uint32_t queryVertexBuffer(std::uint32_t size);

        /**
         * @brief Creates a new index buffer of given size.
         * @param size Buffer size in bytes
         * @return Index of created index buffer
         */
        std::uint32_t createIndexBuffer(std::uint32_t size);

        /**
         * @brief Finds existing index buffer with sufficient space or creates
         * new.
         * @param size Required size in bytes
         * @return Index of found or created index buffer
         */
        std::uint32_t queryIndexBuffer(std::uint32_t size);

        /**
         * @brief Converts Assimp mesh to internal mesh format.
         * @param mesh Assimp mesh pointer
         * @param scene Assimp scene pointer
         * @return Mesh ID of created mesh
         */
        std::uint32_t processMesh(const aiMesh* mesh, const aiScene* scene);

        /**
         * @brief Recursively processes Assimp node and its children.
         * @param meshes Vector to store created mesh IDs
         * @param node   Assimp node to process
         * @param scene  Assimp scene pointer
         */
        void processNode(std::vector<std::uint32_t>& meshes,
                         const aiNode*               node,
                         const aiScene*              scene);

        /**
         * @brief Finds existing staging buffer with sufficient space or creates
         * new.
         * @param size Required size in bytes
         * @return Staging buffer reference
         */
        Ref<Buffer> queryStagingBuffer(std::uint32_t size);

        /**
         * @brief Creates a new staging buffer.
         * @param size Buffer size in bytes
         * @return Staging buffer reference
         */
        Ref<Buffer> createStagingBuffer(std::uint32_t size);

      private:
        Ref<Device>                mDevice;
        Ref<PhysicalDevice>        mPhysicalDevice;
        Ref<CommandPool>           mCommandPool;
        Ref<skr::Logger<MeshPool>> mLogger;

        std::vector<Ref<Buffer>>   mStagingBuffers;
        std::vector<Ref<Buffer>>   mVertexBuffers;
        std::vector<std::uint32_t> mVertexBuffersOffsets;

        std::vector<Ref<Buffer>>   mIndexBuffers;
        std::vector<std::uint32_t> mIndexBuffersOffsets;
        MeshSet                    mMeshes;
    };

} // namespace FREYA_NAMESPACE