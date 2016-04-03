#include "profiler.h"

#include <cassert>
#include <cstdio>

void Increment(int& index, int cycle)
{
    index = (index + 1) % cycle;
}

void Decrement(int& index, int cycle)
{
    index = (cycle + index - 1) % cycle;
}

Profiler::Profiler()
    : CurrFrame(0)
    , GPUReadIndex(0)
    , GPUWriteIndex(0)
    , NumPushedGPUMarkers(0)
{
    glGenQueries(NUM_GPU_MARKERS, QueryIDs);
}

void Profiler::RecordFrame()
{
    CurrFrame++;

    // TODO: Record end time for previous frame and start time for current frame on CPU
}

void Profiler::PushGPUMarker(const char* name)
{
    // OS X doesn't support timestamps so we're limited to time elapsed with one marker
    assert(NumPushedGPUMarkers++ == 0);

    // Reinitialize marker
    GPUMarker& marker = GPUMarkers[GPUWriteIndex];
    marker.Frame = CurrFrame;
    marker.Name = name;
    marker.TimeElapsed = 0;

    glBeginQuery(GL_TIME_ELAPSED, QueryIDs[GPUWriteIndex]);
    Increment(GPUWriteIndex, NUM_GPU_MARKERS);
}

void Profiler::PopGPUMarker()
{
    assert(NumPushedGPUMarkers-- != 0);
    glEndQuery(GL_TIME_ELAPSED);
}

void Profiler::ReadFrame(std::vector<GPUMarker>& frameMarkers)
{
    int frame = CurrFrame - (NUM_BUFFERED_FRAMES - 1);

    while (GPUMarkers[GPUReadIndex].Frame == frame)
    {
        GLint resultAvailable = GL_FALSE;
        glGetQueryObjectiv(QueryIDs[GPUReadIndex], GL_QUERY_RESULT_AVAILABLE, &resultAvailable);

        if (resultAvailable == GL_TRUE)
        {
            GPUMarker& marker = GPUMarkers[GPUReadIndex];
            glGetQueryObjectuiv(QueryIDs[GPUReadIndex], GL_QUERY_RESULT, &marker.TimeElapsed);
            frameMarkers.push_back(marker);
        }

        Increment(GPUReadIndex, NUM_GPU_MARKERS);
    }
}
