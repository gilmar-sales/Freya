#pragma once

#include "Freya/Core/CommandPool.hpp"

#include <Skirnir/Skirnir.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Process-wide command pool for one-shot GPU uploads.
     *
     * Distinct from the per-window frame CommandPool so MeshPool / TexturePool
     * (singletons) do not capture a scoped frame pool.
     */
    class TransferCommandPool
    {
      public:
        explicit TransferCommandPool(skr::Arc<CommandPool> pool) :
            mPool(std::move(pool))
        {
        }

        [[nodiscard]] const skr::Arc<CommandPool>& GetPool() const
        {
            return mPool;
        }

        vk::CommandPool& Get() { return mPool->Get(); }

        [[nodiscard]] const skr::Arc<Device>& GetDevice() const
        {
            return mPool->GetDevice();
        }

        [[nodiscard]] vk::CommandBuffer CreateCommandBuffer() const
        {
            return mPool->CreateCommandBuffer();
        }

        void FreeCommandBuffer(const vk::CommandBuffer commandBuffer) const
        {
            mPool->FreeCommandBuffer(commandBuffer);
        }

      private:
        skr::Arc<CommandPool> mPool;
    };

} // namespace FREYA_NAMESPACE
