#pragma once

#include "opengl.h"

#include <string>
#include <vector>

struct GPUMarker
{
    uint32_t TimeElapsed; // Time elapsed in nanoseconds
    int Frame; // Frame number at which this marker was pushed
    std::string Name;

    GPUMarker() : Frame(0x80000000) { };
};

class Profiler
{
    static const size_t NUM_BUFFERED_FRAMES = 3; // Number of frames to buffer queries for before reading 
    static const size_t NUM_GPU_MARKERS_PER_FRAME = 5; // Max number of GPU markers that can be pushed per frame
    static const size_t NUM_GPU_MARKERS = NUM_BUFFERED_FRAMES * NUM_GPU_MARKERS_PER_FRAME; // GPU marker buffer size

    int CurrFrame; // Current frame number used to associate markers
    int GPUReadIndex; // Index of GPU marker to read from
    int GPUWriteIndex; // Index of GPU marker to write to

    GPUMarker GPUMarkers[NUM_GPU_MARKERS]; // Cyclic buffer for GPU markers
    GLuint QueryIDs[NUM_GPU_MARKERS]; // Cyclic buffer for start time query objects

    int NumPushedGPUMarkers; // Number of currently pushed GPU markers

public:
    Profiler();

    // Invoke at the start of each frame
    void RecordFrame();

    // GPU profiling
    void PushGPUMarker(const char* name);
    void PopGPUMarker();

    // Retrieve profiling markers from the earliest available frame
    void ReadFrame(std::vector<GPUMarker>& frameMarkers);
};
