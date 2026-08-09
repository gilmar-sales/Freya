#pragma once

/**
 * @file Freya.hpp
 * @brief Application-facing Freya umbrella.
 *
 * Prefer this header for scene setup (options, pools, AbstractApplication).
 * For Renderer, passes, and other Vulkan-heavy types, include
 * <Freya/Vulkan.hpp> (or individual headers under Freya/Core/).
 */

#include "Freya/Core/AbstractApplication.hpp"
#include "Freya/Core/FreyaExtension.hpp"
#include "Freya/Core/RenderTarget.hpp"
#include "Freya/FreyaOptions.hpp"

#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Builders/WindowBuilder.hpp"

#include "Freya/Asset/AnimGraph.hpp"
#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Rig.hpp"
#include "Freya/Asset/Skeleton.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Asset/Vertex.hpp"

#include "Freya/Events/EventManager.hpp"
#include "Freya/Events/Events.hpp"
