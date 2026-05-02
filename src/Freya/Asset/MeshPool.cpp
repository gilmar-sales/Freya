#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"

#include <glm/glm.hpp>
#include <unordered_map>

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes           = 1024 * 1024;
    constexpr auto MinVertexBufferSize = 10 * MegaBytes;
    constexpr auto MinIndexBufferSize  = 2 * MegaBytes;

    MeshPool::MeshPool(const Ref<Device>&                device,
                       const Ref<PhysicalDevice>&        physicalDevice,
                       const Ref<CommandPool>&           commandPool,
                       const Ref<skr::Logger<MeshPool>>& logger) :
        mDevice(device), mPhysicalDevice(physicalDevice),
        mCommandPool(commandPool), mLogger(logger), mMeshes(4096)
    {
        mVertexBuffers.reserve(1024);
        mIndexBuffers.reserve(1024);
        mStagingBuffers.reserve(1024);

        createVertexBuffer(MinVertexBufferSize);
    }

    void MeshPool::Draw(const std::uint32_t meshId)
    {
        if (mMeshes.contains(meshId))
        {
            const auto& mesh = mMeshes[meshId];

            const auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            const auto  vertexOffset =
                static_cast<vk::DeviceSize>(mesh.vertexBufferOffset);

            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &vertexOffset);

            const auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            const auto  indexOffset =
                static_cast<vk::DeviceSize>(mesh.indexBufferOffset);
            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                indexOffset,
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer()
                .drawIndexed(mesh.indexCount, 1, 0, 0, 0);
        }
    }

    void MeshPool::DrawInstanced(std::uint32_t meshId,
                                 size_t        instanceCount,
                                 size_t        firstInstance)
    {
        if (mMeshes.contains(meshId))
        {
            auto& mesh = mMeshes[meshId];

            const auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            const auto  vertexOffset =
                static_cast<vk::DeviceSize>(mesh.vertexBufferOffset);

            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &vertexOffset);

            const auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            const auto  indexOffset =
                static_cast<vk::DeviceSize>(mesh.indexBufferOffset);

            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                indexOffset,
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer().drawIndexed(
                mesh.indexCount,
                instanceCount,
                0,
                0,
                firstInstance);
        }
    }

    void MeshPool::DrawIndirect(std::uint32_t      meshId,
                                const Ref<Buffer>& indirectBuffer,
                                std::uint32_t      drawCount)
    {
        if (mMeshes.contains(meshId))
        {
            auto& mesh = mMeshes[meshId];

            const auto& vertexBuffer = mVertexBuffers[mesh.vertexBufferIndex];
            constexpr auto bindOffset = vk::DeviceSize(0);
            mCommandPool->GetCommandBuffer()
                .bindVertexBuffers(0, 1, &vertexBuffer->Get(), &bindOffset);

            const auto& indexBuffer = mIndexBuffers[mesh.indexBufferIndex];
            mCommandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(),
                vk::DeviceSize(0),
                vk::IndexType::eUint16);

            mCommandPool->GetCommandBuffer().drawIndexedIndirect(
                indirectBuffer->Get(),
                0,
                drawCount,
                sizeof(DrawIndexedIndirectCommand));
        }
    }

    std::uint32_t MeshPool::CreateMesh(const std::vector<Vertex>&   vertices,
                                       const std::vector<uint16_t>& indices)
    {
        mLogger->LogTrace("Creating mesh with {} vertices and {} indices.",
                          vertices.size(),
                          indices.size());

        constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        const auto commandBuffer = mCommandPool->CreateCommandBuffer();

        commandBuffer.begin(beginInfo);

        const auto vertexMemorySize = vertices.size() * sizeof(vertices[0]);
        const auto indexMemorySize  = indices.size() * sizeof(indices[0]);

        const auto stagingBuffer =
            queryStagingBuffer(vertexMemorySize + indexMemorySize);

        stagingBuffer->Copy(vertices.data(), vertexMemorySize);

        stagingBuffer->Copy(indices.data(), indexMemorySize, vertexMemorySize);

        // copy vertices
        const auto vertexBufferIndex = queryVertexBuffer(vertexMemorySize);
        auto& vertexBufferOffset     = mVertexBuffersOffsets[vertexBufferIndex];

        const auto vertexCopyRegion =
            vk::BufferCopy()
                .setSrcOffset(0)
                .setSize(vertexMemorySize)
                .setDstOffset(vertexBufferOffset);

        commandBuffer.copyBuffer(stagingBuffer->Get(),
                                 mVertexBuffers[vertexBufferIndex]->Get(),
                                 1,
                                 &vertexCopyRegion);

        // copy indices
        const auto indexBufferIndex  = queryIndexBuffer(indexMemorySize);
        auto&      indexBufferOffset = mIndexBuffersOffsets[indexBufferIndex];

        const auto indexCopyRegion =
            vk::BufferCopy()
                .setSrcOffset(vertexMemorySize)
                .setSize(indexMemorySize)
                .setDstOffset(indexBufferOffset);

        commandBuffer.copyBuffer(stagingBuffer->Get(),
                                 mIndexBuffers[indexBufferIndex]->Get(),
                                 1,
                                 &indexCopyRegion);

        commandBuffer.end();

        const auto submitInfo =
            vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                &commandBuffer);

        mDevice->GetTransferQueue().submit(submitInfo);
        mDevice->GetTransferQueue().waitIdle();

        mCommandPool->FreeCommandBuffer(commandBuffer);

        const auto mesh =
            Mesh { .vertexBufferIndex  = vertexBufferIndex,
                   .vertexBufferOffset = vertexBufferOffset,
                   .indexBufferIndex   = indexBufferIndex,
                   .indexBufferOffset  = indexBufferOffset,
                   .indexCount = static_cast<std::uint32_t>(indices.size()),
                   .id         = static_cast<std::uint32_t>(mMeshes.size()) };

        mMeshes.insert(mesh);

        // Keep a CPU-side copy so CreateSimplifiedMesh can decimate it
        mMeshSourceData[mesh.id] = { vertices, indices };

        vertexBufferOffset += vertexMemorySize;
        indexBufferOffset += indexMemorySize;

        return mesh.id;
    }

    std::vector<std::uint32_t> MeshPool::CreateMeshFromFile(
        const std::string& path)
    {
        mLogger->LogTrace("Creating mesh from file: {}", path.data());

        auto meshes = std::vector<std::uint32_t>();

        Assimp::Importer import;
        const aiScene*   scene = import.ReadFile(
            path,
            aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                aiProcess_SortByPType | aiProcess_GenNormals |
                aiProcess_GenUVCoords | aiProcess_OptimizeMeshes |
                aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale |
                aiProcess_ValidateDataStructure);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
            !scene->mRootNode)
        {
            mLogger->LogError("Failed loading mesh: {}",
                              import.GetErrorString());

            return meshes;
        }

        processNode(meshes, scene->mRootNode, scene);

        return meshes;
    }

    std::uint32_t MeshPool::CreateSimplifiedMesh(
        const std::uint32_t sourceMeshId, const float reduction)
    {
        // Clamp and validate
        const float r = std::clamp(reduction, 0.0f, 0.99f);
        if (r < 0.001f)
        {
            // No reduction requested — return the original mesh
            return sourceMeshId;
        }

        const auto it = mMeshSourceData.find(sourceMeshId);
        if (it == mMeshSourceData.end())
        {
            mLogger->LogError(
                "CreateSimplifiedMesh: source mesh {} not found",
                sourceMeshId);
            return std::numeric_limits<std::uint32_t>::max();
        }

        const auto& src     = it->second;
        const auto& srcVerts = src.vertices;
        const auto& srcIdx   = src.indices;

        if (srcVerts.empty() || srcIdx.empty())
        {
            mLogger->LogError(
                "CreateSimplifiedMesh: source mesh {} has no data",
                sourceMeshId);
            return std::numeric_limits<std::uint32_t>::max();
        }

        // ---------------------------------------------------------------
        //  Vertex clustering
        // ---------------------------------------------------------------
        // 1. Compute bounding box
        glm::vec3 bmin = srcVerts[0].position;
        glm::vec3 bmax = srcVerts[0].position;
        for (const auto& v : srcVerts)
        {
            bmin = glm::min(bmin, v.position);
            bmax = glm::max(bmax, v.position);
        }

        const glm::vec3 extent   = bmax - bmin;
        const float maxExtent =
            glm::max(extent.x, glm::max(extent.y, extent.z));
        if (maxExtent < 0.001f)
        {
            return sourceMeshId;
        }

        // Grid resolution: more reduction → coarser grid
        // At r=0.5 → 16 divisions, at r=0.9 → 3 divisions
        const int gridDivs =
            std::max(1, static_cast<int>(32.0f * (1.0f - r)));

        const glm::vec3 cellSize = extent / static_cast<float>(gridDivs);

        // 2. Assign each vertex to a cluster
        struct Cluster {
            glm::vec3 posSum  = glm::vec3(0.0f);
            glm::vec3 colSum  = glm::vec3(0.0f);
            glm::vec3 nrmSum  = glm::vec3(0.0f);
            glm::vec3 tanSum  = glm::vec3(0.0f);
            glm::vec2 uvSum   = glm::vec2(0.0f);
            std::uint32_t cnt = 0;
        };

        // UV quantisation resolution (how many cells across the [0,1] UV
        // domain).  Using the same resolution as the position grid ensures
        // vertices separated by a sharp UV seam land in distinct clusters
        // even when their positions are nearly identical.
        const int uvGridDivs = std::max(1, gridDivs * 2);

        // Cluster key: {gx, gy, gz, gu, gv}
        struct ClusterKey {
            std::int32_t gx, gy, gz, gu, gv;

            bool operator==(const ClusterKey& o) const
            {
                return gx == o.gx && gy == o.gy && gz == o.gz &&
                       gu == o.gu && gv == o.gv;
            }
        };

        struct ClusterKeyHash {
            std::size_t operator()(const ClusterKey& k) const
            {
                // Pack into 64-bit:
                //   gx:14, gy:14, gz:14, gu:11, gv:11 = 64
                return static_cast<std::size_t>(
                    (static_cast<std::uint64_t>(
                         static_cast<std::uint32_t>(k.gx) & 0x3FFFu)
                     << 50) |
                    (static_cast<std::uint64_t>(
                         static_cast<std::uint32_t>(k.gy) & 0x3FFFu)
                     << 36) |
                    (static_cast<std::uint64_t>(
                         static_cast<std::uint32_t>(k.gz) & 0x3FFFu)
                     << 22) |
                    (static_cast<std::uint64_t>(
                         static_cast<std::uint32_t>(k.gu) & 0x7FFu)
                     << 11) |
                    static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(k.gv) & 0x7FFu));
            }
        };

        std::unordered_map<ClusterKey, std::uint32_t, ClusterKeyHash>
            clusterMap;
        std::vector<Cluster> clusters;
        clusters.reserve(srcVerts.size());

        // Per-vertex cluster index
        std::vector<std::uint32_t> vertCluster(srcVerts.size(), 0xFFFFFFFFu);

        for (std::size_t i = 0; i < srcVerts.size(); ++i)
        {
            const auto& pos = srcVerts[i].position;

            // Compute grid cell coordinate (position)
            const int gx = static_cast<int>(
                (pos.x - bmin.x) / cellSize.x);
            const int gy = static_cast<int>(
                (pos.y - bmin.y) / cellSize.y);
            const int gz = static_cast<int>(
                (pos.z - bmin.z) / cellSize.z);

            // Compute grid cell coordinate (UV)
            const int gu = static_cast<int>(
                srcVerts[i].texCoord.x * uvGridDivs);
            const int gv = static_cast<int>(
                srcVerts[i].texCoord.y * uvGridDivs);

            const ClusterKey key { gx, gy, gz, gu, gv };

            auto [itCluster, inserted] =
                clusterMap.try_emplace(key, clusters.size());
            if (inserted)
            {
                clusters.emplace_back();
            }

            const auto clusterIdx = itCluster->second;
            vertCluster[i]        = clusterIdx;

            auto& cl = clusters[clusterIdx];
            cl.posSum += pos;
            cl.colSum += srcVerts[i].color;
            cl.nrmSum += srcVerts[i].normal;
            cl.tanSum += srcVerts[i].tangent;
            cl.uvSum  += srcVerts[i].texCoord;
            cl.cnt++;
        }

        mLogger->LogTrace(
            "Vertex clustering: {} verts → {} clusters (grid {}³)",
            srcVerts.size(), clusters.size(), gridDivs);

        // 3. Build representative vertices (one per cluster)
        std::vector<Vertex> newVerts;
        newVerts.reserve(clusters.size());
        for (const auto& cl : clusters)
        {
            const float inv = 1.0f / static_cast<float>(cl.cnt);
            Vertex v;
            v.position = cl.posSum * inv;
            v.color    = cl.colSum * inv;
            v.normal   = glm::normalize(cl.nrmSum * inv);
            v.tangent  = glm::normalize(cl.tanSum * inv);
            v.texCoord = cl.uvSum * inv;
            newVerts.push_back(v);
        }

        // 4. Rebuild faces, skipping degenerates
        std::vector<std::uint16_t> newIdx;
        newIdx.reserve(srcIdx.size()); // pessimistic

        const std::uint32_t triCount =
            static_cast<std::uint32_t>(srcIdx.size() / 3);
        for (std::uint32_t t = 0; t < triCount; ++t)
        {
            const auto i0 = vertCluster[srcIdx[t * 3 + 0]];
            const auto i1 = vertCluster[srcIdx[t * 3 + 1]];
            const auto i2 = vertCluster[srcIdx[t * 3 + 2]];

            // Skip degenerate triangles (two or more verts in same cluster)
            if (i0 == i1 || i1 == i2 || i0 == i2)
                continue;

            newIdx.push_back(static_cast<std::uint16_t>(i0));
            newIdx.push_back(static_cast<std::uint16_t>(i1));
            newIdx.push_back(static_cast<std::uint16_t>(i2));
        }

        mLogger->LogInformation(
            "Simplified mesh {}: {} verts, {} tris → {} verts, {} tris "
            "(reduction {:.0f}%)",
            sourceMeshId,
            srcVerts.size(), srcIdx.size() / 3,
            newVerts.size(), newIdx.size() / 3,
            r * 100.0f);

        if (newIdx.empty())
        {
            mLogger->LogError(
                "CreateSimplifiedMesh: simplification produced no triangles "
                "for mesh {}",
                sourceMeshId);
            return std::numeric_limits<std::uint32_t>::max();
        }

        // 5. Upload as a new mesh
        return CreateMesh(newVerts, newIdx);
    }

    std::uint32_t MeshPool::createVertexBuffer(const std::uint32_t size)
    {
        const auto vertexBuffer =
            BufferBuilder(mDevice)
                .SetSize(size)
                .SetUsage(BufferUsage::Vertex)
                .Build();

        mVertexBuffers.push_back(vertexBuffer);
        mVertexBuffersOffsets.push_back(0);

        return mVertexBuffers.size() - 1;
    }

    std::uint32_t MeshPool::queryVertexBuffer(const std::uint32_t size)
    {
        for (auto index = 0; index < mVertexBuffers.size(); index++)
        {
            if (mVertexBuffers[index]->GetSize() -
                    mVertexBuffersOffsets[index] >=
                size)
            {
                return index;
            }
        }

        return createVertexBuffer(
            size > MinVertexBufferSize ? size : MinVertexBufferSize);
    }

    std::uint32_t MeshPool::createIndexBuffer(const std::uint32_t size)
    {
        const auto indexBuffer =
            BufferBuilder(mDevice)
                .SetSize(size)
                .SetUsage(BufferUsage::Index)
                .Build();

        mIndexBuffers.push_back(indexBuffer);
        mIndexBuffersOffsets.push_back(0);

        return mIndexBuffers.size() - 1;
    }

    std::uint32_t MeshPool::queryIndexBuffer(const std::uint32_t size)
    {
        for (auto index = 0; index < mIndexBuffers.size(); index++)
        {
            if (mIndexBuffers[index]->GetSize() - mIndexBuffersOffsets[index] >=
                size)
            {
                return index;
            }
        }

        return createIndexBuffer(
            size > MinIndexBufferSize ? size : MinIndexBufferSize);
    }

    std::uint32_t MeshPool::processMesh(const aiMesh*  mesh,
                                        const aiScene* scene)
    {
        std::vector<Vertex>        vertices;
        std::vector<std::uint16_t> indices;

        for (auto i = 0; i < mesh->mNumVertices; i++)
        {
            const auto& aVertex    = mesh->mVertices[i];
            const auto& aNormal    = mesh->mNormals[i];
            const auto& aTangent   = mesh->mTangents[i];
            const auto& aTextCoord = mesh->mTextureCoords[0][i];

            const auto material = scene->mMaterials[mesh->mMaterialIndex];

            aiColor3D aColor(1.0, 1.0, 1.0);
            material->Get(AI_MATKEY_COLOR_DIFFUSE, aColor);

            auto vertex = Vertex {
                .position = glm::vec3(aVertex.x, aVertex.y, aVertex.z),
                .color    = glm::vec3(aColor.r, aColor.g, aColor.b),
                .normal   = glm::vec3(aNormal.x, aNormal.y, aNormal.z),
                .tangent  = glm::vec3(aTangent.x, aTangent.y, aTangent.z),
                .texCoord = glm::vec2(aTextCoord.x, aTextCoord.y)
            };

            // process vertex positions, normals and texture coordinates
            vertices.push_back(vertex);
        }

        for (auto i = 0; i < mesh->mNumFaces; i++)
        {
            const auto& face = mesh->mFaces[i];

            for (auto j = 0; j < face.mNumIndices; j++)
            {
                indices.push_back(face.mIndices[j]);
            }
        }

        return CreateMesh(vertices, indices);
    }

    void MeshPool::processNode(std::vector<std::uint32_t>& meshes,
                               const aiNode*               node,
                               const aiScene*              scene)
    {
        // process all the node's meshes (if any)
        if (node->mNumMeshes > 0)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshes.push_back(processMesh(mesh, scene));
            }
        }

        // then do the same for each of its children
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(meshes, node->mChildren[i], scene);
        }
    }

    Ref<Buffer> MeshPool::queryStagingBuffer(const std::uint32_t size)
    {
        for (auto stagingBuffer : mStagingBuffers)
        {
            if (stagingBuffer->GetSize() >= size)
            {
                return stagingBuffer;
            }
        }

        return createStagingBuffer(size);
    }

    Ref<Buffer> MeshPool::createStagingBuffer(const std::uint32_t size)
    {
        const auto bufferSize = (size / MegaBytes + 4) * MegaBytes;

        auto stagingBuffer =
            BufferBuilder(mDevice)
                .SetSize(bufferSize)
                .SetUsage(BufferUsage::Staging)
                .Build();

        mStagingBuffers.push_back(stagingBuffer);

        return stagingBuffer;
    }
} // namespace FREYA_NAMESPACE
