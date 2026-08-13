#pragma once

#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"

#include <functional>
#include <optional>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Extension interface for Freya engine service registration.
     *
     * Implements skr::IExtension to register all Freya services
     * in the service provider. Use WithOptions() / WithInstance() / etc.
     * to configure builders before ConfigureServices() runs.
     */
    class FreyaExtension : public skr::IExtension
    {
      public:
        /**
         * @brief Fluent API to configure FreyaOptions.
         */
        FreyaExtension& WithOptions(
            const std::function<void(FreyaOptionsBuilder&)>&
                freyaOptionsBuilderFunc)
        {
            freyaOptionsBuilderFunc(mFreyaOptionsBuilder);
            return *this;
        }

        FreyaExtension& WithInstance(
            const std::function<void(InstanceBuilder&)>& configure)
        {
            mConfigureInstance = configure;
            return *this;
        }

        FreyaExtension& WithDevice(
            const std::function<void(DeviceBuilder&)>& configure)
        {
            mConfigureDevice = configure;
            return *this;
        }

        FreyaExtension& WithPhysicalDevice(
            const std::function<void(PhysicalDeviceBuilder&)>& configure)
        {
            mConfigurePhysicalDevice = configure;
            return *this;
        }

        FreyaExtension& WithSwapChain(
            const std::function<void(SwapChainBuilder&)>& configure)
        {
            mConfigureSwapChain = configure;
            return *this;
        }

        FreyaExtension& WithRenderer(
            const std::function<void(RendererBuilder&)>& configure)
        {
            mConfigureRenderer = configure;
            return *this;
        }

      protected:
        void ConfigureServices(skr::ServiceCollection& services) override;

      private:
        FreyaOptionsBuilder mFreyaOptionsBuilder;

        std::optional<std::function<void(InstanceBuilder&)>> mConfigureInstance;
        std::optional<std::function<void(DeviceBuilder&)>>   mConfigureDevice;
        std::optional<std::function<void(PhysicalDeviceBuilder&)>>
            mConfigurePhysicalDevice;
        std::optional<std::function<void(SwapChainBuilder&)>>
            mConfigureSwapChain;
        std::optional<std::function<void(RendererBuilder&)>> mConfigureRenderer;
    };
} // namespace FREYA_NAMESPACE
