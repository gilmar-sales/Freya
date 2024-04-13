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

        SwapChainBuilder &SetInstance(std::shared_ptr<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        SwapChainBuilder &SetPhysicalDevice(std::shared_ptr<Instance> instance)
        {
            mInstance = instance;
            return *this;
        }

        SwapChainBuilder &SetPhysicalDevice(
            std::shared_ptr<PhysicalDevice> physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        SwapChainBuilder &SetDevice(std::shared_ptr<Device> device)
        {
            mDevice = device;
            return *this;
        }

        SwapChainBuilder &SetSurface(std::shared_ptr<Surface> &surface)
        {
            mSurface = surface;
            return *this;
        }

        SwapChainBuilder &SetRenderPass(std::shared_ptr<RenderPass> &renderPass)
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

        std::shared_ptr<SwapChain> Build();

      protected:
        vk::PresentModeKHR choosePresentMode();

      private:
        std::shared_ptr<Instance> mInstance;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;
        std::shared_ptr<Device> mDevice;
        std::shared_ptr<Surface> mSurface;
        std::shared_ptr<RenderPass> mRenderPass;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mWidth;
        std::uint32_t mHeight;
        std::uint32_t mFrameCount;
        bool mVSync;
    };

} // namespace FREYA_NAMESPACE
