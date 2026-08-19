#include "Freya/Asset/MeshPool.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Containers/MeshSet.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#ifndef NDEBUG
    #undef __OPTIMIZE__
#endif
#include "Freya/Vendor/stb_image.h"
#ifndef NDEBUG
    #define __OPTIMIZE__ 1
#endif

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

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
        skr::Arc<MaterialPool>          materialPool;
        skr::Arc<TexturePool>           texturePool;

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
                 inLogger,
             skr::Arc<MaterialPool>
                 inMaterialPool,
             skr::Arc<TexturePool>
                 inTexturePool) :
            device(std::move(inDevice)),
            physicalDevice(std::move(inPhysicalDevice)),
            commandPool(std::move(inCommandPool)), logger(std::move(inLogger)),
            materialPool(std::move(inMaterialPool)),
            texturePool(std::move(inTexturePool)), meshes(4096)
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
                                 const std::vector<std::uint32_t>& indicesIn,
                                 const bool inflateAabb = false)
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
            if (inflateAabb && !vertices.empty())
            {
                const glm::vec3 center = 0.5f * (aabbMin + aabbMax);
                const glm::vec3 extent = 0.5f * (aabbMax - aabbMin);
                const float     radius = glm::length(extent) * 1.25f;
                aabbMin                = center - glm::vec3(radius);
                aabbMax                = center + glm::vec3(radius);
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
                .skinned  = inflateAabb,
                .id       = static_cast<std::uint32_t>(meshes.size()),
            };

            meshes.insert(mesh);
            vertexOffset += vertexMemorySize;
            indexOffset = runningIndexOffset;
            return mesh.id;
        }

        std::uint32_t processMesh(const aiMesh*  mesh,
                                  const aiScene* scene,
                                  bool           applyDiffuseColor = true)
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

                aiColor3D aColor(1.0, 1.0, 1.0);
                if (applyDiffuseColor)
                {
                    const auto material =
                        scene->mMaterials[mesh->mMaterialIndex];
                    material->Get(AI_MATKEY_COLOR_DIFFUSE, aColor);
                }

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

        static std::string parentDirectory(const std::string& path)
        {
            const auto pos = path.find_last_of("/\\");
            if (pos == std::string::npos)
                return ".";
            return path.substr(0, pos);
        }

        static std::string normalizeSlashes(std::string path)
        {
            for (auto& c : path)
            {
                if (c == '\\')
                    c = '/';
            }
            return path;
        }

        static bool tryTexturePath(const aiMaterial* mat,
                                   aiTextureType     type,
                                   aiString&         out)
        {
            return mat->GetTextureCount(type) > 0 &&
                   mat->GetTexture(type, 0, &out) == AI_SUCCESS &&
                   out.length > 0;
        }

        std::optional<std::uint32_t> loadAssimpTexture(
            const aiScene*                                  scene,
            const std::string&                              directory,
            const aiString&                                 texPath,
            std::unordered_map<std::string, std::uint32_t>& cache)
        {
            std::string key = texPath.C_Str();
            if (key.empty())
                return std::nullopt;

            if (const auto it = cache.find(key); it != cache.end())
                return it->second;

            std::uint32_t id       = 0;
            const auto*   embedded = scene->GetEmbeddedTexture(texPath.C_Str());
            if (embedded)
            {
                if (embedded->mHeight == 0)
                {
                    int   width  = 0;
                    int   height = 0;
                    int   n      = 0;
                    auto* pixels = stbi_load_from_memory(
                        reinterpret_cast<const stbi_uc*>(embedded->pcData),
                        static_cast<int>(embedded->mWidth), &width, &height, &n,
                        4);
                    if (!pixels)
                    {
                        logger->LogError("Failed decoding embedded texture {}",
                                         key);
                        return std::nullopt;
                    }
                    id = texturePool->CreateTextureFromMemory(
                        pixels, static_cast<std::uint32_t>(width),
                        static_cast<std::uint32_t>(height), 4);
                    stbi_image_free(pixels);
                }
                else
                {
                    const auto                w = embedded->mWidth;
                    const auto                h = embedded->mHeight;
                    std::vector<std::uint8_t> rgba(
                        static_cast<size_t>(w) * h * 4);
                    for (unsigned i = 0; i < w * h; ++i)
                    {
                        const auto& texel = embedded->pcData[i];
                        rgba[i * 4 + 0]   = texel.r;
                        rgba[i * 4 + 1]   = texel.g;
                        rgba[i * 4 + 2]   = texel.b;
                        rgba[i * 4 + 3]   = texel.a;
                    }
                    id = texturePool->CreateTextureFromMemory(
                        rgba.data(), w, h, 4);
                }
            }
            else
            {
                auto       file = normalizeSlashes(key);
                const bool absolute =
                    file.size() >= 2 && (file[0] == '/' || file[1] == ':');
                if (!absolute)
                    file = directory + "/" + file;
                id = texturePool->CreateTextureFromFile(file);
            }

            cache.emplace(key, id);
            return id;
        }

        std::optional<std::uint32_t> loadFirstTexture(
            const aiMaterial*  mat,
            const aiScene*     scene,
            const std::string& directory,
            std::initializer_list<aiTextureType>
                                                            types,
            std::unordered_map<std::string, std::uint32_t>& cache,
            aiString* matchedPath = nullptr)
        {
            aiString path;
            for (auto type : types)
            {
                if (!tryTexturePath(mat, type, path))
                    continue;
                auto id = loadAssimpTexture(scene, directory, path, cache);
                if (id)
                {
                    if (matchedPath)
                        *matchedPath = path;
                    return id;
                }
            }
            return std::nullopt;
        }

        std::uint32_t importAssimpMaterial(
            const aiMaterial*                               mat,
            const aiScene*                                  scene,
            const std::string&                              directory,
            std::unordered_map<std::string, std::uint32_t>& cache)
        {
            MaterialCreateInfo info {};

            info.albedo = loadFirstTexture(
                mat, scene, directory,
                { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, cache);
            info.normal = loadFirstTexture(
                mat, scene, directory,
                { aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA,
                  aiTextureType_HEIGHT },
                cache);
            info.emissive = loadFirstTexture(
                mat, scene, directory,
                { aiTextureType_EMISSIVE, aiTextureType_EMISSION_COLOR },
                cache);
            info.occlusion = loadFirstTexture(
                mat, scene, directory,
                { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP,
                  aiTextureType_AMBIENT },
                cache);

            aiString metalPath;
            aiString roughPath;
            aiString packedPath;
            auto     metalTex = loadFirstTexture(
                mat, scene, directory, { aiTextureType_METALNESS }, cache,
                &metalPath);
            auto roughTex = loadFirstTexture(
                mat, scene, directory,
                { aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_SHININESS },
                cache, &roughPath);
            auto packedTex = loadFirstTexture(
                mat, scene, directory,
                { aiTextureType_GLTF_METALLIC_ROUGHNESS,
                  aiTextureType_UNKNOWN },
                cache, &packedPath);

            const auto samePacked = metalTex && roughTex &&
                                    normalizeSlashes(metalPath.C_Str()) ==
                                        normalizeSlashes(roughPath.C_Str());
            if (packedTex)
            {
                info.packedMetallicRoughness = true;
                info.roughness               = packedTex;
            }
            else if (samePacked || (metalTex && !roughTex))
            {
                info.packedMetallicRoughness = true;
                info.roughness               = metalTex ? metalTex : roughTex;
            }
            else
            {
                info.roughness = roughTex;
                info.metalness = metalTex;
            }

            aiColor4D base(1.f, 1.f, 1.f, 1.f);
            if (mat->Get(AI_MATKEY_BASE_COLOR, base) != AI_SUCCESS)
            {
                aiColor3D diffuse(1.f, 1.f, 1.f);
                if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
                    base = aiColor4D(diffuse.r, diffuse.g, diffuse.b, 1.f);
            }
            info.albedoFactor = glm::vec4(base.r, base.g, base.b, base.a);

            float metallic  = 1.f;
            float roughness = 1.f;
            if (mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
                info.metalnessFactor = metallic;
            if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
                info.roughnessFactor = roughness;

            aiColor3D emissive(1.f, 1.f, 1.f);
            if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
                info.emissiveFactor = { emissive.r, emissive.g, emissive.b };

            float opacity = 1.f;
            mat->Get(AI_MATKEY_OPACITY, opacity);
            info.albedoFactor.a *= opacity;

            aiString alphaMode;
            if (mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
            {
                const std::string mode = alphaMode.C_Str();
                if (mode == "MASK")
                    info.alphaMode = AlphaMode::Mask;
                else if (mode == "BLEND")
                    info.alphaMode = AlphaMode::Blend;
            }
            else if (info.albedoFactor.a < 0.99f)
            {
                info.alphaMode = AlphaMode::Blend;
            }

            float cutoff = 0.5f;
            if (mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff) == AI_SUCCESS)
                info.alphaCutoff = cutoff;
            else if (info.alphaMode == AlphaMode::Mask)
                info.alphaCutoff = 0.5f;

            int twoSided = 0;
            if (mat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
                info.doubleSided = twoSided != 0;

            int shading = 0;
            if (mat->Get(AI_MATKEY_SHADING_MODEL, shading) == AI_SUCCESS)
                info.unlit = shading == aiShadingMode_Unlit;

            float clearcoat = 0.f;
            if (mat->Get(AI_MATKEY_CLEARCOAT_FACTOR, clearcoat) == AI_SUCCESS)
                info.clearcoat = clearcoat;
            float coatRough = 0.03f;
            if (mat->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, coatRough) ==
                AI_SUCCESS)
                info.clearcoatRoughness = coatRough;

            return materialPool->Create(info);
        }

        std::vector<ModelSubmesh> createModelFromFile(const std::string& path)
        {
            logger->LogTrace("Creating model from file: {}", path);
            auto             submeshes = std::vector<ModelSubmesh>();
            Assimp::Importer importer;
            importer.SetPropertyBool(AI_CONFIG_PP_PTV_KEEP_HIERARCHY, true);
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
                logger->LogError("Failed loading model: {}",
                                 importer.GetErrorString());
                return submeshes;
            }

            const auto directory = normalizeSlashes(parentDirectory(path));
            std::unordered_map<std::string, std::uint32_t> textureCache;
            std::vector<std::uint32_t> materialIds(scene->mNumMaterials, 0);
            for (unsigned i = 0; i < scene->mNumMaterials; ++i)
            {
                materialIds[i] = importAssimpMaterial(
                    scene->mMaterials[i], scene, directory, textureCache);
            }

            const auto walk = [&](auto&& self, const aiNode* node) -> void {
                for (unsigned i = 0; i < node->mNumMeshes; ++i)
                {
                    const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                    const auto    meshId =
                        processMesh(mesh, scene, /*applyDiffuseColor*/ false);
                    const auto matIndex = mesh->mMaterialIndex;
                    const auto materialId =
                        matIndex < materialIds.size() ? materialIds[matIndex]
                                                      : 0u;
                    submeshes.push_back(ModelSubmesh { meshId, materialId });
                }
                for (unsigned i = 0; i < node->mNumChildren; ++i)
                    self(self, node->mChildren[i]);
            };
            walk(walk, scene->mRootNode);

            logger->LogTrace("Loaded {} submesh(es) from {}", submeshes.size(),
                             path);
            return submeshes;
        }

        static glm::mat4 toGlm(const aiMatrix4x4& m)
        {
            // Assimp stores rows contiguously; glm is column-major.
            return glm::transpose(glm::make_mat4(&m.a1));
        }

        static std::string aiName(const aiString& s)
        {
            return std::string(s.C_Str());
        }

        void collectBoneNames(
            const aiScene*                                  scene,
            std::unordered_map<std::string, std::uint32_t>& nameToIndex,
            Skeleton&                                       skeleton)
        {
            for (unsigned m = 0; m < scene->mNumMeshes; ++m)
            {
                const aiMesh* mesh = scene->mMeshes[m];
                for (unsigned b = 0; b < mesh->mNumBones; ++b)
                {
                    const auto name = aiName(mesh->mBones[b]->mName);
                    if (nameToIndex.contains(name))
                        continue;
                    const auto idx =
                        static_cast<std::uint32_t>(skeleton.names.size());
                    nameToIndex.emplace(name, idx);
                    skeleton.names.push_back(name);
                    skeleton.parents.push_back(-1);
                    skeleton.inverseBind.push_back(
                        toGlm(mesh->mBones[b]->mOffsetMatrix));
                    skeleton.restLocal.push_back(glm::mat4(1.f));
                }
            }
        }

        void assignParentsAndRest(
            const aiNode*                                   node,
            const std::int32_t                              parentBone,
            std::unordered_map<std::string, std::uint32_t>& nameToIndex,
            Skeleton&                                       skeleton)
        {
            const auto   name = aiName(node->mName);
            std::int32_t self = parentBone;
            if (const auto it = nameToIndex.find(name); it != nameToIndex.end())
            {
                self = static_cast<std::int32_t>(it->second);
                skeleton.parents[it->second]   = parentBone;
                skeleton.restLocal[it->second] = toGlm(node->mTransformation);
            }
            for (unsigned i = 0; i < node->mNumChildren; ++i)
                assignParentsAndRest(node->mChildren[i], self, nameToIndex,
                                     skeleton);
        }

        std::uint32_t processSkinnedMesh(
            const aiMesh* mesh, const aiScene* scene,
            const std::unordered_map<std::string, std::uint32_t>& nameToIndex)
        {
            std::vector<Vertex>        vertices(mesh->mNumVertices);
            std::vector<std::uint32_t> indices;

            for (auto i = 0u; i < mesh->mNumVertices; ++i)
            {
                const auto& aVertex = mesh->mVertices[i];
                const auto& aNormal =
                    mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
                const auto& aTangent = mesh->HasTangentsAndBitangents()
                                           ? mesh->mTangents[i]
                                           : aiVector3D(1, 0, 0);
                const auto  aTextCoord =
                    mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i]
                                              : aiVector3D(0, 0, 0);

                aiColor3D aColor(1.0, 1.0, 1.0);
                if (scene->mMaterials &&
                    mesh->mMaterialIndex < scene->mNumMaterials)
                {
                    scene->mMaterials[mesh->mMaterialIndex]->Get(
                        AI_MATKEY_COLOR_DIFFUSE, aColor);
                }

                vertices[i] = Vertex {
                    .position = glm::vec3(aVertex.x, aVertex.y, aVertex.z),
                    .color    = glm::vec3(aColor.r, aColor.g, aColor.b),
                    .normal   = glm::vec3(aNormal.x, aNormal.y, aNormal.z),
                    .tangent  = glm::vec3(aTangent.x, aTangent.y, aTangent.z),
                    .texCoord = glm::vec2(aTextCoord.x, aTextCoord.y),
                    .joints   = glm::uvec4(0),
                    .weights  = glm::vec4(0.f),
                };
            }

            std::vector<std::uint32_t> weightCounts(mesh->mNumVertices, 0);
            for (unsigned b = 0; b < mesh->mNumBones; ++b)
            {
                const aiBone* bone = mesh->mBones[b];
                const auto    it   = nameToIndex.find(aiName(bone->mName));
                if (it == nameToIndex.end())
                    continue;
                const std::uint32_t joint = it->second;
                for (unsigned w = 0; w < bone->mNumWeights; ++w)
                {
                    const auto& aw   = bone->mWeights[w];
                    const auto  vi   = aw.mVertexId;
                    auto&       slot = weightCounts[vi];
                    if (slot >= 4 || aw.mWeight <= 0.f)
                        continue;
                    vertices[vi].joints[slot]  = joint;
                    vertices[vi].weights[slot] = aw.mWeight;
                    ++slot;
                }
            }

            for (auto& v : vertices)
            {
                const float sum =
                    v.weights.x + v.weights.y + v.weights.z + v.weights.w;
                if (sum > 1e-6f)
                    v.weights /= sum;
                else
                    v.weights = glm::vec4(1.f, 0.f, 0.f, 0.f);
            }

            for (auto i = 0u; i < mesh->mNumFaces; ++i)
            {
                const auto& face = mesh->mFaces[i];
                for (auto j = 0u; j < face.mNumIndices; ++j)
                    indices.push_back(face.mIndices[j]);
            }

            return createMesh(vertices, indices, true);
        }

        void processSkinnedNode(
            std::vector<std::uint32_t>& meshIds, const aiNode* node,
            const aiScene*                                        scene,
            const std::unordered_map<std::string, std::uint32_t>& nameToIndex)
        {
            for (unsigned int i = 0; i < node->mNumMeshes; ++i)
            {
                const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                meshIds.push_back(processSkinnedMesh(mesh, scene, nameToIndex));
            }
            for (unsigned int i = 0; i < node->mNumChildren; ++i)
                processSkinnedNode(meshIds, node->mChildren[i], scene,
                                   nameToIndex);
        }

        static AnimationClip convertAnimation(
            const aiAnimation*                                    anim,
            const std::unordered_map<std::string, std::uint32_t>& nameToIndex)
        {
            AnimationClip clip;
            clip.name       = aiName(anim->mName);
            const float tps = anim->mTicksPerSecond > 0.0
                                  ? static_cast<float>(anim->mTicksPerSecond)
                                  : 25.f;
            clip.ticksPerSecond = tps;
            clip.duration =
                static_cast<float>(anim->mDuration) / std::max(tps, 1e-6f);

            for (unsigned c = 0; c < anim->mNumChannels; ++c)
            {
                const aiNodeAnim* ch = anim->mChannels[c];
                const auto        it = nameToIndex.find(aiName(ch->mNodeName));
                if (it == nameToIndex.end())
                    continue;

                AnimationChannel out;
                out.jointIndex = it->second;
                out.translations.reserve(ch->mNumPositionKeys);
                for (unsigned i = 0; i < ch->mNumPositionKeys; ++i)
                {
                    const auto& k = ch->mPositionKeys[i];
                    out.translations.push_back(AnimationVecKey {
                        .time  = static_cast<float>(k.mTime) / tps,
                        .value = glm::vec3(k.mValue.x, k.mValue.y, k.mValue.z),
                    });
                }
                out.rotations.reserve(ch->mNumRotationKeys);
                for (unsigned i = 0; i < ch->mNumRotationKeys; ++i)
                {
                    const auto& k = ch->mRotationKeys[i];
                    out.rotations.push_back(AnimationQuatKey {
                        .time  = static_cast<float>(k.mTime) / tps,
                        .value = glm::normalize(glm::quat(
                            k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z)),
                    });
                }
                out.scales.reserve(ch->mNumScalingKeys);
                for (unsigned i = 0; i < ch->mNumScalingKeys; ++i)
                {
                    const auto& k = ch->mScalingKeys[i];
                    out.scales.push_back(AnimationVecKey {
                        .time  = static_cast<float>(k.mTime) / tps,
                        .value = glm::vec3(k.mValue.x, k.mValue.y, k.mValue.z),
                    });
                }
                clip.channels.push_back(std::move(out));
            }
            return clip;
        }

        SkinnedModel createSkinnedModelFromFile(const std::string& path)
        {
            SkinnedModel out;
            logger->LogTrace("Creating skinned model from file: {}", path);
            Assimp::Importer importer;
            importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
            const aiScene* scene = importer.ReadFile(
                path,
                aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                    aiProcess_SortByPType | aiProcess_GenNormals |
                    aiProcess_GenUVCoords | aiProcess_JoinIdenticalVertices |
                    aiProcess_GlobalScale | aiProcess_LimitBoneWeights |
                    aiProcess_ValidateDataStructure);

            if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
                !scene->mRootNode)
            {
                logger->LogError("Failed loading skinned mesh: {}",
                                 importer.GetErrorString());
                return out;
            }

            std::unordered_map<std::string, std::uint32_t> nameToIndex;
            collectBoneNames(scene, nameToIndex, out.skeleton);
            if (out.skeleton.JointCount() == 0)
            {
                logger->LogError("Skinned load found no bones in '{}'; use "
                                 "CreateMeshFromFile for static models.",
                                 path);
                return out;
            }

            assignParentsAndRest(scene->mRootNode, -1, nameToIndex,
                                 out.skeleton);
            // Refresh inverse-bind from first bone occurrence (already set);
            // later meshes may repeat the same bone with same offset.
            for (unsigned m = 0; m < scene->mNumMeshes; ++m)
            {
                const aiMesh* mesh = scene->mMeshes[m];
                for (unsigned b = 0; b < mesh->mNumBones; ++b)
                {
                    const auto name = aiName(mesh->mBones[b]->mName);
                    const auto it   = nameToIndex.find(name);
                    if (it == nameToIndex.end())
                        continue;
                    out.skeleton.inverseBind[it->second] =
                        toGlm(mesh->mBones[b]->mOffsetMatrix);
                }
            }

            processSkinnedNode(out.meshIds, scene->mRootNode, scene,
                               nameToIndex);

            for (unsigned a = 0; a < scene->mNumAnimations; ++a)
                out.clips.push_back(
                    convertAnimation(scene->mAnimations[a], nameToIndex));

            logger->LogTrace(
                "Loaded skinned model '{}' ({} meshes, {} joints, {} clips)",
                path, out.meshIds.size(), out.skeleton.JointCount(),
                out.clips.size());
            return out;
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
                       const skr::Arc<skr::Logger<MeshPool>>& logger,
                       const skr::Arc<MaterialPool>&          materialPool,
                       const skr::Arc<TexturePool>&           texturePool) :
        mImpl(std::make_unique<Impl>(device, physicalDevice, commandPool,
                                     logger, materialPool, texturePool))
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

    std::vector<ModelSubmesh> MeshPool::CreateModelFromFile(
        const std::string& path)
    {
        return mImpl->createModelFromFile(path);
    }

    SkinnedModel MeshPool::CreateSkinnedModelFromFile(const std::string& path)
    {
        return mImpl->createSkinnedModelFromFile(path);
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
