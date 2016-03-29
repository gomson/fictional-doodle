#version 410

layout(location = 0) in vec3 Position;

uniform mat4 ModelLightProjection;

void main()
{
    gl_Position = ModelLightProjection * vec4(Position, 1.0);
}