#pragma once

#include "Freya/Asset/LODService.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    class LODPool;
    class MeshPool;
    class Renderer;

    /**
     * @brief Builder for LODService
     *
     * Follows the same builder pattern as other Freya services.
     */
    class LODBuilder
    {
      public:
        LODBuilder(const Ref<skr::ServiceProvider>& serviceProvider) :
            mServiceProvider(serviceProvider),
            mLogger(serviceProvider->GetService<skr::Logger<LODBuilder>>())
        {
        }

        Ref<LODService> Build() const
        {
            mLogger->LogInformation(
                "Building LOD service with {} max instances", mMaxInstances);

            auto service = MakeRef<LODService>(mServiceProvider);

            return service;
        }

      private:
        Ref<skr::ServiceProvider>    mServiceProvider;
        Ref<skr::Logger<LODBuilder>> mLogger;

        std::uint32_t mMaxInstances = 65536;
    };

} // namespace FREYA_NAMESPACE