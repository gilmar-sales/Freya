#include "Device.hpp"

#include <vulkan/vulkan.h>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr std::array<float, 4> kNeutralLabelColor { 0.70f, 0.70f, 0.70f,
                                                            1.0f };
    } // namespace

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
        BeginDebugLabel(cmd, name, kNeutralLabelColor);
    }

    void Device::BeginDebugLabel(const vk::CommandBuffer&    cmd,
                                 const char*                 name,
                                 const std::array<float, 4>& color) const
    {
#ifndef NDEBUG
        if (!mCmdBeginDebugLabel || name == nullptr)
            return;

        VkDebugUtilsLabelEXT label {};
        label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        label.color[0]   = color[0];
        label.color[1]   = color[1];
        label.color[2]   = color[2];
        label.color[3]   = color[3];
        mCmdBeginDebugLabel(static_cast<VkCommandBuffer>(cmd), &label);
#else
        (void) cmd;
        (void) name;
        (void) color;
#endif
    }

    void Device::BeginDebugLabel(const vk::CommandBuffer& cmd,
                                 const DebugRegion&       region) const
    {
        BeginDebugLabel(cmd, region.name, region.color);
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

    void Device::SubmitAndWait(vk::Queue queue, vk::SubmitInfo submitInfo) const
    {
        vk::Fence fence = mDevice.createFence({});
        queue.submit(submitInfo, fence);
        (void) mDevice.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);
        mDevice.destroyFence(fence);
    }
} // namespace FREYA_NAMESPACE
