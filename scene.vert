#version 410

layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord0;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec3 Tangent;
layout(location = 4) in vec4 Bitangent;

uniform mat4 ModelViewProjection;

out vec2 fTexCoord0;

void main()
{
    gl_Position = ModelViewProjection * Position;
    fTexCoord0 = TexCoord0;
}