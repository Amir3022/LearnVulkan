#version 450
#extension GL_EXT_buffer_reference : require    //Use the buffer device address extension to be able to use the vertex buffer through it's device address

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

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

void main()
{
    //Get the Current operating vertex from Vertex Buffer using GlobalVertexID
    Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

    //Set the final location as the vertex location multiplied by the transformation matrix
    gl_Position = pushConstants.worldTransform * vec4(v.position, 1.0f);
    //Set values for output color and UVs
    outColor = v.color.xyz;
    outUV = vec2(v.uv_x, v.uv_y);
}