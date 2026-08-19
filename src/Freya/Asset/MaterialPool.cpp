#include "MaterialPool.hpp"

#include "Freya/Asset/GpuScene.hpp"

namespace FREYA_NAMESPACE
{
    std::uint32_t MaterialPool::CreateFromTextureFiles(
        std::vector<std::string> texturesPath)
    {
        MaterialCreateInfo info {};
        if (texturesPath.size() > 0)
            info.albedo = mTexturePool->CreateTextureFromFile(texturesPath[0]);
        if (texturesPath.size() > 1)
            info.normal = mTexturePool->CreateTextureFromFile(texturesPath[1]);
        if (texturesPath.size() > 2)
            info.roughness =
                mTexturePool->CreateTextureFromFile(texturesPath[2]);
        if (texturesPath.size() > 3)
            info.emissive =
                mTexturePool->CreateTextureFromFile(texturesPath[3]);
        if (texturesPath.size() > 4)
            info.metalness =
                mTexturePool->CreateTextureFromFile(texturesPath[4]);
        return Create(info);
    }

    std::uint32_t MaterialPool::Create(const MaterialCreateInfo& createInfo)
    {
        auto material = Material {
            .createInfo = createInfo,
            .id         = static_cast<std::uint32_t>(mMaterials.size()),
        };

        writeBindlessMaterial(material);
        mMaterials.insert(material);

        mLogger->LogTrace("MaterialPool::Create id={}", material.id);
        return material.id;
    }

    void MaterialPool::Update(std::uint32_t             id,
                              const MaterialCreateInfo& createInfo)
    {
        auto& material      = mMaterials[id];
        material.createInfo = createInfo;
        writeBindlessMaterial(material);
    }

    const MaterialCreateInfo& MaterialPool::GetCreateInfo(
        std::uint32_t id) const
    {
        return mMaterials[id].createInfo;
    }

    void MaterialPool::writeBindlessMaterial(Material& material)
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

        mMaterialsRes->WriteMaterial(material.id, gpu);
    }
} // namespace FREYA_NAMESPACE
