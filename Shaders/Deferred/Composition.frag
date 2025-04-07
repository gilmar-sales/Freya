#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputAlbedo;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inputNormal;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inputPosition;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;


void main() 
{
	// Read G-Buffer values from previous sub pass
	vec3 fragPos = subpassLoad(inputPosition).rgb;
	vec3 normal = subpassLoad(inputNormal).rgb;
	vec4 albedo = subpassLoad(inputAlbedo);
	
  	outColor = vec4(fragPos, 1.0);	
}