//Scene data descriptor set bindings
layout(set = 0, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
    vec4 sunlightDirection; //w used for sunlight power
    vec4 sunlightColor;
} sceneData;

//Material Descriptor Set bindings
layout(set = 1, binding = 0) uniform MaterialData
{
    vec4 colorFactors;
    vec4 metal_roughFactors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughnessTex;