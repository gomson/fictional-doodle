#pragma once

// DPI awareness must be set before any other Window API calls. SDL doesn't do it for some reason?
void MySDL_SetDPIAwareness_MustBeFirstWSICallInProgram();

// If SDL fails to get DPI (for some reason this happens on OS X), then the system-specific default DPI is returned.
void MySDL_GetDisplayDPI(int displayIndex, float* hdpi, float* vdpi, float* defaultDpi);