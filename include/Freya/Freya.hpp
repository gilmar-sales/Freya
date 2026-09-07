#pragma once

/**
 * @file Freya.hpp
 * @brief Application-facing Freya umbrella (app tier).
 *
 * Scene setup, options, pools, events, and AbstractApplication.
 * Headers in this tree do not include Vulkan or SDL.
 *
 * For frame-stage plugins, post-process, technique registries, cull dumps,
 * platform natives, and GPU-anim debug, include <Freya/Advanced.hpp>.
 */

#include "Freya/Config.hpp"
#include "Freya/Core/Limits.hpp"

#include "Freya/Core/AbstractApplication.hpp"
#include "Freya/Core/FreyaApp.hpp"
#include "Freya/Core/FreyaExtension.hpp"
#include "Freya/FreyaOptions.hpp"

#include "Freya/Builders/FreyaOptionsBuilder.hpp"

#include "Freya/Asset/AnimGraph.hpp"
#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/FontAtlas.hpp"
#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Rig.hpp"
#include "Freya/Asset/Skeleton.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Asset/Vertex.hpp"

#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/FrameGpuTiming.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/ParticleEmitter.hpp"

#include "Freya/Scene/AssetHandle.hpp"
#include "Freya/Scene/Camera.hpp"
#include "Freya/Scene/Scene.hpp"

#include "Freya/Events/EventManager.hpp"
#include "Freya/Events/Events.hpp"
