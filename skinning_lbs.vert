// Linear Blend Skinning

#version 410

layout(location = 0) in  vec4 Position;
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
    // Transposed skinning matrix
    mat3x4 skinningTransform = mat3x4(0.0);

    // Blend matrices
    for (int i = 0; i < 4; i++)
    {
        skinningTransform[0] += Weights[i] * texelFetch(BoneTransforms, int(BoneIDs[i]) * 3 + 0);
        skinningTransform[1] += Weights[i] * texelFetch(BoneTransforms, int(BoneIDs[i]) * 3 + 1);
        skinningTransform[2] += Weights[i] * texelFetch(BoneTransforms, int(BoneIDs[i]) * 3 + 2);
    }

    // Left multiply vectors with transposed matrix to undo transposition
    oPosition  = Position  * skinningTransform;
    oNormal    = Normal    * mat3(skinningTransform);
    oTangent   = Tangent   * mat3(skinningTransform);
    oBitangent = Bitangent * mat3(skinningTransform);
}
