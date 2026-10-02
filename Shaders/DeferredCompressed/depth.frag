#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

// Depth pre-pass must apply the same alpha test as gbuffer.frag, otherwise a
// cutout's full triangle writes depth and hides whatever is behind the holes
// (the G-buffer pass does not write depth).
#include "Include/shadow_alpha.inc"

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) flat in uint inMaterialId;

void main() {
    discardMaskedShadow(inTexCoord, inMaterialId);
}
