#pragma once

#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ForwardPassBuilder.hpp"
#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/TexturePool.hpp"

#include "Freya/Core/AbstractApplication.hpp"

namespace FREYA_NAMESPACE
{
    template <typename T>
    concept IsApplication = std::is_base_of_v<AbstractApplication, T>;

    class ApplicationBuilder
    {
      public:
        ApplicationBuilder() :
            mServiceCollection(std::make_shared<skr::ServiceCollection>())
        {
            mFreyaOptionsBuilderFunc = [](FreyaOptionsBuilder& freyaOptions) {};
        }

        [[nodiscard]] Ref<skr::ServiceCollection> GetServiceCollection() const
        {
            return mServiceCollection;
        }

        ApplicationBuilder& WithOptions(
            std::function<void(FreyaOptionsBuilder&)> freyaOptionsBuilderFunc);

        template <typename T>
            requires IsApplication<T>
        Ref<T> Build()
        {
            mServiceCollection->AddSingleton<T>();

            AddServices();

            const auto serviceProvider =
                mServiceCollection->CreateServiceProvider();

            return serviceProvider->GetService<T>();
        }

      protected:
        void AddServices();

        std::function<void(FreyaOptionsBuilder&)> mFreyaOptionsBuilderFunc;

        Ref<skr::ServiceCollection> mServiceCollection;
    };
} // namespace FREYA_NAMESPACE
