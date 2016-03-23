#include "skinning.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "scene.h"

static void ComputeUnitQuatW(glm::quat& q)
{
    // Attempt to set the w component so that the quaternion has unit length
    float ww = 1.0f - (q.x * q.x) - (q.y * q.y) - (q.z * q.z);
    q.w = ww < 0.0f ? 0.0f : -sqrt(ww);
}

void DecodeFrame(const Scene* scene, int animationID, int frameID, std::vector<SQT>& boneTransforms)
{
    const AnimSequence& animSeq = scene->AnimSequences[animationID];
    const Skeleton &skeleton = scene->Skeletons[animSeq.SkeletonID];

    int numBones = size(animSeq.BoneBaseFrame);
    int frameOffset = frameID * animSeq.NumFrameComponents;

    boneTransforms.resize(numBones);

    // Decode channel animation data
    for (int bone = 0; bone < numBones; bone++)
    {
        const float* frameData = data(animSeq.BoneFrameData) + animSeq.BoneFrameDataOffsets[bone] + frameOffset;
        glm::vec3 animatedT = animSeq.BoneBaseFrame[bone].T;
        glm::quat animatedQ = animSeq.BoneBaseFrame[bone].Q;

        if (animSeq.BoneChannelBits[bone] & ANIMCHANNEL_TX_BIT)
        {
            animatedT.x = *frameData++;
        }
        if (animSeq.BoneChannelBits[bone] & ANIMCHANNEL_TY_BIT)
        {
            animatedT.y = *frameData++;
        }
        if (animSeq.BoneChannelBits[bone] & ANIMCHANNEL_TZ_BIT)
        {
            animatedT.z = *frameData++;
        }
        if (animSeq.BoneChannelBits[bone] & ANIMCHANNEL_QX_BIT)
        {
            animatedQ.x = *frameData++;
        }
        if (animSeq.BoneChannelBits[bone] & ANIMCHANNEL_QY_BIT)
        {
            animatedQ.y = *frameData++;
        }
        if (animSeq.BoneChannelBits[bone] & ANIMCHANNEL_QZ_BIT)
        {
            animatedQ.z = *frameData++;
        }

        ComputeUnitQuatW(animatedQ);

        if (skeleton.BoneParents[bone] < 0)
        {
            boneTransforms[bone].T = animatedT;
            boneTransforms[bone].Q = animatedQ;
        }
        else
        {
            const SQT& parentTransform = boneTransforms[skeleton.BoneParents[bone]];

            // Rotate position relative to parent
            glm::vec3 rotatedT = glm::rotate(parentTransform.Q, animatedT);

            // Compute final transformations
            boneTransforms[bone].T = parentTransform.T + rotatedT;
            boneTransforms[bone].Q = parentTransform.Q * animatedQ;
        }
    }
}

void InterpolateFrames(Scene* scene)
{

}
