#include "Freya/Asset/TexturePool.hpp"

#ifndef NDEBUG
    #undef __OPTIMIZE__
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "Freya/Vendor/stb_image.h"

#ifndef NDEBUG
    #define __OPTIMIZE__ 1
#endif

#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/Texture.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Containers/SparseSet.hpp" // src/Freya/Containers (internal)
#include "Freya/Core/Device.hpp"
#include "Freya/Core/SpinLock.hpp"
#include "Freya/Core/TransferCommandPool.hpp"

#include <vulkan/vulkan.hpp>

namespace FREYA_NAMESPACE
{
    constexpr auto MegaBytes = 1024 * 1024;

    struct TexturePool::Impl
    {
        skr::Arc<skr::Logger<TexturePool>>    logger;
        skr::Arc<skr::ServiceProvider>        serviceProvider;
        skr::Arc<Device>                      device;
        skr::Arc<TransferCommandPool>         transferPool;
        skr::Arc<MaterialDescriptorResources> materialsRes;
        std::vector<skr::Arc<Buffer>>         stagingBuffers;
        SparseSet<Texture>                    textures { 4096 };
        mutable SpinLock                      lock;

        skr::Arc<Buffer> queryStagingBuffer(std::uint32_t size);
        skr::Arc<Buffer> createStagingBuffer(std::uint32_t size);
    };

    TexturePool::TexturePool(
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mImpl(std::make_unique<Impl>())
    {
        mImpl->serviceProvider = serviceProvider;
        mImpl->device          = serviceProvider->GetService<Device>();
        mImpl->transferPool =
            serviceProvider->GetService<TransferCommandPool>();
        mImpl->materialsRes =
            serviceProvider->GetService<MaterialDescriptorResources>();
        mImpl->logger = serviceProvider->GetService<skr::Logger<TexturePool>>();
        stbi_set_flip_vertically_on_load(true);
    }

    TexturePool::~TexturePool()
    {
        if (!mImpl || !mImpl->device)
            return;

        mImpl->device->Get().waitIdle();
        for (auto texture : mImpl->textures)
        {
            texture.image.reset();
            mImpl->device->Get().destroySampler(texture.sampler);
        }
    }

    std::optional<TextureHandle> TexturePool::CreateTextureFromFile(
        std::string path)
    {
        mImpl->logger->LogTrace("TexturePool::CreateTextureFromFile:");
        mImpl->logger->LogTrace("\tPath: {}", path);

        int        width, height, channels;
        const auto imageData =
            stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!imageData)
        {
            mImpl->logger->LogWarning("Failed to load texture: {}", path);
            return std::nullopt;
        }

        const auto id = CreateTextureFromMemory(
            imageData, static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height), STBI_rgb_alpha);

