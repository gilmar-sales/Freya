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
        std::uint32_t                         nextId = 0;

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

    MaterialPool::~MaterialPool()
    {
        if (!mImpl || !mImpl->materialsRes)
            return;

        for (const auto& material : mImpl->materials.getDense())
        {
            mImpl->materialsRes->WriteMaterial(material.id, MaterialGPU {});
        }
    }

    MaterialHandle MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        auto&              i = *mImpl;
        MaterialCreateInfo info {};
        if (!texturesPath.empty())
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
        if (texturesPath.size() > 5)
            info.occlusion =
                i.texturePool->CreateTextureFromFile(texturesPath[5]);
        return Create(info);
    }

    MaterialHandle MaterialPool::Create(const MaterialCreateInfo& createInfo)
    {
        auto& i        = *mImpl;
        auto  id       = i.nextId++;
        auto  material = Material {
             .createInfo = createInfo,
             .id         = id,
        };

        i.writeBindlessMaterial(material);
        i.materials.insert(material);

        i.logger->LogTrace("MaterialPool::Create id={}", material.id);
        return MaterialHandle { material.id };
    }

    void MaterialPool::Update(MaterialHandle            id,
                              const MaterialCreateInfo& createInfo)
    {
        auto& i        = *mImpl;
        auto* material = i.materials.find(id.Id());

        if (!id.IsValid() || material == nullptr)
            return;

        material->createInfo = createInfo;
        i.writeBindlessMaterial(*material);
    }

    const MaterialCreateInfo& MaterialPool::GetCreateInfo(
        MaterialHandle id) const
    {
        return mImpl->materials.atId(id.Id()).createInfo;
    }

    MaterialDrawInfo MaterialPool::GetDrawInfo(MaterialHandle id) const
    {
        const auto&      info = GetCreateInfo(id);
        MaterialDrawInfo draw {};
        draw.techniqueId = info.techniqueId;
        draw.alphaMode   = info.alphaMode;
        return draw;
    }

    bool MaterialPool::Contains(const MaterialHandle id) const
    {
        return id.IsValid() && mImpl->materials.contains(id.Id());
    }

    void MaterialPool::Destroy(const MaterialHandle id)
    {
        auto& i = *mImpl;
        if (!id.IsValid() || !i.materials.contains(id.Id()))
            return;

        Material cleared {
            .createInfo = {},
            .id         = id.Id(),
        };
        i.writeBindlessMaterial(cleared);
        i.materials.remove(Material { .createInfo = {}, .id = id.Id() });
        i.logger->LogTrace("MaterialPool::Destroy id={}", id.Id());
    }

    void MaterialPool::Impl::writeBindlessMaterial(Material& material)
    {
        const auto& info = material.createInfo;

        auto resolveIndex = [&](const std::optional<TextureHandle>& textureId,
                                const std::uint32_t fallback) -> std::uint32_t {
            if (!textureId || !textureId->IsValid() ||
                !texturePool->Contains(*textureId))
                return fallback;
            return MaterialDescriptorResources::TextureHeapIndex(
                textureId->Id());
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
        gpu.transmission       = info.transmission;
        gpu.ior                = info.ior > 1e-3f ? info.ior : 1.5f;

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
