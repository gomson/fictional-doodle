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
uniform samplerBuffer BoneTransforms;

const vec4 kLightPosition = vec4(200.0, 1000.0, 200.0, 1.0);

out vec2 fTexCoord0;
out vec3 fNormal;
out vec3 fLight;

void main()
{
    fTexCoord0 = TexCoord0;

    // TODO: Use transform feedback to skin separately.
    mat4 skinningTransform = mat4(0.0);

    for (int i = 0; i < 4; i++)
    {
        mat4 boneTransform = mat4
        (
            texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 0),
            texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 1),
            texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 2),
            texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 3)
        );

        skinningTransform += Weights[i] * boneTransform;
    }

    vec4 skinnedPosition = skinningTransform * Position;
    vec4 viewSpacePosition = ModelView * skinnedPosition;
    vec4 viewSpaceLightPosition = View * kLightPosition;

    fLight = viewSpaceLightPosition.xyz - viewSpacePosition.xyz;

    vec3 skinnedNormal = mat3(skinningTransform) * Normal;

    fNormal = mat3(ModelView) * skinnedNormal;

    gl_Position = ModelViewProjection * skinnedPosition;
}
