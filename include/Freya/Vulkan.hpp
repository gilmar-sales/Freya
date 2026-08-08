#pragma once

/**
 * @file Vulkan.hpp
 * @brief Opt-in umbrella for Vulkan-facing Freya types.
 *
 * Includes Renderer, frame stages, device/swapchain builders, and passes.
 * App code that only configures options and submits draws can use
 * <Freya/Freya.hpp> alone.
 */

#include "Freya/Freya.hpp"

#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DebugLabels.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/FrameStages.hpp"
#include "Freya/Core/HiZPyramid.hpp"
#include "Freya/Core/IBLService.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/RenderFrameContext.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/SsaoPass.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/TaaPass.hpp"
#include "Freya/Core/Window.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/InstanceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RendererBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"
