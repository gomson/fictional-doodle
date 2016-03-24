#pragma once

#include <vector>

struct Scene;
struct SQT;

void DecodeFrame(
    Scene* scene,
    int animID,
    int frameID,
    std::vector<SQT>& frame);

void InterpolateFrames(
    Scene* scene,
    int animID,
    int frame1ID,
    int frame2ID,
    float alpha,
    std::vector<SQT>& frame);
