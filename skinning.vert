#version 410

layout(location = 0) in  vec4 Position;
// layout(location = 1) in  vec2 TexCoord0;
layout(location = 2) in  vec3 Normal;
layout(location = 3) in  vec3 Tangent;
layout(location = 4) in  vec3 Bitangent;
layout(location = 5) in uvec4 BoneIDs;
layout(location = 6) in  vec4 Weights;

uniform samplerBuffer BoneTransforms;

out vec3 oPosition;
out vec3 oNormal;
out vec3 oTangent;
out vec3 oBitangent;

void main()
{
    mat4 skinningTransform = mat4(0.0);

    for (int i = 0; i < 4; i++)
    {
        mat4 boneTransform = mat4(
                texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 0),
                texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 1),
                texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 2),
                texelFetch(BoneTransforms, int(BoneIDs[i]) * 4 + 3));

        skinningTransform += Weights[i] * boneTransform;
    }

    oPosition = vec3(skinningTransform * Position);

    // assuming no non-uniform scale
    oNormal = mat3(skinningTransform) * Normal;
    oTangent = mat3(skinningTransform) * Tangent;
    oBitangent = mat3(skinningTransform) * Bitangent;
}
