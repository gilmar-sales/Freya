#ifndef GBUFFER_INC
#define GBUFFER_INC

// Single source of truth for the deferred G-buffer packing. Producers
// (gbuffer*.frag, triplanar, unlit_emissive) and consumers (lighting, SSAO,
// shadow mask, outline, cell) must go through these helpers.
//
// Layout (14 B/px with velocity; depth separate):
//   RT0 RGBA8_SRGB       albedo.rgb (HW sRGB) | A = material ID
//   RT1 A2B10G10R10      R,G = octahedral normal (10+10)
//                        B   = roughness (8) << 2 | variant (2)
//                        A   = shading flags (2)
//   RT2 RG8_UNORM        R = metalness
//                        G = AO (variant none) or clearcoat(4)|coatRough(4)
//   RT3 B10G11R11_UFLOAT emissive HDR, then lighting accumulation
//   RT4 RG16F            motion vectors
//
// Normal RTs hold octahedral data: never sample them with a linear filter.

const uint kGBufferFlagNone          = 0u;
const uint kGBufferFlagReceiveShadow = 1u;
const uint kGBufferFlagUnlit         = 3u;

const uint kGBufferVariantNone = 0u; // uncoated PBR, G = AO
const uint kGBufferVariantCoat = 1u; // clearcoat, G = coat nibbles
const uint kGBufferVariantCel  = 2u; // cel-shaded (cell lighting)

// Emissive HDR gain, shared by every G-buffer / OIT producer.
const float kEmissiveIntensity = 2.0;

vec2 GBufferOctWrap(vec2 v)
{
    return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0,
                                     v.y >= 0.0 ? 1.0 : -1.0);
}

// Unit vector -> [0,1]^2.
vec2 GBufferOctEncode(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 e = n.z >= 0.0 ? n.xy : GBufferOctWrap(n.xy);
    return e * 0.5 + 0.5;
}

// [0,1]^2 -> unit vector.
vec3 GBufferOctDecode(vec2 f)
{
    f       = f * 2.0 - 1.0;
    vec3 n  = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy += vec2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
    return normalize(n);
}

// ---- RT1: normal + roughness + variant + flags ---------------------------

vec4 GBufferPackNormal(vec3 worldNormal, float roughness, uint variant,
                       uint flags)
{
    uint  r8   = uint(clamp(roughness, 0.0, 1.0) * 255.0 + 0.5);
    float bits = float((r8 << 2u) | (variant & 3u));
    vec2  oct  = GBufferOctEncode(worldNormal);
    return vec4(oct, bits / 1023.0, float(flags & 3u) / 3.0);
}

vec3 GBufferUnpackNormal(vec4 s)
{
    return GBufferOctDecode(s.xy);
}

float GBufferUnpackRoughness(vec4 s)
{
    return float(uint(s.z * 1023.0 + 0.5) >> 2u) / 255.0;
}

uint GBufferUnpackVariant(vec4 s)
{
    return uint(s.z * 1023.0 + 0.5) & 3u;
}

uint GBufferUnpackFlags(vec4 s)
{
    return uint(clamp(round(s.w * 3.0), 0.0, 3.0));
}

// ---- RT2: metalness + AO / coat ------------------------------------------

vec2 GBufferPackPbr(float metalness, float ao)
{
    return vec2(metalness, ao);
}

vec2 GBufferPackPbrCoat(float metalness, float clearcoat,
                        float coatRoughness)
{
    // Nibble >= 1 so a coated pixel never decodes as uncoated.
    uint c = max(uint(clamp(clearcoat, 0.0, 1.0) * 15.0 + 0.5), 1u);
    uint r = uint(clamp(coatRoughness, 0.0, 1.0) * 15.0 + 0.5);
    return vec2(metalness, float((c << 4u) | r) / 255.0);
}

float GBufferUnpackMetalness(vec2 pbr)
{
    return pbr.x;
}

float GBufferUnpackAo(vec2 pbr)
{
    return pbr.y;
}

float GBufferUnpackClearcoat(vec2 pbr)
{
    return float(uint(pbr.y * 255.0 + 0.5) >> 4u) / 15.0;
}

float GBufferUnpackCoatRoughness(vec2 pbr)
{
    return float(uint(pbr.y * 255.0 + 0.5) & 15u) / 15.0;
}

#endif
