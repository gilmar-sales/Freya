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
        Ref<LODService> Build() const
        {
            mLogger->LogInformation(
                "Building LOD service with {} max instances", mMaxInstances);

            auto service = MakeRef<LODService>(
                mDevice, mPhysicalDevice, mCommandPool, mLODPool, mMeshPool,
                mRenderer, mFreyaOptions, mServiceProvider);

            return service;
        }

        LODBuilder& SetMaxInstances(std::uint32_t maxInstances)
        {
            mMaxInstances = maxInstances;
            return *this;
        }

        void SetDevice(const Ref<Device>& device) { mDevice = device; }
        void SetPhysicalDevice(const Ref<PhysicalDevice>& pd)
        {
            mPhysicalDevice = pd;
        }
        void SetCommandPool(const Ref<CommandPool>& cp) { mCommandPool = cp; }
        void SetLODPool(const Ref<LODPool>& lp) { mLODPool = lp; }
        void SetMeshPool(const Ref<MeshPool>& mp) { mMeshPool = mp; }
        void SetRenderer(const Ref<Renderer>& r) { mRenderer = r; }
        void SetFreyaOptions(const Ref<FreyaOptions>& fo)
        {
            mFreyaOptions = fo;
        }
        void SetServiceProvider(const Ref<skr::ServiceProvider>& sp)
        {
            mServiceProvider = sp;
        }

      private:
        Ref<Device>                  mDevice;
        Ref<PhysicalDevice>          mPhysicalDevice;
        Ref<CommandPool>             mCommandPool;
        Ref<LODPool>                 mLODPool;
        Ref<MeshPool>                mMeshPool;
        Ref<Renderer>                mRenderer;
        Ref<FreyaOptions>            mFreyaOptions;
        Ref<skr::ServiceProvider>    mServiceProvider;
        Ref<skr::Logger<LODBuilder>> mLogger;

        std::uint32_t mMaxInstances = 65536;
    };

} // namespace FREYA_NAMESPACE