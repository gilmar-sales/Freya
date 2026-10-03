#ifndef VELOCITY_INC
#define VELOCITY_INC

// UV-space motion vector from the xyw of the UNJITTERED current and previous
// clip positions. Producers interpolate clip xyw (linear in screen space under
// perspective-correct interpolation) and divide per pixel: dividing per vertex
// breaks for triangles crossing the near plane, where w <= 0 at a vertex.
// A previous position behind the camera has no valid reprojection: report no
// motion instead of a garbage vector.
vec2 VelocityFromClip(vec3 currClipXYW, vec3 prevClipXYW)
{
    if (currClipXYW.z <= 1e-5 || prevClipXYW.z <= 1e-5)
        return vec2(0.0);
    return (currClipXYW.xy / currClipXYW.z - prevClipXYW.xy / prevClipXYW.z) *
           0.5;
}

#endif
