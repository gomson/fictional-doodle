#version 410

layout(location = 0) in  vec4 Position;
// layout(location = 1) in  vec2 TexCoord0;
layout(location = 2) in  vec3 Normal;
layout(location = 3) in  vec3 Tangent;
layout(location = 4) in  vec3 Bitangent;
layout(location = 5) in uvec4 BoneIDs;
layout(location = 6) in  vec4 Weights;

uniform mat4 ModelWorld;
uniform samplerBuffer BoneTransforms;
uniform int BoneOffset;

out vec3 oPosition;
out vec3 oNormal;
out vec3 oTangent;
out vec3 oBitangent;

void main()
{
    // TODO: Use transform feedback to skin separately.
    mat4 skinningTransform = mat4(0.0);

    for (int i = 0; i < 4; i++)
    {
        mat4 boneTransform = mat4
            (
                texelFetch(BoneTransforms, int(BoneOffset + BoneIDs[i]) * 4 + 0),
                texelFetch(BoneTransforms, int(BoneOffset + BoneIDs[i]) * 4 + 1),
                texelFetch(BoneTransforms, int(BoneOffset + BoneIDs[i]) * 4 + 2),
                texelFetch(BoneTransforms, int(BoneOffset + BoneIDs[i]) * 4 + 3)
                );

        skinningTransform += Weights[i] * boneTransform;
    }

    vec4 skinnedPosition = skinningTransform * Position;
    vec4 worldPosition = ModelWorld * skinnedPosition;
    oPosition = worldPosition.xyz;

    // assuming no non-uniform scale
    vec3 skinnedNormal = mat3(skinningTransform) * Normal;
    vec3 skinnedTangent = mat3(skinningTransform) * Tangent;
    vec3 skinnedBitangent = mat3(skinningTransform) * Bitangent;
    oNormal = mat3(ModelWorld) * skinnedNormal;
    oTangent = mat3(ModelWorld) * skinnedTangent;
    oBitangent = mat3(ModelWorld) * skinnedBitangent;
}
