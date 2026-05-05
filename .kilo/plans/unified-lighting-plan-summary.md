# Freya Unified Lighting System - Implementation Plan

## Goal

Implement a unified lighting system that works with both Forward and Deferred rendering paths in the Freya Vulkan engine.

## Instructions

- The lighting system should support Point, Directional, and Spot light types
- Use FreyaOptions to get parameters for the LightService (via IoC)
- The MAX_LIGHTS constant was moved to FreyaOptions as `maxLights` option
- Both Forward and Deferred paths should use the same LightUniformBuffer structure
- Backward compatibility: when no lights are defined, fall back to existing ambient light behavior

## Discoveries

1. The RenderPassBuilder creates the pipeline layout for Forward rendering with only binding 0 (UBO). Added binding 1 for the LightBuffer.
2. The DeferredCompressedPassBuilder creates a separate pipeline layout for fullscreen passes (`fullscreenPipelineLayout`) that only includes the input attachment layout. This needs binding 4 for the LightBuffer.
3. The LightService uses a ring-buffer strategy with one descriptor set per frame.
4. The DeferredCompressedPass constructor needs to accept LightService as a dependency.
5. The validation error "descriptor has never been updated" means we need to update binding 4 in the lighting input descriptor set.

## Accomplished

**Completed:**
1. Created `LightUniformBuffer` struct in `UniformBuffer.hpp` with support for 16 lights
2. Created `LightService.hpp` and `LightService.cpp` for managing lights
3. Created `LightServiceBuilder.hpp` (though IoC approach was changed to direct instantiation)
4. Added `maxLights` option to `FreyaOptions` and `FreyaOptionsBuilder`
5. Registered `LightService` singleton in `FreyaExtension.cpp`
6. Updated `Renderer.hpp/cpp` to include `mLightService` member and call `Update()` in `UpdateCamera()`
7. Updated `RendererBuilder.cpp` to pass LightService to Renderer constructor
8. Updated `Shaders/Forward/Frag.frag` with multi-light support and fallback
9. Updated `Shaders/DeferredCompressed/lighting.frag` with multi-light support and fallback
10. Added light buffer binding (binding 1) to RenderPassBuilder's descriptor set layout
11. Added light buffer binding (binding 4) to DeferredCompressedPassBuilder's input attachment layout
12. Updated descriptor pool sizes to accommodate the new bindings

**In Progress:**
- The deferred lighting pass needs the light buffer descriptor to be updated/bound
- The validation error shows: "the descriptor [Set 0, Binding 4, Index 0, variable "lights"] is being used in draw but has never been updated"

## Relevant files / directories

**Core Files Created:**
- `/home/gilmar/dev/Freya/src/Freya/Core/LightService.hpp`
- `/home/gilmar/dev/Freya/src/Freya/Core/LightService.cpp`

**Files Modified:**
- `/home/gilmar/dev/Freya/src/Freya/Core/UniformBuffer.hpp` - Added LightUniformBuffer struct
- `/home/gilmar/dev/Freya/src/Freya/FreyaOptions.hpp` - Added maxLights option
- `/home/gilmar/dev/Freya/src/Freya/Builders/FreyaOptionsBuilder.hpp` - Added SetMaxLights method
- `/home/gilmar/dev/Freya/src/Freya/Core/FreyaExtension.cpp` - Registered LightService singleton
- `/home/gilmar/dev/Freya/src/Freya/Core/Renderer.hpp` - Added mLightService member
- `/home/gilmar/dev/Freya/src/Freya/Core/Renderer.cpp` - Call LightService::Update() in UpdateCamera()
- `/home/gilmar/dev/Freya/src/Freya/Builders/RendererBuilder.cpp` - Pass LightService to Renderer
- `/home/gilmar/dev/Freya/src/Freya/Builders/RenderPassBuilder.cpp` - Added binding 1 for LightBuffer
- `/home/gilmar/dev/Freya/src/Freya/Builders/DeferredCompressedPassBuilder.cpp` - Added binding 4 for LightBuffer
- `/home/gilmar/dev/Freya/src/Freya/Core/DeferredCompressedPass.hpp` - Added LightService include, added members
- `/home/gilmar/dev/Freya/Shaders/Forward/Frag.frag` - Multi-light support
- `/home/gilmar/dev/Freya/Shaders/DeferredCompressed/lighting.frag` - Multi-light support

## Next Steps

1. **Fix binding 4 in DeferredCompressedPassBuilder**: Add code to update the light buffer descriptor for binding 4 in the lighting input descriptor set. The LightService needs to be accessed here.

2. **Update DeferredCompressedPass constructor**: Pass LightService reference to the pass and expose a method to bind the light buffer for each frame.

3. **Update Renderer::EndFrame()**: Before calling `DrawFullscreenTriangle` for the lighting pass, bind the light buffer descriptor set to binding 4.

4. **Test**: Run the Sofa example in both Forward and Deferred modes to verify lighting works correctly.