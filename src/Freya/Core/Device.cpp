#include "Device.hpp"

#include <vulkan/vulkan.h>

namespace FREYA_NAMESPACE
{
    Device::Device(const skr::Arc<PhysicalDevice>& physicalDevice,
                   const vk::Device                device,
                   const vk::Queue                 graphicsQueue,
                   const vk::Queue                 presentQueue,
                   const vk::Queue                 transferQueue,
                   const QueueFamilyIndices&       queueFamilyIndices) :
        mPhysicalDevice(physicalDevice), mDevice(device),
        mGraphicsQueue(graphicsQueue), mPresentQueue(presentQueue),
        mTransferQueue(transferQueue), mQueueFamilyIndices(queueFamilyIndices)
    {
#ifndef NDEBUG
        mCmdBeginDebugLabel =
            reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                mDevice.getProcAddr("vkCmdBeginDebugUtilsLabelEXT"));
        mCmdEndDebugLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            mDevice.getProcAddr("vkCmdEndDebugUtilsLabelEXT"));
#endif
    }

    void Device::BeginDebugLabel(const vk::CommandBuffer& cmd,
                                 const char*              name) const
    {
#ifndef NDEBUG
        if (!mCmdBeginDebugLabel || name == nullptr)
            return;

        VkDebugUtilsLabelEXT label {};
        label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        mCmdBeginDebugLabel(static_cast<VkCommandBuffer>(cmd), &label);
#else
        (void) cmd;
        (void) name;
#endif
    }

    void Device::EndDebugLabel(const vk::CommandBuffer& cmd) const
    {
#ifndef NDEBUG
        if (!mCmdEndDebugLabel)
            return;

        mCmdEndDebugLabel(static_cast<VkCommandBuffer>(cmd));
#else
        (void) cmd;
#endif
    }
} // namespace FREYA_NAMESPACE
