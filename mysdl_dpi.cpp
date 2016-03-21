#include "mysdl_dpi.h"

// For Windows-specific code
#ifdef _WIN32
#define UNICODE 1
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <ShellScalingAPI.h>
#include <comdef.h>
#endif

#include <SDL.h>

void MySDL_SetDPIAwareness_MustBeFirstWSICallInProgram()
{
#ifdef _WIN32
{
    HMODULE ShcoreLib = LoadLibraryW(L"Shcore.dll");
    if (ShcoreLib != NULL)
    {
        typedef HRESULT(WINAPI * PFNSETPROCESSDPIAWARENESSPROC)(PROCESS_DPI_AWARENESS);
        PFNSETPROCESSDPIAWARENESSPROC pfnSetProcessDpiAwareness = (PFNSETPROCESSDPIAWARENESSPROC)GetProcAddress(ShcoreLib, "SetProcessDpiAwareness");
        if (pfnSetProcessDpiAwareness != NULL)
        {
            HRESULT hr = pfnSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
            if (FAILED(hr))
            {
                _com_error err(hr);
                fwprintf(stderr, L"SetProcessDpiAwareness failed: %s\n", err.ErrorMessage());
            }
        }
        FreeLibrary(ShcoreLib);
    }
}
#endif
}

void MySDL_GetDisplayDPI(int displayIndex, float* hdpi, float* vdpi, float* defaultDpi)
{
    static const float kSysDefaultDpi =
#ifdef __APPLE__
        72.0f;
#elif defined(_WIN32)
        96.0f;
#else
        static_assert(false, "No system default DPI set for this platform");
#endif

    if (SDL_GetDisplayDPI(displayIndex, NULL, hdpi, vdpi))
    {
        if (hdpi) *hdpi = kSysDefaultDpi;
        if (vdpi) *vdpi = kSysDefaultDpi;
    }

    if (defaultDpi) *defaultDpi = kSysDefaultDpi;
}