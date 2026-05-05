# Plan: Finalizar Implementação da Deferred Graphics Pipeline

## Contexto Atual

O projeto Freya já possui:
- `FreyaOptions` com `RenderingStrategy` enum (`Forward` e `Deferred`)
- `RenderPass` totalmente implementado para Forward Rendering
- `DeferredCompressedPass` parcialmente implementado (apenas estrutura básica)
- Shaders básicos para G-buffer em `Shaders/Deferred/`
- Shaders adicionais em `Shaders/DeferredCompressed/` (translucency, lighting, composing)

## Objetivo

Completar a implementação da Deferred Graphics Pipeline respeitando a arquitetura existente, permitindo alternar entre Forward e Deferred via `FreyaOptions`.

## Estrutura da Deferred Pipeline

Baseada no código existente em `DeferredCompressedPass.hpp`, a pipeline terá 5 subpasses:

1. **DepthPrePass** (subpass 0): Depth-only pass para early-Z
2. **GBufferPass** (subpass 1): Gera G-buffer (position, normal, albedo, metallic/roughness)
3. **LightingPass** (subpass 2): Calcula iluminação usando G-buffer
4. **TranslucentPass** (subpass 3): Objetos transparentes (forward tradicional)
5. **CompositePass** (subpass 4): Combina opaque + translucent no back buffer

### Attachments (baseado no código existente):

- `DeferredBackAttachment`: Back buffer color (swapchain)
- `DeferredDepthAttachment`: Depth buffer
- `DeferredGBufferAttachment`: G-buffer (position/normal compactado)
- `DeferredTranslucentAttachment`: Buffer para objetos translúcidos
- `DeferredOpaqueAttachment`: Buffer para objetos opacos (resultado da iluminação)

## Implementação

### 1. Expandir DeferredCompressedPass (`src/Freya/Core/DeferredCompressedPass.hpp`)

Adicionar membros necessários:
- Múltiplos graphics pipelines (um por subpass relevante)
- Pipeline layouts
- Descriptor sets para sampling do G-buffer no lighting pass
- G-buffer images (se necessário criar manualmente)
- Framebuffers
- Uniform buffers

```cpp
class DeferredCompressedPass {
    // Pipelines por subpass
    vk::Pipeline mDepthPrepassPipeline;
    vk::Pipeline mGBufferPipeline;
    vk::Pipeline mLightingPipeline;
    vk::Pipeline mTranslucentPipeline;
    vk::Pipeline mCompositePipeline;
    
    vk::PipelineLayout mPipelineLayout;
    vk::PipelineLayout mLightingPipelineLayout; // com sampler layout
    
    // G-buffer attachments (images)
    std::vector<vk::Image> mGBufferImages;
    std::vector<vk::ImageView> mGBufferImageViews;
    std::vector<vk::DeviceMemory> mGBufferMemory;
    
    // Framebuffers
    std::vector<vk::Framebuffer> mFramebuffers;
    
    // Descriptor sets para lighting pass (ler G-buffer)
    vk::DescriptorSetLayout mGBufferSamplerLayout;
    vk::DescriptorPool mGBufferDescriptorPool;
    std::vector<vk::DescriptorSet> mGBufferDescriptorSets;
    
    // Depth image
    Ref<Image> mDepthImage;
    
    // Métodos
    void Begin(const Ref<SwapChain>, const Ref<CommandPool>) const;
    void End(const Ref<CommandPool>) const;
    void BindGBuffer(const Ref<CommandPool>, uint32_t frameIndex) const;
    // ... outros métodos similares ao RenderPass
};
```

### 2. Completar DeferredCompressedPassBuilder (`src/Freya/Builders/DeferredCompressedPassBuilder.cpp`)

O método `Build()` atual apenas cria o render pass e carrega shaders do G-buffer, mas não:
- Cria pipelines para todos os subpasses
- Cria G-buffer images e image views
- Configura descriptor sets para sampling do G-buffer
- Cria framebuffers

Implementar:

```cpp
Ref<DeferredCompressedPass> DeferredCompressedPassBuilder::Build() {
    auto renderPass = createRenderPass();
    
    // Criar G-buffer images (position/normal, albedo, metallic/roughness)
    // baseado na resolução da swapchain
    
    // Criar pipelines para cada subpass:
    // - Depth prepass pipeline (depth-only)
    // - G-buffer pipeline (escreve em múltiplos attachments)
    // - Lighting pipeline (fullscreen quad, lê G-buffer via input attachments)
    // - Translucent pipeline (forward tradicional)
    // - Composite pipeline (combina opaque + translucent)
    
    // Criar descriptor sets para ler G-buffer no lighting pass
    
    // Criar framebuffers com todos os attachments
    
    return MakeRef<DeferredCompressedPass>(...);
}
```

Shaders necessários (verificar se existem ou precisam ser criados):
- `Shaders/DeferredCompressed/depth.vert.spv` - Depth pre-pass
- `Shaders/DeferredCompressed/gbuffer.vert.spv` + `gbuffer.frag.spv` - Já existe
- `Shaders/DeferredCompressed/lighting.vert.spv` + `lighting.frag.spv` - Vert existe, precisa frag
- `Shaders/DeferredCompressed/translucency.vert.spv` + `translucency.frag.spv` - Já existe
- `Shaders/DeferredCompressed/composing.vert.spv` + `composing.frag.spv` - Já existe

### 3. Implementar métodos Begin/End em DeferredCompressedPass

