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
        explicit PhysicalDevice(const vk::PhysicalDevice physicalDevice) :
            mPhysicalDevice(physicalDevice)
        {
        }

        operator bool() const { return mPhysicalDevice; }

        vk::PhysicalDevice& Get() { return mPhysicalDevice; }

        SwapChainSupportDetails QuerySwapChainSupport(
            vk::SurfaceKHR surface) const;

        std::uint32_t QueryCompatibleMemoryType(
            std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

        std::uint32_t QuerySamplesSupport(std::uint32_t desired) const;

        vk::Format GetDepthFormat() const;

        std::vector<const char*> FilterSupportedExtensions(
            std::vector<const char*> requestedExtensions) const;

      private:
        vk::PhysicalDevice mPhysicalDevice;
    };

} // namespace FREYA_NAMESPACE
