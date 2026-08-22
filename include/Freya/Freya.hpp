#pragma once

/**
 * @file Freya.hpp
 * @brief Application-facing Freya umbrella.
 *
 * Scene setup, options, pools, events, and AbstractApplication.
 * Headers in this tree do not include Vulkan or SDL.
 */

#include "Freya/Config.hpp"
#include "Freya/Core/Limits.hpp"

#include "Freya/Core/AbstractApplication.hpp"
#include "Freya/Core/FreyaExtension.hpp"
#include "Freya/FreyaOptions.hpp"

#include "Freya/Builders/FreyaOptionsBuilder.hpp"

#include "Freya/Asset/AnimDebug.hpp"
#include "Freya/Asset/AnimGraph.hpp"
#include "Freya/Asset/AnimGraphDebug.hpp"
#include "Freya/Asset/AnimationClip.hpp"
#include "Freya/Asset/BakedAnimation.hpp"
#include "Freya/Asset/FontAtlas.hpp"
#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MaterialTechniqueRegistry.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/Pose.hpp"
#include "Freya/Asset/Rig.hpp"
#include "Freya/Asset/Skeleton.hpp"
#include "Freya/Asset/SkinnedModel.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Asset/Vertex.hpp"

#include "Freya/Core/BillboardDraw.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/ParticleEmitter.hpp"
#include "Freya/Core/PostProcess.hpp"

#include "Freya/Builders/PostProcessBuilder.hpp"

#include "Freya/Events/EventManager.hpp"
#include "Freya/Events/Events.hpp"
