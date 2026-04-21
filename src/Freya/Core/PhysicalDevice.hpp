#pragma once

namespace FREYA_NAMESPACE
{
    /**
     * @brief Swap chain support details queried from physical device.
     */
    struct SwapChainSupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities; ///< Surface capabilities
        std::vector<vk::SurfaceFormatKHR>
            formats; ///< Available surface formats
        std::vector<vk::PresentModeKHR>
            presentModes; ///< Available present modes
    };

    /**
     * @brief Queries physical device capabilities and memory properties.
     *
     * Wrapper around vk::PhysicalDevice providing query methods for swap chain
     * support, memory type allocation, sample count support, and extension
     * filtering.
     *
     * @param physicalDevice Raw Vulkan physical device handle
     */
    class PhysicalDevice
    {
      public:
        explicit PhysicalDevice(const vk::PhysicalDevice physicalDevice) :
            mPhysicalDevice(physicalDevice)
        {
        }

        /**
         * @brief Conversion operator to check if physical device is valid.
         */
        operator bool() const { return mPhysicalDevice; }

        /**
         * @brief Returns the underlying physical device handle.
         */
        vk::PhysicalDevice& Get() { return mPhysicalDevice; }

        /**
         * @brief Queries swap chain support details for a given surface.
         * @param surface Vulkan surface to query support for
         * @return SwapChainSupportDetails containing capabilities, formats, and
         * present modes
         */
        SwapChainSupportDetails QuerySwapChainSupport(
            vk::SurfaceKHR surface) const;

        /**
         * @brief Finds a compatible memory type matching the filter and
         * property flags.
         * @param typeFilter Memory type bitfield filter
         * @param properties Required memory property flags
         * @return Memory type index suitable for allocation
         * @note Asserts if no suitable memory type is found
         */
        std::uint32_t QueryCompatibleMemoryType(
            std::uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

        /**
         * @brief Queries the device's supported sample count for a desired
         * value.
         * @param desired Desired sample count (as vk::SampleCountFlagBits
         * value)
         * @return The actual supported sample count, clamped to device limits
         */
        std::uint32_t QuerySamplesSupport(std::uint32_t desired) const;

        /**
         * @brief Returns the best supported depth format from a candidate list.
         * @return Preferred depth format (defaults to eD32Sfloat)
         */
        vk::Format GetDepthFormat() const;

        /**
         * @brief Filters requested extensions to only those supported by this
         * device.
         * @param requestedExtensions List of extension names to check
         * @return Filtered list containing only supported extensions
         */
        std::vector<const char*> FilterSupportedExtensions(
            std::vector<const char*> requestedExtensions) const;

      private:
        vk::PhysicalDevice mPhysicalDevice;
    };

} // namespace FREYA_NAMESPACE
