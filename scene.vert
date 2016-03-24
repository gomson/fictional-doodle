#version 410

layout(location = 0) in  vec4 Position;
layout(location = 1) in  vec2 TexCoord;
layout(location = 2) in  vec3 Normal;
layout(location = 3) in  vec3 Tangent;
layout(location = 4) in  vec3 Bitangent;

uniform mat4 ModelWorld;
//uniform mat4 ModelView;
uniform mat4 ModelViewProjection;
//uniform mat4 WorldView;

out vec3 fPosition;
out vec2 fTexCoord;
out vec3 fNormal;
out vec3 fTangent;
out vec3 fBitangent;

void main()
{
    fPosition = Position.xyz;
    fTexCoord = TexCoord;
    fNormal = Normal;
    fTangent = Tangent;
    fBitangent = Bitangent;

    gl_Position = ModelViewProjection * Position;
}
