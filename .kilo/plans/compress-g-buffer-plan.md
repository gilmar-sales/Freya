# Compressão de G-Buffer

Vou analisar cada attachment e recomendar o formato ideal baseado nas características físicas dos dados.

---

## Análise por Attachment

### 1. Position — **Eliminar completamente**
O maior ganho possível: reconstrua a posição a partir do **depth buffer + UV + inverse projection matrix**.

```glsl
vec3 ReconstructPosition(vec2 uv, float depth) {
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpace = invProjection * clipSpace;
    viewSpace /= viewSpace.w;
    return (invView * viewSpace).xyz;
}
```
- **Saving:** elimina um attachment inteiro (~16–32 bytes/pixel)
- **Custo:** algumas instruções a mais no shader de lighting

---

### 2. Normal — `RGB10A2_UNORM` ou Octahedron Encoding

Normais não precisam de float completo. Use **Octahedron Encoding** para comprimir para `RG16F` ou até `RG8`:

```glsl
// Encode (no G-buffer pass)
vec2 EncodeNormal(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    return n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
}

// Decode (no lighting pass)
vec3 DecodeNormal(vec2 enc) {
    vec3 n = vec3(enc.xy, 1.0 - abs(enc.x) - abs(enc.y));
    float t = max(-n.z, 0.0);
    n.xy += mix(vec2(t), vec2(-t), greaterThanEqual(n.xy, vec2(0.0)));
    return normalize(n);
}
```

| Formato | Qualidade | Tamanho |
|---|---|---|
| `RGB16F` | Alta | 6 bytes |
| `RGB10A2` | Boa | 4 bytes |
| `RG16F` (oct) | Boa | 4 bytes |
| `RG8` (oct) | Aceitável | 2 bytes |

---

### 3. Albedo + Specular — `RGBA8_UNORM`
Albedo é cor em sRGB — **8 bits por canal é suficiente**. Specular intensity também cabe em 8 bits.

```
R8 G8 B8 = Albedo (sRGB)
A8        = Specular intensity
```
**Formato:** `RGBA8_UNORM` com `GL_FRAMEBUFFER_SRGB` ativado

---

### 4. Metalness + Roughness — **Empacotar juntos em `RG8`**

Ambos são valores escalares `[0,1]` — empacote-os num único attachment:

```glsl
// Encode
gbuffer.metalRough = vec2(metalness, roughness);

// Decode
float metalness  = texture(gbuffer, uv).r;
float roughness  = texture(gbuffer, uv).g;
```
**Formato:** `RG8_UNORM` — salva um attachment inteiro.

---

### 5. Emissive — `R11F_G11F_B10F`
Emissive pode ter valores HDR (> 1.0 para bloom), mas **não precisa de canal alpha**. O formato `R11F_G11F_B10F` dá HDR com apenas **4 bytes/pixel**:

- 11 bits para R e G, 10 bits para B
- Sem canal alpha (emissive não precisa)
- Suporte nativo como render target em hardware moderno

---

## Layout Final Recomendado

| Attachment | Formato | Bytes/pixel | Observação |
|---|---|---|---|
| Position | ❌ **Removido** | 0 | Reconstruir do depth |
| Depth | `DEPTH24_STENCIL8` | 4 | Já existia |
| Normal | `RG16F` (oct) | 4 | Octahedron encoding |
| Albedo+Spec | `RGBA8_UNORM` | 4 | sRGB |
| Metal+Rough | `RG8_UNORM` | 2 | Empacotado |
| Emissive | `R11F_G11F_B10F` | 4 | HDR sem alpha |
| **Total** | | **18 bytes** | vs ~72 bytes ingênuo |

---

## Dica Extra: Tile-Based (Mobile/Console)

Se o seu target incluir **tile-based GPUs** (Mali, Apple Silicon, consoles), considere **Programmable Blending / Subpass inputs** — você pode ler o G-buffer diretamente no tile sem ir à memória principal, reduzindo drasticamente o bandwidth mesmo sem mudar os formatos.