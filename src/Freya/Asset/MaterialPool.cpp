#include "MaterialPool.hpp"

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Containers/SparseSet.hpp"

namespace FREYA_NAMESPACE
{
    struct MaterialPool::Impl
    {
        skr::Arc<MaterialDescriptorResources> materialsRes;
        skr::Arc<TexturePool>                 texturePool;
        skr::Arc<skr::Logger<MaterialPool>>   logger;
        SparseSet<Material>                   materials { 4096 };

        void writeBindlessMaterial(Material& material);
    };

    MaterialPool::MaterialPool(
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mImpl(std::make_unique<Impl>())
    {
        mImpl->materialsRes =
            serviceProvider->GetService<MaterialDescriptorResources>();
        mImpl->texturePool = serviceProvider->GetService<TexturePool>();
        mImpl->logger =
            serviceProvider->GetService<skr::Logger<MaterialPool>>();
    }

    MaterialPool::~MaterialPool() = default;

    std::uint32_t MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        auto&              i = *mImpl;
        MaterialCreateInfo info {};
        if (texturesPath.size() > 0)
            info.albedo = i.texturePool->CreateTextureFromFile(texturesPath[0]);
        if (texturesPath.size() > 1)
            info.normal = i.texturePool->CreateTextureFromFile(texturesPath[1]);
        if (texturesPath.size() > 2)
            info.roughness =
                i.texturePool->CreateTextureFromFile(texturesPath[2]);
        if (texturesPath.size() > 3)
            info.emissive =
                i.texturePool->CreateTextureFromFile(texturesPath[3]);
        if (texturesPath.size() > 4)
            info.metalness =
                i.texturePool->CreateTextureFromFile(texturesPath[4]);
        return Create(info);
    }

    std::uint32_t MaterialPool::Create(const MaterialCreateInfo& createInfo)
    {
        auto& i        = *mImpl;
        auto  material = Material {
            .createInfo = createInfo,
            .id         = static_cast<std::uint32_t>(i.materials.size()),
        };

        i.writeBindlessMaterial(material);
        i.materials.insert(material);

        i.logger->LogTrace("MaterialPool::Create id={}", material.id);
        return material.id;
    }

    void MaterialPool::Update(std::uint32_t             id,
                              const MaterialCreateInfo& createInfo)
    {
        auto& i             = *mImpl;
        auto& material      = i.materials[id];
        material.createInfo = createInfo;
        i.writeBindlessMaterial(material);
    }

    const MaterialCreateInfo& MaterialPool::GetCreateInfo(
        std::uint32_t id) const
    {
        return mImpl->materials[id].createInfo;
    }

    bool MaterialPool::Contains(const std::uint32_t id) const
    {
        return mImpl->materials.contains(id);
    }

    void MaterialPool::Destroy(const std::uint32_t id)
    {
        auto& i = *mImpl;
        if (!i.materials.contains(id))
            return;

        Material cleared {
            .createInfo = {},
            .id         = id,
        };
        i.writeBindlessMaterial(cleared);
        i.materials.remove(Material { .createInfo = {}, .id = id });
        i.logger->LogTrace("MaterialPool::Destroy id={}", id);
    }

    void MaterialPool::Impl::writeBindlessMaterial(Material& material)
    {
        const auto& info = material.createInfo;

        auto resolveIndex = [&](const std::optional<std::uint32_t>& textureId,
                                const std::uint32_t fallback) -> std::uint32_t {
            if (!textureId)
                return fallback;
            return MaterialDescriptorResources::TextureHeapIndex(*textureId);
        };

        MaterialGPU gpu {};
        gpu.albedoIndex = resolveIndex(info.albedo, kBindlessWhiteTexture);
        gpu.normalIndex = resolveIndex(info.normal, kBindlessWhiteTexture);
        gpu.roughnessIndex =
            resolveIndex(info.roughness, kBindlessWhiteTexture);
        gpu.emissiveIndex = resolveIndex(info.emissive, kBindlessBlackTexture);
        gpu.metalnessIndex =
            resolveIndex(info.metalness, kBindlessBlackTexture);
        gpu.occlusionIndex =
            resolveIndex(info.occlusion, kBindlessWhiteTexture);
        if (info.packedMetallicRoughness && !info.occlusion && info.roughness)
            gpu.occlusionIndex = gpu.roughnessIndex;

        gpu.albedoFactor       = info.albedoFactor;
        gpu.emissiveFactor     = glm::vec4(info.emissiveFactor, info.aoFactor);
        gpu.roughMetal         = { info.roughnessFactor, info.metalnessFactor };
        gpu.materialId         = static_cast<float>(material.id);
        gpu.alphaCutoff        = info.alphaCutoff;
        gpu.alphaMode          = static_cast<std::uint32_t>(info.alphaMode);
        gpu.clearcoat          = info.clearcoat;
        gpu.clearcoatRoughness = info.clearcoatRoughness;

        gpu.flags = 0;
        if (info.packedMetallicRoughness)
            gpu.flags |= kMaterialFlagPackedMR;
        if (info.unlit)
            gpu.flags |= kMaterialFlagUnlit;
        if (info.doubleSided)
            gpu.flags |= kMaterialFlagDoubleSided;
        if (info.receiveShadows)
            gpu.flags |= kMaterialFlagReceiveShadow;

        materialsRes->WriteMaterial(material.id, gpu);
    }
} // namespace FREYA_NAMESPACE
