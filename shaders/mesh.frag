#version 460

#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

void main()
{
    float lightValue = max(dot(sceneData.sunlightDirection, inNormal), 0.1f);
    vec3 fragColor = inColor.xyz * materialData.colorFactors.xyz * texture(colorTex, inUV).xyz;
    vec3 ambientFinalColor = fragColor * sceneData.ambientColor.xyz;
    outFragColor = vec4(ambientFinalColor + lightValue * sceneData.sunlightColor.xyz * fragColor, 1.0f);
}