//Scene data descriptor set bindings


//Material Descriptor Set bindings
layout(set = 1, binding = 0) uniform MaterialData
{
    vec4 colorFactors;
    vec4 metal_roughFactors;
} materialData;

layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughnessTex;