// Dual Quaternion Linear Blending

# version 410

layout(location = 0) in  vec3 Position;
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

vec3 QuatRotate(in vec4 q, in vec3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

void main()
{
    vec4 real = vec4(0.0);
    vec4 dual = vec4(0.0);

    // Blend dual quaternions
    for (int i = 0; i < 4; i++)
    {
        real += Weights[i] * texelFetch(BoneTransforms, int(BoneIDs[i]) * 2 + 0);
        dual += Weights[i] * texelFetch(BoneTransforms, int(BoneIDs[i]) * 2 + 1);
    }

    // Normalize dual quaternion to represent a rigid transformation
    float len = length(real);
    real /= len;
    dual /= len;

    // Rotate
    oPosition  = QuatRotate(real, Position);
    oNormal    = QuatRotate(real, Normal);
    oTangent   = QuatRotate(real, Tangent);
    oBitangent = QuatRotate(real, Bitangent);

    // Translate
    oPosition += 2.0 * (real.w * dual.xyz - dual.w * real.xyz + cross(real.xyz, dual.xyz));
}
