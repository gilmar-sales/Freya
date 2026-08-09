#include "Freya/Asset/MeshPool.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Containers/MeshSet.hpp"
#include "Freya/Core/Buffer.hpp"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes           = 1024 * 1024;
    constexpr auto MinVertexBufferSize = 4 * MegaBytes;
    constexpr auto MinIndexBufferSize  = 4 * MegaBytes;
    constexpr auto kMinIndicesForLod   = 756u;

    struct MeshPool::Impl
    {
        skr::Arc<Device>                device;
        skr::Arc<PhysicalDevice>        physicalDevice;
        skr::Arc<CommandPool>           commandPool;
        skr::Arc<skr::Logger<MeshPool>> logger;

        std::vector<skr::Arc<Buffer>> stagingBuffers;

        skr::Arc<Buffer> vertexBuffer;
        std::uint32_t    vertexOffset = 0;

        skr::Arc<Buffer> indexBuffer;
        std::uint32_t    indexOffset = 0;

        MeshSet                  meshes;
        std::vector<MeshLodInfo> meshLods;

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
            stagingBuffers.reserve(64);
            vertexBuffer = BufferBuilder(device)
                               .SetSize(MinVertexBufferSize)
                               .SetUsage(BufferUsage::Vertex)
                               .Build();
            indexBuffer  = BufferBuilder(device)
                               .SetSize(MinIndexBufferSize)
                               .SetUsage(BufferUsage::Index)
                               .Build();
        }

        skr::Arc<Buffer> createStagingBuffer(std::uint32_t size)
        {
            const auto bufferSize = (size / MegaBytes + 4) * MegaBytes;
            auto       stagingBuffer =
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
                    return stagingBuffer;
            }
            return createStagingBuffer(size);
        }

        void growBuffer(skr::Arc<Buffer>& buffer, std::uint32_t& usedBytes,
                        std::uint32_t neededExtra, BufferUsage usage,
                        std::uint32_t minSize)
        {
            const auto need = usedBytes + neededExtra;
            if (buffer && buffer->GetSize() >= need)
                return;

            auto newSize =
                buffer ? static_cast<std::uint32_t>(buffer->GetSize())
                       : minSize;
            while (newSize < need)
                newSize = std::max(newSize * 2u, minSize);

            auto newBuffer =
                BufferBuilder(device).SetSize(newSize).SetUsage(usage).Build();

            if (buffer && usedBytes > 0)
            {
                constexpr auto beginInfo =
                    vk::CommandBufferBeginInfo().setFlags(
                        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
                const auto cmd = commandPool->CreateCommandBuffer();
                cmd.begin(beginInfo);
                const auto region =
                    vk::BufferCopy().setSrcOffset(0).setDstOffset(0).setSize(
                        usedBytes);
                cmd.copyBuffer(buffer->Get(), newBuffer->Get(), 1, &region);
                cmd.end();
                const auto submit = vk::SubmitInfo()
                                        .setCommandBufferCount(1)
                                        .setPCommandBuffers(&cmd);
                device->GetTransferQueue().submit(submit);
                device->GetTransferQueue().waitIdle();
                commandPool->FreeCommandBuffer(cmd);
            }

            buffer = std::move(newBuffer);
        }

        std::vector<std::vector<std::uint32_t>> buildLodIndices(
            const std::vector<Vertex>&        vertices,
            const std::vector<std::uint32_t>& lod0)
        {
            std::vector<std::vector<std::uint32_t>> lods;
            lods.push_back(lod0);

            if (lod0.size() < kMinIndicesForLod || vertices.empty())
                return lods;

            std::vector<float> positions(vertices.size() * 3);
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                positions[i * 3 + 0] = vertices[i].position.x;
                positions[i * 3 + 1] = vertices[i].position.y;
                positions[i * 3 + 2] = vertices[i].position.z;
            }

            auto current = lod0;
            for (std::uint32_t level = 1; level < kMaxLodsPerMesh; ++level)
            {
                const auto target =
                    std::max<std::size_t>((current.size() / 2) / 3 * 3, 3);
                if (target >= current.size())
                    break;

                std::vector<std::uint32_t> simplified(current.size());
                float                      resultError = 0.0f;
                const auto                 written     = meshopt_simplify(
                    simplified.data(), current.data(), current.size(),
                    positions.data(), vertices.size(), sizeof(float) * 3,
                    target, 1e-2f * static_cast<float>(level), 0, &resultError);
                simplified.resize(written);

                if (written < 3 || written >= current.size() * 9 / 10)
                    break;

                meshopt_optimizeVertexCache(
                    simplified.data(), simplified.data(), simplified.size(),
                    vertices.size());
                lods.push_back(std::move(simplified));
                current = lods.back();
            }

            return lods;
        }

        std::uint32_t createMesh(const std::vector<Vertex>&        vertices,
                                 const std::vector<std::uint32_t>& indicesIn)
        {
            auto indices = indicesIn;
            if (!indices.empty() && !vertices.empty())
            {
                meshopt_optimizeVertexCache(indices.data(), indices.data(),
                                            indices.size(), vertices.size());
            }

            const auto lodIndexSets = buildLodIndices(vertices, indices);

            logger->LogTrace(
                "Creating mesh with {} vertices, {} lod0 indices, {} LODs.",
                vertices.size(), indices.size(), lodIndexSets.size());

            const auto vertexMemorySize =
                static_cast<std::uint32_t>(vertices.size() * sizeof(Vertex));
            std::uint32_t totalIndexBytes = 0;
            for (const auto& lod : lodIndexSets)
            {
                totalIndexBytes += static_cast<std::uint32_t>(
                    lod.size() * sizeof(std::uint32_t));
            }

            growBuffer(vertexBuffer, vertexOffset, vertexMemorySize,
                       BufferUsage::Vertex, MinVertexBufferSize);
            growBuffer(indexBuffer, indexOffset, totalIndexBytes,
                       BufferUsage::Index, MinIndexBufferSize);

            constexpr auto beginInfo = vk::CommandBufferBeginInfo().setFlags(
                vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
            const auto commandBuffer = commandPool->CreateCommandBuffer();
            commandBuffer.begin(beginInfo);

            const auto stagingBuffer =
                queryStagingBuffer(vertexMemorySize + totalIndexBytes);
            stagingBuffer->Copy(vertices.data(), vertexMemorySize);

            std::uint32_t stagingIndexAt = vertexMemorySize;
            for (const auto& lod : lodIndexSets)
            {
                const auto bytes = static_cast<std::uint32_t>(
                    lod.size() * sizeof(std::uint32_t));
                if (bytes > 0)
                {
                    stagingBuffer->Copy(lod.data(), bytes, stagingIndexAt);
                    stagingIndexAt += bytes;
                }
            }

            const auto vertexCopy =
                vk::BufferCopy()
                    .setSrcOffset(0)
                    .setSize(vertexMemorySize)
                    .setDstOffset(vertexOffset);
            if (vertexMemorySize > 0)
            {
                commandBuffer.copyBuffer(
                    stagingBuffer->Get(), vertexBuffer->Get(), 1, &vertexCopy);
            }

            std::uint32_t srcIndexAt = vertexMemorySize;
            std::uint32_t dstIndexAt = indexOffset;
            for (const auto& lod : lodIndexSets)
            {
                const auto bytes = static_cast<std::uint32_t>(
                    lod.size() * sizeof(std::uint32_t));
                if (bytes == 0)
                    continue;
                const auto indexCopy =
                    vk::BufferCopy()
                        .setSrcOffset(srcIndexAt)
                        .setSize(bytes)
                        .setDstOffset(dstIndexAt);
                commandBuffer.copyBuffer(
                    stagingBuffer->Get(), indexBuffer->Get(), 1, &indexCopy);
                srcIndexAt += bytes;
                dstIndexAt += bytes;
            }

            commandBuffer.end();
            const auto submitInfo =
                vk::SubmitInfo().setCommandBufferCount(1).setPCommandBuffers(
                    &commandBuffer);
            device->GetTransferQueue().submit(submitInfo);
            device->GetTransferQueue().waitIdle();
            commandPool->FreeCommandBuffer(commandBuffer);

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

            const auto lodBase = static_cast<std::uint32_t>(meshLods.size());
            const auto vertexOffUnits = static_cast<std::int32_t>(
                vertexOffset / static_cast<std::uint32_t>(sizeof(Vertex)));

            std::uint32_t runningIndexOffset = indexOffset;
            MeshLodInfo   lod0Info {};
            for (std::size_t i = 0; i < lodIndexSets.size(); ++i)
            {
                const auto count =
                    static_cast<std::uint32_t>(lodIndexSets[i].size());
                const auto first =
                    runningIndexOffset /
                    static_cast<std::uint32_t>(sizeof(std::uint32_t));
                const MeshLodInfo info {
                    .indexCount   = count,
                    .firstIndex   = first,
                    .vertexOffset = vertexOffUnits,
                };
                meshLods.push_back(info);
                if (i == 0)
                    lod0Info = info;
                runningIndexOffset +=
                    count * static_cast<std::uint32_t>(sizeof(std::uint32_t));
            }

            const auto mesh = Mesh {
                .vertexBufferOffset = vertexOffset,
                .indexBufferOffset  = indexOffset,
                .firstIndex         = lod0Info.firstIndex,
                .vertexOffset       = lod0Info.vertexOffset,
                .indexCount         = lod0Info.indexCount,
                .lodCount = static_cast<std::uint32_t>(lodIndexSets.size()),
                .lodBase  = lodBase,
                .aabbMin  = aabbMin,
                .aabbMax  = aabbMax,
                .id       = static_cast<std::uint32_t>(meshes.size()),
            };

            meshes.insert(mesh);
            vertexOffset += vertexMemorySize;
            indexOffset = runningIndexOffset;
            return mesh.id;
        }

        std::uint32_t processMesh(const aiMesh* mesh, const aiScene* scene)
        {
            std::vector<Vertex>        vertices;
            std::vector<std::uint32_t> indices;

            for (auto i = 0u; i < mesh->mNumVertices; ++i)
            {
                const auto& aVertex  = mesh->mVertices[i];
                const auto& aNormal  = mesh->mNormals[i];
                const auto& aTangent = mesh->HasTangentsAndBitangents()
                                           ? mesh->mTangents[i]
                                           : aiVector3D(1, 0, 0);
                const auto  aTextCoord =
                    mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i]
                                              : aiVector3D(0, 0, 0);

                const auto material = scene->mMaterials[mesh->mMaterialIndex];
                aiColor3D  aColor(1.0, 1.0, 1.0);
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
                    indices.push_back(face.mIndices[j]);
            }

            return createMesh(vertices, indices);
        }

        void processNode(std::vector<std::uint32_t>& meshIds,
                         const aiNode*               node,
                         const aiScene*              scene)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; ++i)
            {
                const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshIds.push_back(processMesh(mesh, scene));
            }
            for (unsigned int i = 0; i < node->mNumChildren; ++i)
                processNode(meshIds, node->mChildren[i], scene);
        }

        std::vector<std::uint32_t> createMeshFromFile(const std::string& path)
        {
            logger->LogTrace("Creating mesh from file: {}", path);
            auto             meshIds = std::vector<std::uint32_t>();
            Assimp::Importer importer;
            // Without KEEP_HIERARCHY, PreTransformVertices collapses every
            // submesh into one — Blend materials (e.g. lamp bulb) cannot be
            // assigned and disappear into the opaque body draw.
            importer.SetPropertyBool(AI_CONFIG_PP_PTV_KEEP_HIERARCHY, true);
            // Do not OptimizeMeshes: shared placeholder materials in glTF
            // would re-merge named parts (body/bulb/switch) after PTV.
            const aiScene* scene = importer.ReadFile(
                path,
                aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                    aiProcess_SortByPType | aiProcess_GenNormals |
                    aiProcess_GenUVCoords | aiProcess_JoinIdenticalVertices |
                    aiProcess_GlobalScale | aiProcess_PreTransformVertices |
                    aiProcess_ValidateDataStructure);

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
                !scene->mRootNode)
            {
                logger->LogError("Failed loading mesh: {}",
                                 importer.GetErrorString());
                return meshIds;
            }

            processNode(meshIds, scene->mRootNode, scene);
            logger->LogTrace("Loaded {} mesh(es) from {}", meshIds.size(),
                             path);
            return meshIds;
        }

        void bindGeometry() const
        {
            constexpr vk::DeviceSize zeroOffset = 0;
            auto&                    cb = commandPool->GetCommandBuffer();
            cb.bindVertexBuffers(0, 1, &vertexBuffer->Get(), &zeroOffset);
            cb.bindIndexBuffer(indexBuffer->Get(), 0, vk::IndexType::eUint32);
        }

        void draw(std::uint32_t meshId)
        {
            if (!meshes.contains(meshId))
                return;
            const auto& mesh = meshes[meshId];
            bindGeometry();
            commandPool->GetCommandBuffer().drawIndexed(
                mesh.indexCount, 1, mesh.firstIndex, mesh.vertexOffset, 0);
        }

        void drawInstanced(std::uint32_t meshId, size_t instanceCount,
                           size_t firstInstance)
        {
            if (!meshes.contains(meshId))
                return;
            const auto& mesh = meshes[meshId];
            bindGeometry();
            commandPool->GetCommandBuffer().drawIndexed(
                mesh.indexCount, instanceCount, mesh.firstIndex,
                mesh.vertexOffset, firstInstance);
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

    std::uint32_t MeshPool::CreateMesh(
        const std::vector<Vertex>&        vertices,
        const std::vector<std::uint32_t>& indices)
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
            const auto& mesh = mImpl->meshes[id];
            out[id]          = MeshInfo {
                .lodCount = mesh.lodCount,
                .lodBase  = mesh.lodBase,
                .aabbMin  = glm::vec4(mesh.aabbMin, 0.0f),
                .aabbMax  = glm::vec4(mesh.aabbMax, 0.0f),
            };
        }
    }

    void MeshPool::FillMeshLods(std::vector<MeshLodInfo>& out) const
    {
        out = mImpl->meshLods;
    }

    void MeshPool::BindGeometry() const
    {
        mImpl->bindGeometry();
    }

    const skr::Arc<Buffer>& MeshPool::GetVertexBuffer() const
    {
        return mImpl->vertexBuffer;
    }

    const skr::Arc<Buffer>& MeshPool::GetIndexBuffer() const
    {
        return mImpl->indexBuffer;
    }

    void MeshPool::Draw(const std::uint32_t meshId)
    {
        mImpl->draw(meshId);
    }

    void MeshPool::DrawInstanced(std::uint32_t meshId, size_t instanceCount,
                                 size_t firstInstance)
    {
        mImpl->drawInstanced(meshId, instanceCount, firstInstance);
    }

} // namespace FREYA_NAMESPACE
