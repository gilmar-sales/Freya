#pragma once

#include "Freya/Core/SwapChain.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;
    class ForwardPass;

    class SwapChainBuilder
    {
      public:
        SwapChainBuilder(const Ref<skr::Logger>&          logger,
                         const Ref<skr::ServiceProvider>& serviceProvider) :
            mLogger(logger), mServiceProvider(serviceProvider),
            mPhysicalDevice(nullptr), mSurface(nullptr),
            mSamples(vk::SampleCountFlagBits::e1), mWidth(1280), mHeight(720),
            mFrameCount(2), mVSync(true)
        {
        }

        SwapChainBuilder& SetInstance(const Ref<Instance>& instance)
        {
            mInstance = instance;
            return *this;
        }

        SwapChainBuilder& SetPhysicalDevice(const Ref<Instance>& instance)
        {
            mInstance = instance;
            return *this;
        }

        SwapChainBuilder& SetPhysicalDevice(
            const Ref<PhysicalDevice>& physicalDevice)
        {
            mPhysicalDevice = physicalDevice;
            return *this;
        }

        SwapChainBuilder& SetDevice(const Ref<Device>& device)
        {
            mDevice = device;
            return *this;
        }

        SwapChainBuilder& SetSurface(const Ref<Surface>& surface)
        {
            mSurface = surface;
            return *this;
        }

        SwapChainBuilder& SetRenderPass(const Ref<ForwardPass>& renderPass)
        {
            mRenderPass = renderPass;
            return *this;
        }

        SwapChainBuilder& SetSamples(const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        SwapChainBuilder& SetWidth(const std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        SwapChainBuilder& SetHeight(const uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        SwapChainBuilder& SetFrameCount(const uint32_t frameCount)
        {
            mFrameCount = frameCount;
            return *this;
        }

        SwapChainBuilder& SetVSync(const bool vSync)
        {
            mVSync = vSync;
            return *this;
        }

        Ref<SwapChain> Build();

      protected:
        vk::PresentModeKHR choosePresentMode();

      private:
        Ref<skr::Logger>          mLogger;
        Ref<skr::ServiceProvider> mServiceProvider;

        Ref<Instance>       mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device>         mDevice;
        Ref<Surface>        mSurface;
        Ref<ForwardPass>    mRenderPass;

        vk::SampleCountFlagBits mSamples;

        std::uint32_t mWidth;
        std::uint32_t mHeight;
        std::uint32_t mFrameCount;
        bool          mVSync;
    };

} // namespace FREYA_NAMESPACE
