#version 410

layout(location = 0) in  vec4 Position;
layout(location = 1) in  vec2 TexCoord0;
layout(location = 2) in  vec3 Normal;
layout(location = 3) in  vec3 Tangent;
layout(location = 4) in  vec3 Bitangent;

uniform mat4 WorldView;
uniform mat4 WorldViewProjection;

const vec4 kLightPosition = vec4(200.0, 1000.0, 200.0, 1.0);

out vec2 fTexCoord0;
out vec3 fNormal;
out vec3 fLight;

void main()
{
    fTexCoord0 = TexCoord0;

    vec4 viewPosition = WorldView * Position;
    vec4 viewLightPosition = WorldView * kLightPosition;

    fLight = viewLightPosition.xyz - viewPosition.xyz;

    // assuming no non-uniform scale
    fNormal = mat3(WorldView) * Normal;

    gl_Position = WorldViewProjection * Position;
}