        stbi_image_free(imageData);
        return id;
    }

    TextureHandle TexturePool::CreateTextureFromMemory(
        const void* pixels, std::uint32_t width, std::uint32_t height,
        std::uint32_t channels, std::uint32_t mipLevelCount)
    {
        SpinLockGuard guard(mImpl->lock);
        auto&         i = *mImpl;
        i.logger->LogTrace("TexturePool::CreateTextureFromMemory:");
        i.logger->LogTrace("\tSize: {}x{} channels={}", width, height,
                           channels);
        i.logger->Assert(pixels != nullptr, "\tNull pixel data.");
        i.logger->Assert(width > 0 && height > 0, "\tInvalid dimensions.");
        i.logger->Assert(channels > 0, "\tInvalid channel count.");

        const auto stagingBuffer =
            i.queryStagingBuffer(width * height * channels);

        auto builder =
            i.serviceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Texture)
                .SetWidth(width)
                .SetHeight(height)
                .SetChannels(channels)
                .SetStagingBuffer(stagingBuffer)
                .SetData(const_cast<void*>(pixels));
        if (mipLevelCount > 0)
            builder.SetMipLevels(mipLevelCount);

        const auto image     = builder.Build();
        const auto mipLevels = image->GetMipLevels();

        const auto samplerCreateInfo =
            vk::SamplerCreateInfo()
                .setMagFilter(vk::Filter::eLinear)
                .setMinFilter(vk::Filter::eLinear)
                .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                .setBorderColor(vk::BorderColor::eIntOpaqueBlack)
                .setUnnormalizedCoordinates(false)
                .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                .setMipLodBias(0.0f)
                .setMinLod(0.0f)
                .setMaxLod(static_cast<float>(mipLevels))
                .setAnisotropyEnable(true)
                .setMaxAnisotropy(16);

        const auto sampler = i.device->Get().createSampler(samplerCreateInfo);

        const auto texture = Texture {
            .image        = image,
            .sampler      = sampler,
            .width        = width,
            .height       = height,
            .id           = static_cast<std::uint32_t>(i.textures.size()),
            .external     = false,
            .externalView = {},
        };

        i.textures.insert(texture);

        i.materialsRes->WriteBindlessTexture(
            MaterialDescriptorResources::TextureHeapIndex(texture.id),
            texture.image->GetImageView(), texture.sampler);

        return TextureHandle { texture.id };
    }

    TextureHandle TexturePool::RegisterExternalImage(void*         imageView,
                                                     void*         sampler,
                                                     std::uint32_t width,
                                                     std::uint32_t height)
    {
        SpinLockGuard guard(mImpl->lock);
        auto&         i = *mImpl;
        i.logger->Assert(imageView != nullptr && sampler != nullptr,
                         "RegisterExternalImage null view/sampler");
        i.logger->Assert(width > 0 && height > 0, "Invalid dimensions");

        const auto view = static_cast<vk::ImageView>(
            reinterpret_cast<VkImageView>(imageView));
        const auto samp =
            static_cast<vk::Sampler>(reinterpret_cast<VkSampler>(sampler));

        const auto texture = Texture {
            .image        = {},
            .sampler      = samp,
            .width        = width,
            .height       = height,
            .id           = static_cast<std::uint32_t>(i.textures.size()),
            .external     = true,
            .externalView = view,
        };

        i.textures.insert(texture);
        i.materialsRes->WriteBindlessTexture(
            MaterialDescriptorResources::TextureHeapIndex(texture.id), view,
            samp);
        i.logger->LogTrace("TexturePool::RegisterExternalImage id={} {}x{}",
                           texture.id, width, height);
        return TextureHandle { texture.id };
    }

    void TexturePool::UnregisterExternal(const TextureHandle id)
    {
        SpinLockGuard guard(mImpl->lock);
        auto&         i = *mImpl;
        if (!id.IsValid() || !i.textures.contains(id.Id()))
            return;
        auto& texture = i.textures[id.Id()];
        if (!texture.external)
            return;

        i.materialsRes->WriteBindlessTexture(
            MaterialDescriptorResources::TextureHeapIndex(id.Id()),
            i.materialsRes->GetFallbackImageView(),
            i.materialsRes->GetFallbackSampler());

        i.textures.remove(Texture {
            .image        = {},
            .sampler      = {},
            .width        = 0,
            .height       = 0,
            .id           = id.Id(),
            .external     = true,
            .externalView = {},
        });
        i.logger->LogTrace("TexturePool::UnregisterExternal id={}", id.Id());
    }

    skr::Arc<Buffer> TexturePool::Impl::queryStagingBuffer(std::uint32_t size)
    {
        for (auto stagingBuffer : stagingBuffers)
        {
            if (stagingBuffer->GetSize() >= size)
                return stagingBuffer;
        }

        return createStagingBuffer(size);
    }

    skr::Arc<Buffer> TexturePool::Impl::createStagingBuffer(std::uint32_t size)
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

    bool TexturePool::Contains(const TextureHandle id) const
    {
        SpinLockGuard guard(mImpl->lock);
        return id.IsValid() && mImpl->textures.contains(id.Id());
    }

    void TexturePool::Destroy(const TextureHandle id)
    {
        SpinLockGuard guard(mImpl->lock);
        auto&         i = *mImpl;
        if (!id.IsValid() || !i.textures.contains(id.Id()))
            return;

        auto& texture = i.textures[id.Id()];

        i.materialsRes->WriteBindlessTexture(
            MaterialDescriptorResources::TextureHeapIndex(id.Id()),
            i.materialsRes->GetFallbackImageView(),
            i.materialsRes->GetFallbackSampler());

        if (texture.external)
        {
            i.textures.remove(Texture {
                .image        = {},
                .sampler      = {},
                .width        = 0,
                .height       = 0,
                .id           = id.Id(),
                .external     = true,
                .externalView = {},
            });
            i.logger->LogTrace("TexturePool::Destroy external id={}", id.Id());
            return;
        }

        i.device->Get().waitIdle();

        texture.image.reset();
        i.device->Get().destroySampler(texture.sampler);

        i.textures.remove(Texture {
            .image        = {},
            .sampler      = {},
            .width        = 0,
            .height       = 0,
            .id           = id.Id(),
            .external     = false,
            .externalView = {},
        });
        i.logger->LogTrace("TexturePool::Destroy id={}", id.Id());
    }
} // namespace FREYA_NAMESPACE
