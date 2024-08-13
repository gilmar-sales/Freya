#pragma once

#include "Core/SwapChain.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;
    class RenderPass;

    class SwapChainBuilder
    {
      public:
        SwapChainBuilder()
            : mPhysicalDevice(nullptr), mSurface(nullptr),
              mSamples(vk::SampleCountFlagBits::e1), mFrameCount(2), mVSync(true),
              mWidth(1280), mHeight(720)
        {
        }

        SwapChainBuilder &SetInstance(Ref<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        SwapChainBuilder &SetPhysicalDevice(Ref<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        SwapChainBuilder &SetPhysicalDevice(
            Ref<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        SwapChainBuilder &SetDevice(Ref<Device> device)
        {
            mDevice = device;
            return *this;
        }

        SwapChainBuilder &SetSurface(Ref<Surface> &surface)
        {
            mSurface = surface;
            return *this;
        }

        SwapChainBuilder &SetRenderPass(Ref<RenderPass> &renderPass)
        {
            mRenderPass = renderPass;
            return *this;
        }
        SwapChainBuilder &SetSamples(vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        SwapChainBuilder &SetWidth(std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        SwapChainBuilder &SetHeight(uint32_t height)
        {
            mHeight = height;
            return *this;
        }
        SwapChainBuilder &SetFrameCount(uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        SwapChainBuilder &SetVSync(bool vSync)
        {
            mVSync = vSync;
            return *this;
        }

        Ref<SwapChain> Build();

      protected:
        vk::PresentModeKHR choosePresentMode();

      private:
        Ref<Instance> mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device> mDevice;
        Ref<Surface> mSurface;
        Ref<RenderPass> mRenderPass;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mWidth;
        std::uint32_t mHeight;
        std::uint32_t mFrameCount;
        bool mVSync;
    };

} // namespace FREYA_NAMESPACE
