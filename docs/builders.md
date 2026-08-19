# Builders

Freya uses the builder pattern to provide a fluent API for constructing engine
components. Apps configure the engine with `FreyaExtension::WithOptions`.
Vulkan device / swapchain / pass builders are internal (`src/Freya/`).

## FreyaExtension hooks

```cpp
.WithExtension<fra::FreyaExtension>([](fra::FreyaExtension& freya) {
    freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
        o.SetTitle("My App")
         .SetShaderRoot("./Resources/Shaders")
         .SetEnableBloom(true);
    });
})
```

Shader SPIR-V paths are `shaderRoot + "/DeferredCompressed|Shadow|Pick/…"`.

## Overview

Public builders follow a consistent pattern:

```cpp
auto component = SomeBuilder(params)
    .SetProperty1(value1)
    .SetProperty2(value2)
    .Build();
```

## FreyaOptionsBuilder

Configures engine options.

```cpp
freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
    freyaOptions.SetTitle("My App")
        .SetWidth(1920)
        .SetHeight(1080)
        .SetVSync(false)
        .SetSampleCount(8)
        .SetFullscreen(false)
        .SetDrawDistance(1000.0f)
        .SetShadowQuality(fra::ShadowQuality::Medium);
});
```

`SetShadowQuality` applies Low / Medium / High / Ultra budgets for shadow map
resolution, cascade count, spot/point slots, and soft-shadow tap count.
Individual setters can still override fields after the preset.

## FullscreenEffectBuilder

Builds a post-deferred fullscreen pass. Obtain it from the service provider
and insert the stage before Bloom:

```cpp
auto cell = serviceProvider->GetService<fra::FullscreenEffectBuilder>()
                ->SetName("Cell")
                .SetFragment("Cell/cell.frag.spv")
                .SetInputs({ fra::EffectInput::SceneColor,
                             fra::EffectInput::Depth,
                             fra::EffectInput::Normal })
                .SetPushConstantSize(sizeof(fra::CellPushConstants))
                .Build();

mRenderer->InsertFrameStage("Bloom", cell->MakeStage());
```

See [Flexibility](flexibility.md).
