#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class BoneMatrixResourcesBuilder
    {
      public:
        BoneMatrixResourcesBuilder(
            const skr::Arc<Device>&       device,
            const skr::Arc<FreyaOptions>& freyaOptions) :
            mDevice(device), mFreyaOptions(freyaOptions)
        {
        }

        skr::Arc<BoneMatrixResources> Build()
        {
            return skr::MakeArc<BoneMatrixResources>(
                mDevice, mFreyaOptions->frameCount,
                BoneMatrixResources::kDefaultCapacity);
        }

      private:
        skr::Arc<Device>       mDevice;
        skr::Arc<FreyaOptions> mFreyaOptions;
    };

} // namespace FREYA_NAMESPACE
