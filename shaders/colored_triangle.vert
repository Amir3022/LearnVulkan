#version 460

layout(location = 0) out vec3 outColor;

void main()
{
    //Const array of vertices positions for the triangle
    const vec3 positions[3] =
    {
        vec3(-0.5f, 0.5f, 0.0f),
        vec3(0.0f, -0.7f, 0.0f),
        vec3(0.5f, 0.5f, 0.0f)
    };

    //const array of colors for each vertex
    const vec3 colors[3] = 
    {
        vec3(1.0f, 0.0f, 0.0f),
        vec3(0.0f, 1.0f, 0.0f),
        vec3(0.0f, 0.0f, 1.0f)
    };

    //Set the Vertex position based on the current vertex index
    gl_Position = vec4(positions[gl_VertexIndex], 1.0f);

    //Set the color of the vertex to be used by the rasterizer to color all pixels between vertices
    outColor = colors[gl_VertexIndex];
}