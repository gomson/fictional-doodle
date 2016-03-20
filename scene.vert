#version 410

layout(location = 0) in  vec4 Position;
layout(location = 1) in  vec2 TexCoord0;
layout(location = 2) in  vec3 Normal;
layout(location = 3) in  vec3 Tangent;
layout(location = 4) in  vec4 Bitangent;
layout(location = 5) in uvec4 BoneIDs;
layout(location = 6) in  vec4 Weights;

uniform mat4 View;
uniform mat4 ModelView;
uniform mat4 ModelViewProjection;

const vec4 kLightPosition = vec4(200.0, 1000.0, 200.0, 1.0);

out vec2 fTexCoord0;
out vec3 fNormal;
out vec3 fLight;

void main()
{
    // Compute view space positions.
    vec4 viewSpacePosition = ModelView * Position;
    vec4 viewSpaceLightPosition = View * kLightPosition;

    // Compute fragment shader input.
    fLight = viewSpaceLightPosition.xyz - viewSpacePosition.xyz;
    fNormal = mat3(ModelView) * Normal;
    fTexCoord0 = TexCoord0;

    gl_Position = ModelViewProjection * Position;
}
