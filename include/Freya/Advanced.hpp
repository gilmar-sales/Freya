#pragma once

/**
 * @file Advanced.hpp
 * @brief Advanced / plugin Freya surface (tools, frame stages, RHI escapes).
 *
 * Prefer <Freya/Freya.hpp> for application authors. Include this header when
 * authoring IFrameStage plugins, post-process chains, technique overrides,
 * cull-frame dumps, platform observers, or GPU-animation tooling.
 */

#include "Freya/Freya.hpp"

#include "Freya/Core/RendererAdvanced.hpp"

#include "Freya/Core/GpuAnimationSystem.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/IPlatform.hpp"
#include "Freya/Core/PostProcess.hpp"
#include "Freya/Core/RendererUi.hpp"
#include "Freya/Core/StageContext.hpp"

#include "Freya/Builders/PostProcessBuilder.hpp"

#include "Freya/Asset/AnimDebug.hpp"
#include "Freya/Asset/AnimGraphDebug.hpp"
#include "Freya/Asset/CullFrameDump.hpp"
#include "Freya/Asset/GpuAnimDebug.hpp"
#include "Freya/Asset/GpuAnimation.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/LightingTechniqueRegistry.hpp"
#include "Freya/Asset/MaterialTechniqueRegistry.hpp"
#include "Freya/Asset/SceneInstanceUpload.hpp"
