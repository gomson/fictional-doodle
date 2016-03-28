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
    vec4 reals[4];
    vec4 duals[4];

    // Read dual quaternion real and dual components from texture buffer
    for (int i = 0; i < 4; i++)
    {
        reals[i] = texelFetch(BoneTransforms, int(BoneIDs[i]) * 2 + 0);
        duals[i] = texelFetch(BoneTransforms, int(BoneIDs[i]) * 2 + 1);
    }

    // Reflect dual quaternions so that the dot products of the real components
    // are positive to ensure consistent interpolation
    for (int i = 1; i < 4; i++)
    {
        // Extract sign bit and map to -1 or 1 for reflection
        uint bits = floatBitsToUint(dot(reals[0], reals[i]));
        int s = 1 - int((bits & 0x80000000u) >> 30);
        reals[i] *= s;
        duals[i] *= s;
    }

    vec4 real = vec4(0.0);
    vec4 dual = vec4(0.0);

    // Blend dual quaternions
    for (int i = 0; i < 4; i++)
    {
        real += Weights[i] * reals[i];
        dual += Weights[i] * duals[i];
    }

    // Normalize
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
