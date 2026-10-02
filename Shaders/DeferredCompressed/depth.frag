#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "Include/shadow_alpha.inc"

layout (location = 0) in vec2 inTexCoord;
layout (location = 1) flat in uint inMaterialId;

void main() {
    if (materials[inMaterialId].alphaMode == 2u)
        discard;
    discardMaskedShadow(inTexCoord, inMaterialId);
}
