#pragma once

#include <vector>

struct Scene;
struct SQT;

void DecodeFrame(Scene* scene, int animationID, int frameID, std::vector<SQT>& boneTransforms);
void InterpolateFrames(Scene* scene, int animationID, int frame1ID, int frame2ID, std::vector<SQT>& boneTransforms);
