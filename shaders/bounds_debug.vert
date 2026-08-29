#version 460

#extension GL_EXT_buffer_reference: require

struct Vertex
{
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants
{
    mat4 worldTransform;
    VertexBuffer vertexBuffer;
} pushConstants;

//Scene data descriptor set binding
layout(set = 0, binding = 0) uniform SceneData
{
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
    vec4 sunlightDirection; //w used for sunlight power
    vec4 sunlightColor;
} sceneData;

layout(location = 0) out vec4 outColor;

void main()
{
    //Get vertex using Vertex Index indexing from vertex buffer
    Vertex vertex = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = sceneData.viewProj * pushConstants.worldTransform * vec4(vertex.position, 1.0f);

    outColor = vertex.color;
}