Diferente do RenderPass simples, o DeferredCompressedPass precisa gerenciar múltiplos subpasses:

```cpp
void DeferredCompressedPass::Begin(const Ref<SwapChain> swapChain,
                                   const Ref<CommandPool> commandPool) const {
    auto commandBuffer = commandPool->GetCommandBuffer();
    
    // Começar render pass com CLEAR values para todos os attachments
    auto clearValues = std::vector<vk::ClearValue> {
        vk::ClearValue().setColor(...), // Back buffer
        vk::ClearValue().setDepthStencil(...), // Depth
        vk::ClearValue().setColor(...), // G-buffer
        vk::ClearValue().setColor(...), // Translucent
        vk::ClearValue().setColor(...), // Opaque
    };
    
    commandBuffer.beginRenderPass(...);
    
    // O Vulkan automaticamente gerencia transições de subpass
    // O primeiro subpass (depth pre-pass) já está ativo
}

void DeferredCompressedPass::End(const Ref<CommandPool> commandPool) const {
    auto commandBuffer = commandPool->GetCommandBuffer();
    
    // Terminar último subpass e render pass
    commandBuffer.endRenderPass();
}
```

### 4. Modificar Renderer para suportar ambas as estratégias

O `Renderer` atual usa apenas `Ref<RenderPass>`. Opções:

**Opção A (Recomendada):** Usar `std::variant` ou type erasure
```cpp
class Renderer {
    // Em vez de: Ref<RenderPass> mRenderPass;
    std::variant<Ref<RenderPass>, Ref<DeferredCompressedPass>> mActivePass;
    
    // Ou criar interface comum:
    // Ref<IRenderPass> mActivePass;
};
```

**Opção B:** Ter ambos e selecionar baseado na estratégia
```cpp
class Renderer {
    Ref<RenderPass> mForwardPass;
    Ref<DeferredCompressedPass> mDeferredPass;
    
    // Getter que retorna o pass ativo
    auto GetActivePass() const {
        return mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred
            ? mDeferredPass : mForwardPass;
    }
};
```

### 5. Modificar RendererBuilder

Atualizar para construir o pass correto baseado na estratégia:

```cpp
Ref<Renderer> RendererBuilder::Build() {
    Ref<RenderPass> forwardPass = nullptr;
    Ref<DeferredCompressedPass> deferredPass = nullptr;
    
    if (mFreyaOptions->renderingStrategy == RenderingStrategy::Forward) {
        forwardPass = mServiceProvider->GetService<RenderPassBuilder>()->Build();
    } else {
        deferredPass = mServiceProvider->GetService<DeferredCompressedPassBuilder>()->Build();
    }
    
    return MakeRef<Renderer>(..., forwardPass, deferredPass, ...);
}
```

### 6. Atualizar BeginFrame/EndFrame no Renderer

O Renderer precisa chamar os métodos apropriados baseado na estratégia atual.

## Arquivos a Modificar

1. `src/Freya/Core/DeferredCompressedPass.hpp` - Adicionar membros e métodos
2. `src/Freya/Core/DeferredCompressedPass.cpp` - Implementar métodos (NOVO)
3. `src/Freya/Builders/DeferredCompressedPassBuilder.hpp` - Atualizar se necessário
4. `src/Freya/Builders/DeferredCompressedPassBuilder.cpp` - Completar Build()
5. `src/Freya/Core/Renderer.hpp` - Adicionar suporte para deferred
6. `src/Freya/Core/Renderer.cpp` - Modificar para usar estratégia correta
7. `src/Freya/Builders/RendererBuilder.hpp` - Atualizar para ambas estratégias
8. `src/Freya/Builders/RendererBuilder.cpp` - Completar Build()

## Shaders Necessários

Verificar se os seguintes shaders compilados existem:
- `Shaders/DeferredCompressed/depth.vert.spv` (pode precisar criar)
- `Shaders/DeferredCompressed/lighting.frag.spv` (o vert existe, falta frag)

Os shader source files em `Shaders/DeferredCompressed/` já existem para:
- gbuffer (vert/frag) ✓
- lighting (vert) ✗ falta frag
- translucency (vert/frag) ✓
- composing (vert/frag) ✓

## Considerações Técnicas

1. **G-buffer Format**: O código atual usa `eR32G32B32A32Uint` para G-buffer. Considerar formatos mais eficientes como packing position+normal em 2x `eR16G16B16A16_SFloat` ou usar `eR8G8B8A8` para albedo.

2. **Subpass Dependencies**: Já definidas em `DeferredCompressedPassBuilder::createRenderPass()` - verificar se estão corretas.

3. **Input Attachments**: No Vulkan, subpasses podem ler attachments de subpassos anteriores via input attachments. O lighting pass deve ler o G-buffer assim.

4. **Framebuffer Creation**: Precisa criar framebuffers com TODOS os attachments (swapchain image + G-buffer images + depth image).

5. **MSAA**: O código atual do DeferredCompressedPass usa `e1` (sem MSAA). Se necessário, adicionar suporte similar ao RenderPass.

## Ordem de Implementação Sugerida

1. Criar/compilar shaders faltantes (lighting.frag, depth.vert)
2. Completar `DeferredCompressedPassBuilder::Build()` com todos os pipelines
3. Implementar `DeferredCompressedPass` com Begin/End
4. Modificar `Renderer` para suportar ambas estratégias
5. Testar com exemplo Sofa alternando entre Forward e Deferred
