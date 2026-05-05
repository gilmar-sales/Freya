# Plan: Implement Unified Lighting System

## Contexto Atual

O projeto Freya possui:
- **Forward rendering**: iluminação calculada no fragment shader com luz direcional única (`ambientLight`)
- **DeferredCompressed rendering**: iluminação em pass separado (fullscreen quad) com luz direcional hardcoded
- **Shaders/Deferred/**: possui `Light` struct com 6 point lights (não usado pelo pipeline atual)

### Estrutura Atual

**ProjectionUniformBuffer** (`src/Freya/Core/UniformBuffer.hpp`):
```cpp
struct ProjectionUniformBuffer {
    alignas(64) glm::mat4 view;
    alignas(64) glm::mat4 projection;
    alignas(64) glm::vec4 ambientLight; // direction.xyz, intensity.w
};
```

**Sampler layout (set=1) por render path:**

| Binding | Forward | Deferred G-buffer |
|---------|---------|-------------------|
| 0 | albedoSampler | albedoSampler |
| 1 | normalSampler | normalSampler |
| 2 | roughnessSampler | (unused) |

### Sistema de Luz Existente (Shaders/Deferred/Composition.frag)

```glsl
struct Light {
    vec4 position;  // xyz = position, w = type (0=point, 1=directional, 2=spot)
    vec3 color;
    float radius;
};
// UBO com 6 lights + viewPos + displayDebugTarget
```

## Objetivo

Criar um sistema de iluminação unificado que:
1. Suporte múltiplas luzes (point, directional, spot)
2. Funcione com Forward e Deferred rendering
3. Use a mesma estrutura de dados em ambos os caminhos
4. Seja configurável via FreyaOptions ou serviço dedicado

## Design Proposto

### 1. Estrutura de Dados

#### LightUniformBuffer (novo)

```cpp
struct LightData {
    static constexpr std::uint32_t MAX_LIGHTS = 16;
    
    // Light array (std140 layout requires proper alignment)
    alignas(64) glm::vec4 lightPositions[MAX_LIGHTS];  // xyz = pos, w = type
    alignas(64) glm::vec4 lightColorsAndRadius[MAX_LIGHTS]; // rgb = color, w = radius
    alignas(64) glm::vec4 lightDirectionsAndCutoff[MAX_LIGHTS]; // xyz = dir, w = spotlight inner cutoff
    alignas(64) glm::vec4 lightOuterCutoffAndIntensity[MAX_LIGHTS]; // x = outer cutoff, y = intensity
    
    alignas(16) glm::vec4 viewPosition;  // Camera position for attenuation calculations
    std::uint32_t lightCount;
    std::uint32_t padding[3]; // Alignment
};

enum class LightType : std::uint32_t {
    Point = 0,
    Directional = 1,
    Spot = 2
};
```

**Nota**: `alignas(64)` é usado para conformidade com std140/GLM. Cada `glm::vec4` ocupa 16 bytes, então arrays de vec4 já estão alinhados corretamente.

#### Extensão do ProjectionUniformBuffer

O `ProjectionUniformBuffer` mantém backward compatibility, mas podemos adicionar um ponteiro para `LightData` ou criar um buffer separado para luzes.

**Opção A (Recomendada)**: Buffer separado para lights
- LightData terá seu próprio buffer e descritor set
- Forward: binds no mesmo pipeline que ProjectionUniformBuffer
- Deferred: binds no lighting pass

**Opção B**: Unificar em um único buffer
- Adicionar `LightData lights` ao final do ProjectionUniformBuffer
- Maior coesão, mas aumenta tamanho do UBO

### 2. Arquitetura de Serviço

```cpp
class LightService {
public:
    // Add/remove lights
    void AddLight(const LightData& light);
    void RemoveLight(std::uint32_t index);
    void ClearLights();
    
    // Update light buffer (called per frame)
    void UpdateBuffers();
    
    // Getters
    std::uint32_t GetLightCount() const;
    const LightData& GetLightData() const;
    vk::Buffer GetBuffer() const;
    vk::DescriptorSet GetDescriptorSet(std::uint32_t frameIndex) const;
    
private:
    std::vector<LightData> mLights;
    Ref<Buffer> mLightBuffer;
    std::vector<vk::DescriptorSet> mDescriptorSets;
    std::uint32_t mMaxLights = 16;
};
```

### 3. Modificações nos Shaders

#### Forward Rendering

**forward.vert** (sem mudançasNeeded):
- Já outputs `fragPosition`, `fragNormal`, `fragTexCoord`

**forward.frag** (novo):
```glsl
// Input attachments / UBO
layout(set=0, binding=0) uniform ProjectionUniformBuffer {
    mat4 view;
    mat4 proj;
    vec4 ambientLight; // deprecated, use light[0] with type=directional
} pub;

layout(set=0, binding=1) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 viewPosition;
    uint lightCount;
} lights;

// Textures (set=1, same as current)
layout(binding=0) uniform texture2D albedoSampler;
layout(binding=1) uniform texture2D normalSampler;
layout(binding=2) uniform texture2D roughnessSampler;

// Output
layout(location=0) out vec4 outColor;

void main() {
    vec3 albedo = texture(albedoSampler, fragTexCoord).rgb;
    vec3 normal = texture(normalSampler, fragTexCoord).rgb * 2.0 - 1.0;
    float roughness = texture(roughnessSampler, fragTexCoord).r;
    
    vec3 N = normalize(TBN * normal);
    vec3 V = normalize(pub.viewPosition - fragPosition);
    
    vec3 totalLighting = vec3(0.0);
    
    for (int i = 0; i < lights.lightCount; i++) {
        vec3 lightDir;
        float attenuation = 1.0;
        
        if (lights.lightPositions[i].w == 0.0) {
            // Point light
            vec3 toLight = lights.lightPositions[i].xyz - fragPosition;
            float dist = length(toLight);
            lightDir = normalize(toLight);
            attenuation = lights.lightColorsAndRadius[i].w / (dist * dist + 1.0);
        } else if (lights.lightPositions[i].w == 1.0) {
            // Directional light
            lightDir = -normalize(lights.lightDirectionsAndCutoff[i].xyz);
            attenuation = 1.0;
        } else {
            // Spot light
            vec3 toLight = lights.lightPositions[i].xyz - fragPosition;
            float dist = length(toLight);
            lightDir = normalize(toLight);
            float spotCos = dot(lightDir, -normalize(lights.lightDirectionsAndCutoff[i].xyz));
            float spotCutoff = lights.lightOuterCutoffAndIntensity[i].x;
            float spotOuter = lights.lightOuterCutoffAndIntensity[i].y;
            attenuation = smoothstep(spotOuter, spotCutoff, spotCos) * 
                          lights.lightColorsAndRadius[i].w / (dist * dist + 1.0);
        }
        
        // Diffuse
        float diff = max(dot(N, lightDir), 0.0);
        
        // Specular (Blinn-Phong)
        vec3 H = normalize(V + lightDir);
        float spec = pow(max(dot(N, H), 0.0), mix(16.0, 2.0, roughness));
        
        vec3 lightColor = lights.lightColorsAndRadius[i].rgb;
        float intensity = lights.lightOuterCutoffAndIntensity[i].y;
        
        totalLighting += (diff + spec) * lightColor * intensity * attenuation;
    }
    
    outColor = vec4(albedo * totalLighting, 1.0);
}
```

#### Deferred G-Buffer (sem mudançasNeeded)

O G-buffer pass não precisa de mudanças - ele apenas escreve position, normal, albedo. A iluminação é calculada no lighting pass.

#### Deferred Lighting Pass (novo)

O `lighting.frag` atual tem luz hardcoded. Precisamos modificar para usar o LightBuffer:

```glsl
// Input attachments (read-only from previous subpasses)
layout(input_attachment_index=0, set=0, binding=0) uniform texture2D depthInput;
layout(input_attachment_index=1, set=0, binding=1) uniform texture2D positionInput;
layout(input_attachment_index=2, set=0, binding=2) uniform texture2D normalInput;
layout(input_attachment_index=3, set=0, binding=3) uniform texture2D albedoInput;

layout(set=1, binding=0) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 viewPosition;
    uint lightCount;
} lights;

layout(location=0) out vec4 outColor;

void main() {
    // Sample G-buffer
    ivec2 coord = ivec2(gl_FragCoord.xy);
    float depth = texelFetch(depthInput, coord, 0).r;
    vec3 position = texelFetch(positionInput, coord, 0).rgb;
    vec3 normal = texelFetch(normalInput, coord, 0).rgb;
    vec4 albedo = texelFetch(albedoInput, coord, 0);
    
    // Skip skybox/no geometry
    if (depth == 0.0) { discard; }
    
    // Reconstruct N
    normal = normal * 2.0 - 1.0; // Un-flip Y
    
    // V from position and viewPosition
    vec3 V = normalize(lights.viewPosition.xyz - position);
    
    // Same lighting loop as forward
    vec3 totalLighting = vec3(0.0);
    for (int i = 0; i < lights.lightCount; i++) {
        // ... same calculation ...
    }
    
    outColor = vec4(albedo.rgb * totalLighting, 1.0);
}
```

### 4. Modificações no código C++

#### LightService (novo)

Criar em `src/Freya/Core/LightService.hpp`:

```cpp
class LightService {
public:
    struct Light {
        glm::vec3 position;
        float type; // 0=point, 1=directional, 2=spot
        glm::vec3 color;
        float radius;
        glm::vec3 direction; // for directional/spot
        float innerCutoff; // for spot
        float outerCutoff; // for spot
        float intensity;
    };

    LightService(const Ref<Device>& device, std::uint32_t maxLights = 16);
    ~LightService();
    
    void AddLight(const Light& light);
    void RemoveLight(std::uint32_t index);
    void ClearLights();
    
    void Update(std::uint32_t frameIndex, const glm::vec3& viewPosition);
    
    Ref<Buffer> GetBuffer() const { return mBuffer; }
    vk::DescriptorSetLayout GetLayout() const { return mLayout; }
    
private:
    Ref<Device> mDevice;
    std::uint32_t mMaxLights;
    std::vector<Light> mLights;
    Ref<Buffer> mBuffer;
    vk::DescriptorSetLayout mLayout;
    vk::DescriptorPool mPool;
    std::vector<vk::DescriptorSet> mSets;
};
```

#### Modificações no FreyaExtension

Adicionar registro do LightService:

```cpp
void FreyaExtension::Register(skr::ServiceCollection& serviceCollection) {
    // ... existing registrations ...
    serviceCollection.Register<LightService>(device, maxLights);
}
```

#### Modificações no Renderer

- Adicionar `Ref<LightService> mLightService` como membro
- No `BeginFrame()` ou `UpdateCamera()`, chamar `mLightService->Update(frameIndex, cameraPosition)`
- No `BindSubpass()` ou `AdvanceSubpass()` para o lighting pass, binds do light buffer

#### Modificações no MaterialPool

Se necessário, ajustar sampler layout para suportar mais texturas. Actualmente:
- binding 0 = albedo
- binding 1 = normal  
- binding 2 = roughness

Com iluminação avançada podemos adicionar:
- binding 3 = emissive
- binding 4 = metallic

## Arquivos a Modificar

### Novos Arquivos

1. `src/Freya/Core/LightService.hpp` - Header do serviço de luzes
2. `src/Freya/Core/LightService.cpp` - Implementação do serviço
3. `src/Freya/Builders/LightServiceBuilder.hpp` - Builder para criar o serviço

### Arquivos Existentes

4. `src/Freya/Core/UniformBuffer.hpp` - Adicionar `LightUniformBuffer` struct
5. `src/Freya/Core/LightService.hpp` - Atualizar se necessário
6. `src/Freya/Core/FreyaExtension.hpp` - Registrar LightService
7. `src/Freya/Core/FreyaExtension.cpp` - Implementar registro
8. `src/Freya/Core/Renderer.hpp` - Adicionar mLightService
9. `src/Freya/Core/Renderer.cpp` - Bind light buffer no UpdateCamera/BeginFrame

### Shaders a Modificar

10. `Shaders/Forward/forward.frag` - Usar LightBuffer em vez de ambientLight hardcoded
11. `Shaders/DeferredCompressed/lighting.frag` - Usar LightBuffer
12. `Shaders/Forward/forward.vert` - Provavelmente sem mudanças
13. `Shaders/DeferredCompressed/lighting.vert` - Provavelmente sem mudanças

## Ordem de Implementação Sugerida

1. **Criar LightUniformBuffer struct** em `UniformBuffer.hpp`
2. **Criar LightService** com buffer e descritor sets
3. **Criar LightServiceBuilder** para injeção de dependência
4. **Registrar LightService** no FreyaExtension
5. **Modificar forward.frag** para usar LightBuffer
6. **Modificar lighting.frag (Deferred)** para usar LightBuffer
7. **Atualizar Renderer** para fazer bind do light buffer
8. **Atualizar MaterialPool** se necessário para sampler layout
9. **Testar com exemplo Sofa** em ambos Forward e Deferred modes

## Considerações Técnicas

### Descriptor Set Layout

O LightBuffer precisa de um descriptor set layout próprio:

```cpp
// LightBuffer layout
layout(set=X, binding=0) uniform LightBuffer {
    vec4 lightPositions[16];
    vec4 lightColorsAndRadius[16];
    vec4 lightDirectionsAndCutoff[16];
    vec4 lightOuterCutoffAndIntensity[16];
    vec4 viewPosition;
    uint lightCount;
} lights;
```

Para Forward rendering:
- set 0 = ProjectionUniformBuffer (binding 0) + LightBuffer (binding 1)
- set 1 = Textures (albedo, normal, roughness)

Para Deferred Lighting:
- set 0 = Input attachments (depth, position, normal, albedo) - já existe
- set 1 = LightBuffer (binding 0)

### Pipeline Layout Updates

Precisamos criar pipelines com o novo descriptor set layout. Para Forward:
- O pipeline de forward precisa ter set 0 com bindings 0 (UBO) e 1 (LightBuffer)

Para Deferred:
- O pipeline de lighting já usa um layout específico (`fullscreenPipelineLayout`)
- Precisamos adicionar o LightBuffer ao set 0 ou criar um novo set

### Performance Considerations

1. **Buffer frequency**: Light buffer pode ser atualizado apenas quando luzes mudam, não todo frame
2. **Shadow maps**: Não implementado nesta fase
3. **Light culling**: Não implementado nesta fase - todas 16 luzes são passadas sempre
4. **Binding overhead**: Para Deferred, o lighting pass já faz bind de input attachments, adicionar light buffer é mínimo

### Backward Compatibility

O `ambientLight` no ProjectionUniformBuffer será ignorado se `lightCount > 0`. Para aplicações existentes sem luzes, o sistema deve manter comportamento com luz direcional padrão.

## Debugging

Para verificar se está funcionando:
1. Adicionar luz point colorida no centro da cena
2. Verificar se a iluminação aparece corretamente em ambos os modos
3. Verificar que não há validation layer errors

## Testing Plan

1. **Forward com 0 luzes**: Scene deve usar fallback (talvez ambient branco)
2. **Forward com 1 luz direcional**: Deve funcionar como antes
3. **Forward com múltiplas point lights**: Iluminação acumulativa
4. **Deferred com 0 luzes**: Fundo preto ou ambient
5. **Deferred com point lights**: Point lights visíveis
6. **Deferred com spot light**: Cone de luz visível
7. **Alternar entre Forward/Deferred**: Ambos devem compilar e funcionar