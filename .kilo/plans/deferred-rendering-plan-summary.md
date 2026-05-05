# Freya Deferred Rendering — Status & Plan

## Goal
Implement and debug the deferred rendering pipeline for the Freya Vulkan framework, achieving proper multi-subpass rendering with the Sofa example.

## Constraints & Preferences
- Must respect existing architecture patterns (Builder pattern, `skr::ServiceProvider`, `Ref<>`, `FREYA_NAMESPACE = fra`)
- Must support both Forward and Deferred strategies via `RenderingStrategy` enum
- Shaders compiled at build time via `cmake/CompileShaders.cmake` (never manual `glslc`)
- Do not write platform-specific syntax in documentation
- Examples run from their target directory (relative `./Resources/...` paths)

## Progress

### Completed
- Created 10 GLSL shaders in `Shaders/DeferredCompressed/` (depth, gbuffer, lighting, translucent, composing) and compiled to `.spv`
- Rewrote `DeferredCompressedPass.hpp` with 5 pipelines, 2 pipeline layouts, G-buffer images, input attachment descriptors, framebuffers
- Created `DeferredCompressedPass.cpp` with `Begin()`, `End()`, `NextSubpass()`, `BindPipeline()`, `AdvanceSubpass()`, `DrawFullscreenTriangle()`, `UpdateProjection()`
- Rewrote `DeferredCompressedPassBuilder.cpp` (~1000 lines): 7-attachment render pass with 5 subpasses + correct dependencies, 5 graphics pipelines, G-buffer/depth/translucent/opaque images, UBO + sampler + input attachment descriptor sets, per-swapchain-image framebuffers
- Fixed `ImageBuilder.cpp`: added `eInputAttachment` flag to Color/Depth/GBuffer usages, added GBuffer types to aspect mask switch
- Modified `Renderer.hpp/cpp`: stores both forward and deferred passes, delegates Begin/End per strategy, `NextSubpass()`, `BindSubpass()`, `AdvanceSubpass()`, `GetActivePipelineLayout()`, auto-advance through subpasses 2-4 in `EndFrame()` with fullscreen triangle draws
- Modified `RendererBuilder.hpp/cpp`: creates `DeferredCompressedPass` in `Build()` with the same `SwapChain` the Renderer uses (fixes mismatched-swapchain bug)
- Updated `FreyaExtension.cpp`: removed `DeferredCompressedPass` singleton (was creating separate SwapChain), registered `DeferredCompressedPassBuilder` as transient
- Updated `Examples/Sofa/Main.cpp`: draws geometry in both subpass 0 (depth) and subpass 1 (G-buffer), uses `AdvanceSubpass(DeferredGBufferPass)`
- Added `FreyaOptionsBuilder::SetRenderingStrategy()`; Sofa example configured for Deferred mode
- Fixed `RenderPassBuilder::createDependencies()`: always returns forward-compatible (single-subpass) dependencies regardless of strategy
- Fixed deferred render pass backbuffer format: uses `mSurface->QuerySurfaceFormat().format` instead of hardcoded `eR8G8B8A8Unorm`
- Fixed deferred pipeline multisampling: forced to `VK_SAMPLE_COUNT_1_BIT` for all intermediate images
- Fixed input attachment descriptor pool: increased from 6 to 8 descriptors, allocated both lighting+composite sets in single call
- Created `cmake/CompileShaders.cmake` with `find_glslc()` and `add_shader_target()`; top-level CMakeLists.txt uses it; Sofa CMakeLists.txt uses POST_BUILD copy
- Added `GetCurrentImageIndex()` to `SwapChain`; `DeferredCompressedPass::Begin()` now uses image index for framebuffer selection instead of ring-buffer frame index

### In Progress
- Debugging black screen output: all subpasses execute correctly per RenderDoc (depth→gbuffer→lighting→translucent→composite, all draws present), no Vulkan validation errors, but final output is black

### Blocked
- (none)

