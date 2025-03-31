#pragma once

#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/ForwardPass.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Events/EventManager.hpp"

namespace FREYA_NAMESPACE
{
    class Buffer;

    class Renderer
    {
      public:
        Renderer(const Ref<Instance>&             instance,
                 const Ref<Surface>&              surface,
                 const Ref<PhysicalDevice>&       physicalDevice,
                 const Ref<Device>&               device,
                 const Ref<SwapChain>&            swapChain,
                 const Ref<ForwardPass>&          renderPass,
                 const Ref<CommandPool>&          commandPool,
                 const Ref<skr::ServiceProvider>& serviceProvider,
                 const Ref<FreyaOptions>&         freyaOptions,
                 const Ref<EventManager>&         eventManager) :
            mInstance(instance), mSurface(surface),
            mPhysicalDevice(physicalDevice), mDevice(device),
            mSwapChain(swapChain), mRenderPass(renderPass),
            mCommandPool(commandPool), mServiceProvider(serviceProvider),
            mFreyaOptions(freyaOptions), mEventManager(eventManager),
            mCurrentProjection({})
        {
            ClearProjections();

            mEventManager->Subscribe<WindowResizeEvent>(
                [this](WindowResizeEvent event) {
                    if (!event.handled)
                    {
                        mResizeEvent = event;
                    }
                });
        }

        ~Renderer();

        void BeginFrame();

        void EndFrame();

        void               RebuildSwapChain();
        [[nodiscard]] bool GetVSync() const { return mFreyaOptions->vSync; }
        void               SetVSync(bool vSync);
        void               SetSamples(vk::SampleCountFlagBits samples);

        [[nodiscard]] vk::SampleCountFlagBits GetSamples() const
        {
            return mFreyaOptions->sampleCount;
        }

        [[nodiscard]] float GetDrawDistance() const
        {
            return mFreyaOptions->drawDistance;
        }

        void      SetDrawDistance(float drawDistance);
        void      ClearProjections();
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        [[nodiscard]] const ProjectionUniformBuffer& GetCurrentProjection()
            const
        {
            return mCurrentProjection;
        }
        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);
        void UpdateModel(const glm::mat4& model) const;

        [[nodiscard]] BufferBuilder GetBufferBuilder() const;

        void BindBuffer(const Ref<Buffer>& buffer) const;

      private:
        Ref<skr::ServiceProvider> mServiceProvider;
        Ref<Instance>             mInstance;
        Ref<Surface>              mSurface;
        Ref<PhysicalDevice>       mPhysicalDevice;
        Ref<Device>               mDevice;
        Ref<SwapChain>            mSwapChain;
        Ref<ForwardPass>          mRenderPass;
        Ref<CommandPool>          mCommandPool;
        Ref<EventManager>         mEventManager;
        Ref<FreyaOptions>         mFreyaOptions;

        std::optional<WindowResizeEvent> mResizeEvent;

        ProjectionUniformBuffer mCurrentProjection;
    };

} // namespace FREYA_NAMESPACE
