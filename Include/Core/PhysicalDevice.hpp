#pragma once

namespace FREYA_NAMESPACE
{
    struct SwapChainSupportDetails
    {
        vk::SurfaceCapabilitiesKHR        capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR>   presentModes;
    };

    class PhysicalDevice
    {
      public:
        PhysicalDevice(vk::PhysicalDevice physicalDevice) :
            mPhysicalDevice(physicalDevice)
        {
        }

        operator bool() { return mPhysicalDevice; }

        vk::PhysicalDevice&     Get() { return mPhysicalDevice; }
        SwapChainSupportDetails QuerySwapChainSupport(vk::SurfaceKHR surface);
        std::uint32_t           QueryCompatibleMemoryType(std::uint32_t           typeFilter,
                                                          vk::MemoryPropertyFlags properties);
        vk::SampleCountFlagBits QuerySamplesSupport(
            vk::SampleCountFlagBits desired = vk::SampleCountFlagBits::e1);

        vk::Format GetDepthFormat();

      private:
        vk::PhysicalDevice mPhysicalDevice;
    };

} // namespace FREYA_NAMESPACE