## Key Decisions
- **Option B (both passes stored)**: Renderer stores `Ref<RenderPass>` (forward) and `Ref<DeferredCompressedPass>` (deferred), selects by `RenderingStrategy`
- **Forward pass always built**: needed by SwapChainBuilder for framebuffer creation; unused in deferred mode
- **SwapChain passed through Builder**: `DeferredCompressedPassBuilder::Build(const Ref<SwapChain>&)` receives the same SwapChain the Renderer uses — framebuffers reference correct swapchain images
- **MaterialPool compatibility**: sampler layout at `set=1` matches forward pass pattern; existing material binding works unchanged
- **EndFrame auto-advance**: advances through subpasses 2-4 automatically, draws fullscreen triangles for lighting and composite; user must advance to subpass 1 (G-buffer) and draw there

## Next Steps
1. Determine why composite output never reaches screen despite correct RenderDoc capture (all draws present, no validation errors)
2. Confirm whether the `GetCurrentImageIndex()` fix resolves the black-screen issue
3. If still black, capture a full RenderDoc frame and inspect the swapchain image at Present time
4. Revert diagnostic shader changes (lighting red, input-attachment reads) once rendering works
5. Add tessellation/subpass-specific content for proper lighting in the Sofa example

## Critical Context
- **Black screen despite correct RenderDoc capture** suggests rendering to wrong swapchain image — fixed by using `GetCurrentImageIndex()` instead of `GetCurrentFrameIndex()` for framebuffer selection
- Forward pass uses `mFrames[mCurrentFrameIndex]` for framebuffer → may have same bug but works because indices happen to match with Immediate present mode
- `MaterialPool::Bind()` uses `mRenderPass->GetPipelineLayout()` (forward layout) for descriptor binding; the G-buffer pipeline uses deferred `mVertexPipelineLayout` — both have identical set layouts at sets 0 and 1, so binding should be compatible
- **Shader naming must match compiled output**: source `Vert.vert` → compiled `Vert.vert.spv`; C++ loads `Vert.vert.spv` (was `Vert.spv` before rename)
- **All intermediate images use VK_SAMPLE_COUNT_1_BIT** regardless of MSAA setting
- **Backbuffer format uses surface format** (e.g. B8G8R8A8Unorm), not hardcoded R8G8B8A8Unorm

## Relevant Files
- `src/Freya/Core/DeferredCompressedPass.hpp`: full deferred pass class with 5 pipelines, 2 layouts, 7 framebuffer attachments
- `src/Freya/Core/DeferredCompressedPass.cpp`: Begin/End/AdvanceSubpass/BindPipeline/DrawFullscreenTriangle/UpdateProjection implementations
- `src/Freya/Builders/DeferredCompressedPassBuilder.hpp`: Builder taking Device/PhysicalDevice/Surface/FreyaOptions/ServiceProvider
- `src/Freya/Builders/DeferredCompressedPassBuilder.cpp`: 1000-line Build() creating render pass, 5 pipelines, all images/framebuffers/descriptors
- `src/Freya/Core/Renderer.hpp` / `Renderer.cpp`: dual-pass support, subpass management methods, EndFrame auto-advance
- `src/Freya/Builders/RendererBuilder.hpp` / `RendererBuilder.cpp`: creates deferred pass with same SwapChain as Renderer
- `src/Freya/Core/FreyaExtension.cpp`: registers DeferredCompressedPassBuilder as transient
- `src/Freya/Builders/RenderPassBuilder.cpp`: createDependencies() always returns single-subpass deps
- `src/Freya/Builders/ImageBuilder.cpp`: added eInputAttachment flag to Color/Depth/GBuffer usages; GBuffer types in aspect mask
- `src/Freya/Core/SwapChain.hpp` / `SwapChain.cpp`: added GetCurrentImageIndex()
- `src/Freya/Builders/FreyaOptionsBuilder.hpp`: added SetRenderingStrategy()
- `Examples/Sofa/Main.cpp`: draws in both depth pre-pass and G-buffer subpass
- `cmake/CompileShaders.cmake`: reusable module for build-time shader compilation
- `CMakeLists.txt`: uses CompileShaders module, Freya depends on Shaders target
- `Examples/Sofa/CMakeLists.txt`: POST_BUILD copies compiled shaders to example's Resources
- `Shaders/DeferredCompressed/`: all 10 GLSL shader sources for deferred pipeline
- `AGENTS.md`: updated with shader pipeline docs, running example instructions
