#include "animation.h"

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

void DecodeFrame(Scene* scene, int animID, int frameID, std::vector<SQT>& frame)
{
    const AnimSequence& animSeq = scene->AnimSequences[animID];
    const Skeleton &skeleton = scene->Skeletons[animSeq.SkeletonID];

    int frameOffset = frameID * animSeq.NumFrameComponents;

    frame.resize(skeleton.NumBones);

    // Decode channel animation data
    for (int bone = 0; bone < skeleton.NumBones; bone++)
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
            frame[bone].T = animatedT;
            frame[bone].Q = animatedQ;
        }
        else
        {
            // Apply parent transformations
            const SQT& parentTransform = frame[skeleton.BoneParents[bone]];
            frame[bone].T = rotate(parentTransform.Q, animatedT) + parentTransform.T;
            frame[bone].Q = normalize(parentTransform.Q * animatedQ);
        }
    }
}

void InterpolateFrames(
    Scene* scene,
    int animID,
    int frame1ID,
    int frame2ID,
    float alpha,
    std::vector<SQT>& frame)
{
    const AnimSequence& animSeq = scene->AnimSequences[animID];
    const Skeleton& skeleton = scene->Skeletons[animSeq.SkeletonID];

    // TODO: These should be persisted somewhere to avoid reallocations, perhaps on the scene object itself as
    // general buffers to use for any frames?
    std::vector<SQT> frame1;
    std::vector<SQT> frame2;

    DecodeFrame(scene, animID, frame1ID, frame1);
    DecodeFrame(scene, animID, frame2ID, frame2);

    // Maybe only resize if size if too small?
    frame.resize(skeleton.NumBones);

    for (int bone = 0; bone < skeleton.NumBones; bone++)
    {
        frame[bone].T = mix(frame1[bone].T, frame2[bone].T, alpha);
        frame[bone].Q = mix(frame1[bone].Q, frame2[bone].Q, alpha);
    }
}

void GetFrameAtTime(
    Scene* scene,
    int animID,
    int animTime,
    bool interpolate,
    std::vector<SQT>& frame)
{
    const AnimSequence& animSeq = scene->AnimSequences[animID];

    int frameTime = animTime * animSeq.FramesPerSecond;
    int frameNum = frameTime / 1000;
    int frame1ID = frameNum % animSeq.NumFrames;

    if (interpolate)
    {
        int frame2ID = (frame1ID + 1) % animSeq.NumFrames;
        float alpha = (frameTime % 1000) * 0.001f; // Percent of interpolation between frames

        InterpolateFrames(scene, animID, frame1ID, frame2ID, alpha, frame);
    }
    else
    {
        DecodeFrame(scene, animID, frame1ID, frame);
    }
}
