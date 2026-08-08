#include "Freya/Asset/MeshPool.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Containers/MeshSet.hpp"
#include "Freya/Core/Buffer.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <limits>

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes           = 1024 * 1024;
    constexpr auto MinVertexBufferSize = 1 * MegaBytes;
    constexpr auto MinIndexBufferSize  = 2 * MegaBytes;

    struct MeshPool::Impl
    {
        skr::Arc<Device>                device;
        skr::Arc<PhysicalDevice>        physicalDevice;
        skr::Arc<CommandPool>           commandPool;
        skr::Arc<skr::Logger<MeshPool>> logger;

        std::vector<skr::Arc<Buffer>> stagingBuffers;
        std::vector<skr::Arc<Buffer>> vertexBuffers;
        std::vector<std::uint32_t>    vertexBuffersOffsets;

        std::vector<skr::Arc<Buffer>> indexBuffers;
        std::vector<std::uint32_t>    indexBuffersOffsets;
        MeshSet                       meshes;

        Impl(skr::Arc<Device> inDevice,
             skr::Arc<PhysicalDevice>
                 inPhysicalDevice,
             skr::Arc<CommandPool>
                 inCommandPool,
             skr::Arc<skr::Logger<MeshPool>>
                 inLogger) :
            device(std::move(inDevice)),
            physicalDevice(std::move(inPhysicalDevice)),
            commandPool(std::move(inCommandPool)), logger(std::move(inLogger)),
            meshes(4096)
        {
            vertexBuffers.reserve(1024);
            indexBuffers.reserve(1024);
            stagingBuffers.reserve(1024);
            createVertexBuffer(MinVertexBufferSize);
        }

        std::uint32_t createVertexBuffer(std::uint32_t size)
        {
            const auto vertexBuffer =
                BufferBuilder(device)
                    .SetSize(size)
                    .SetUsage(BufferUsage::Vertex)
                    .Build();

            vertexBuffers.push_back(vertexBuffer);
            vertexBuffersOffsets.push_back(0);

            return static_cast<std::uint32_t>(vertexBuffers.size() - 1);
        }

        std::uint32_t queryVertexBuffer(std::uint32_t size)
        {
            for (auto index = 0u; index < vertexBuffers.size(); ++index)
            {
                if (vertexBuffers[index]->GetSize() -
                        vertexBuffersOffsets[index] >=
                    size)
                {
                    return index;
                }
            }

            return createVertexBuffer(
                size > MinVertexBufferSize ? size : MinVertexBufferSize);
        }

        std::uint32_t createIndexBuffer(std::uint32_t size)
        {
            const auto indexBuffer =
                BufferBuilder(device)
                    .SetSize(size)
                    .SetUsage(BufferUsage::Index)
                    .Build();

            indexBuffers.push_back(indexBuffer);
            indexBuffersOffsets.push_back(0);

            return static_cast<std::uint32_t>(indexBuffers.size() - 1);
        }

        std::uint32_t queryIndexBuffer(std::uint32_t size)
        {
            for (auto index = 0u; index < indexBuffers.size(); ++index)
            {
                if (indexBuffers[index]->GetSize() -
                        indexBuffersOffsets[index] >=
                    size)
                {
                    return index;
                }
            }

            return createIndexBuffer(
                size > MinIndexBufferSize ? size : MinIndexBufferSize);
        }

        skr::Arc<Buffer> createStagingBuffer(std::uint32_t size)
        {
            const auto bufferSize = (size / MegaBytes + 4) * MegaBytes;

            auto stagingBuffer =
                BufferBuilder(device)
                    .SetSize(bufferSize)
                    .SetUsage(BufferUsage::Staging)
                    .Build();

            stagingBuffers.push_back(stagingBuffer);

            return stagingBuffer;
        }

        skr::Arc<Buffer> queryStagingBuffer(std::uint32_t size)
        {
            for (auto stagingBuffer : stagingBuffers)
            {
                if (stagingBuffer->GetSize() >= size)
                {
                    return stagingBuffer;
                }
            }

            return createStagingBuffer(size);
        }

        std::uint32_t createMesh(const std::vector<Vertex>&   vertices,
                                 const std::vector<uint16_t>& indices)
        {
            logger->LogTrace("Creating mesh with {} vertices and {} indices.",
                             vertices.size(),
                             indices.size());

            constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

            const auto commandBuffer = commandPool->CreateCommandBuffer();

            commandBuffer.begin(beginInfo);

            const auto vertexMemorySize = vertices.size() * sizeof(vertices[0]);
            const auto indexMemorySize  = indices.size() * sizeof(indices[0]);

            const auto stagingBuffer =
                queryStagingBuffer(vertexMemorySize + indexMemorySize);

            stagingBuffer->Copy(vertices.data(), vertexMemorySize);
            stagingBuffer->Copy(indices.data(), indexMemorySize,
                                vertexMemorySize);

            const auto vertexBufferIndex = queryVertexBuffer(vertexMemorySize);
            auto& vertexBufferOffset = vertexBuffersOffsets[vertexBufferIndex];

            const auto vertexCopyRegion =
                vk::BufferCopy()
                    .setSrcOffset(0)
                    .setSize(vertexMemorySize)
                    .setDstOffset(vertexBufferOffset);

            commandBuffer.copyBuffer(stagingBuffer->Get(),
                                     vertexBuffers[vertexBufferIndex]->Get(),
                                     1,
                                     &vertexCopyRegion);

            const auto indexBufferIndex = queryIndexBuffer(indexMemorySize);
            auto& indexBufferOffset     = indexBuffersOffsets[indexBufferIndex];

            const auto indexCopyRegion =
                vk::BufferCopy()
                    .setSrcOffset(vertexMemorySize)
                    .setSize(indexMemorySize)
                    .setDstOffset(indexBufferOffset);

            commandBuffer.copyBuffer(stagingBuffer->Get(),
                                     indexBuffers[indexBufferIndex]->Get(),
                                     1,
                                     &indexCopyRegion);

            commandBuffer.end();

            const auto submitInfo =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &commandBuffer);

            device->GetTransferQueue().submit(submitInfo);
            device->GetTransferQueue().waitIdle();

            commandPool->FreeCommandBuffer(commandBuffer);

            const auto indexCount = static_cast<std::uint32_t>(indices.size());

            glm::vec3 aabbMin(std::numeric_limits<float>::max());
            glm::vec3 aabbMax(std::numeric_limits<float>::lowest());
            for (const auto& v : vertices)
            {
                aabbMin = glm::min(aabbMin, v.position);
                aabbMax = glm::max(aabbMax, v.position);
            }
            if (vertices.empty())
            {
                aabbMin = glm::vec3(0.0f);
                aabbMax = glm::vec3(0.0f);
            }

            const auto mesh = Mesh {
                .vertexBufferIndex  = vertexBufferIndex,
                .vertexBufferOffset = vertexBufferOffset,
                .indexBufferIndex   = indexBufferIndex,
                .indexBufferOffset  = indexBufferOffset,
                .firstIndex = indexBufferOffset /
                              static_cast<std::uint32_t>(sizeof(std::uint16_t)),
                .vertexOffset = static_cast<std::int32_t>(
                    vertexBufferOffset /
                    static_cast<std::uint32_t>(sizeof(Vertex))),
                .indexCount = indexCount,
                .aabbMin    = aabbMin,
                .aabbMax    = aabbMax,
                .id         = static_cast<std::uint32_t>(meshes.size())
            };

            meshes.insert(mesh);

            vertexBufferOffset += static_cast<std::uint32_t>(vertexMemorySize);
            indexBufferOffset += static_cast<std::uint32_t>(indexMemorySize);

            return mesh.id;
        }

        std::uint32_t processMesh(const aiMesh* mesh, const aiScene* scene)
        {
            std::vector<Vertex>        vertices;
            std::vector<std::uint16_t> indices;

            for (auto i = 0u; i < mesh->mNumVertices; ++i)
            {
                const auto& aVertex    = mesh->mVertices[i];
                const auto& aNormal    = mesh->mNormals[i];
                const auto& aTangent   = mesh->mTangents[i];
                const auto& aTextCoord = mesh->mTextureCoords[0][i];

                const auto material = scene->mMaterials[mesh->mMaterialIndex];

                aiColor3D aColor(1.0, 1.0, 1.0);
                material->Get(AI_MATKEY_COLOR_DIFFUSE, aColor);

                vertices.push_back(Vertex {
                    .position = glm::vec3(aVertex.x, aVertex.y, aVertex.z),
                    .color    = glm::vec3(aColor.r, aColor.g, aColor.b),
                    .normal   = glm::vec3(aNormal.x, aNormal.y, aNormal.z),
                    .tangent  = glm::vec3(aTangent.x, aTangent.y, aTangent.z),
                    .texCoord = glm::vec2(aTextCoord.x, aTextCoord.y),
                });
            }

            for (auto i = 0u; i < mesh->mNumFaces; ++i)
            {
                const auto& face = mesh->mFaces[i];
                for (auto j = 0u; j < face.mNumIndices; ++j)
                {
                    indices.push_back(
                        static_cast<std::uint16_t>(face.mIndices[j]));
                }
            }

            return createMesh(vertices, indices);
        }

        void processNode(std::vector<std::uint32_t>& meshIds,
                         const aiNode*               node,
                         const aiScene*              scene)
        {
            if (node->mNumMeshes > 0)
            {
                for (unsigned int i = 0; i < node->mNumMeshes; ++i)
                {
                    const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                    meshIds.push_back(processMesh(mesh, scene));
                }
            }

            for (unsigned int i = 0; i < node->mNumChildren; ++i)
            {
                processNode(meshIds, node->mChildren[i], scene);
            }
        }

        std::vector<std::uint32_t> createMeshFromFile(const std::string& path)
        {
            logger->LogTrace("Creating mesh from file: {}", path);

            auto meshIds = std::vector<std::uint32_t>();

            Assimp::Importer importer;
            const aiScene*   scene = importer.ReadFile(
                path,
                aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                    aiProcess_SortByPType | aiProcess_GenNormals |
                    aiProcess_GenUVCoords | aiProcess_OptimizeMeshes |
                    aiProcess_JoinIdenticalVertices | aiProcess_GlobalScale |
                    aiProcess_ValidateDataStructure);

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
                !scene->mRootNode)
            {
                logger->LogError("Failed loading mesh: {}",
                                 importer.GetErrorString());
                return meshIds;
            }

            processNode(meshIds, scene->mRootNode, scene);
            return meshIds;
        }

        void draw(std::uint32_t meshId)
        {
            if (!meshes.contains(meshId))
                return;

            const auto& mesh = meshes[meshId];

            const auto& vertexBuffer = vertexBuffers[mesh.vertexBufferIndex];
            constexpr vk::DeviceSize zeroOffset = 0;

            commandPool->GetCommandBuffer().bindVertexBuffers(
                0, 1, &vertexBuffer->Get(), &zeroOffset);

            const auto& indexBuffer = indexBuffers[mesh.indexBufferIndex];
            commandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(), 0, vk::IndexType::eUint16);

            commandPool->GetCommandBuffer().drawIndexed(
                mesh.indexCount, 1, mesh.firstIndex, mesh.vertexOffset, 0);
        }

        void drawInstanced(std::uint32_t meshId,
                           size_t        instanceCount,
                           size_t        firstInstance)
        {
            if (!meshes.contains(meshId))
                return;

            const auto& mesh = meshes[meshId];

            const auto& vertexBuffer = vertexBuffers[mesh.vertexBufferIndex];
            constexpr vk::DeviceSize zeroOffset = 0;

            commandPool->GetCommandBuffer().bindVertexBuffers(
                0, 1, &vertexBuffer->Get(), &zeroOffset);

            const auto& indexBuffer = indexBuffers[mesh.indexBufferIndex];

            commandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffer->Get(), 0, vk::IndexType::eUint16);

            commandPool->GetCommandBuffer().drawIndexed(
                mesh.indexCount, instanceCount, mesh.firstIndex,
                mesh.vertexOffset, firstInstance);
        }

        void bindChunk(std::uint32_t vertexBufferIndex,
                       std::uint32_t indexBufferIndex) const
        {
            constexpr vk::DeviceSize zeroOffset = 0;
            commandPool->GetCommandBuffer().bindVertexBuffers(
                0, 1, &vertexBuffers[vertexBufferIndex]->Get(), &zeroOffset);
            commandPool->GetCommandBuffer().bindIndexBuffer(
                indexBuffers[indexBufferIndex]->Get(), 0,
                vk::IndexType::eUint16);
        }
    };

    MeshPool::MeshPool(const skr::Arc<Device>&                device,
                       const skr::Arc<PhysicalDevice>&        physicalDevice,
                       const skr::Arc<CommandPool>&           commandPool,
                       const skr::Arc<skr::Logger<MeshPool>>& logger) :
        mImpl(
            std::make_unique<Impl>(device, physicalDevice, commandPool, logger))
    {
    }

    MeshPool::~MeshPool() = default;

    MeshPool::MeshPool(MeshPool&&) noexcept            = default;
    MeshPool& MeshPool::operator=(MeshPool&&) noexcept = default;

    std::uint32_t MeshPool::CreateMesh(const std::vector<Vertex>&   vertices,
                                       const std::vector<uint16_t>& indices)
    {
        return mImpl->createMesh(vertices, indices);
    }

    std::vector<std::uint32_t> MeshPool::CreateMeshFromFile(
        const std::string& path)
    {
        return mImpl->createMeshFromFile(path);
    }

    bool MeshPool::Contains(const std::uint32_t meshId) const
    {
        return mImpl->meshes.contains(meshId);
    }

    const Mesh& MeshPool::GetMesh(const std::uint32_t meshId) const
    {
        return mImpl->meshes[meshId];
    }

    std::uint32_t MeshPool::GetMeshCount() const
    {
        return static_cast<std::uint32_t>(mImpl->meshes.size());
    }

    void MeshPool::FillMeshInfos(std::vector<MeshInfo>& out) const
    {
        const auto count = GetMeshCount();
        out.assign(count, MeshInfo {});
        for (std::uint32_t id = 0; id < count; ++id)
        {
            if (!mImpl->meshes.contains(id))
                continue;
            const auto& mesh          = mImpl->meshes[id];
            out[id]                   = MeshInfo {};
            out[id].indexCount        = mesh.indexCount;
            out[id].firstIndex        = mesh.firstIndex;
            out[id].vertexOffset      = mesh.vertexOffset;
            out[id].vertexBufferIndex = mesh.vertexBufferIndex;
            out[id].indexBufferIndex  = mesh.indexBufferIndex;
            out[id].aabbMin           = glm::vec4(mesh.aabbMin, 0.0f);
            out[id].aabbMax           = glm::vec4(mesh.aabbMax, 0.0f);
        }
    }

    void MeshPool::BindChunk(const std::uint32_t vertexBufferIndex,
                             const std::uint32_t indexBufferIndex) const
    {
        mImpl->bindChunk(vertexBufferIndex, indexBufferIndex);
    }

    const skr::Arc<Buffer>& MeshPool::GetVertexBuffer(
        const std::uint32_t index) const
    {
        return mImpl->vertexBuffers[index];
    }

    const skr::Arc<Buffer>& MeshPool::GetIndexBuffer(
        const std::uint32_t index) const
    {
        return mImpl->indexBuffers[index];
    }

    void MeshPool::Draw(const std::uint32_t meshId)
    {
        mImpl->draw(meshId);
    }

    void MeshPool::DrawInstanced(std::uint32_t meshId,
                                 size_t        instanceCount,
                                 size_t        firstInstance)
    {
        mImpl->drawInstanced(meshId, instanceCount, firstInstance);
    }

} // namespace FREYA_NAMESPACE
