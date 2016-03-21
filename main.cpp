#include <SDL.h>
#include <GL/glcorearb.h>

#include "imgui/imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include "opengl.h"
#include "renderer.h"
#include "scene.h"
#include "mysdl_dpi.h"

#include <cstdio>
#include <functional>

extern "C"
int main(int argc, char *argv[])
{
    MySDL_SetDPIAwareness_MustBeFirstWSICallInProgram();

    if (SDL_Init(SDL_INIT_EVERYTHING))
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    // GL 4.1 for OS X support
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#ifdef _DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Enable multisampling
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    // Enable SRGB
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    // Don't need depth, it's done manually through the FBO.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    // Scale window accoridng to DPI zoom
    int windowDpiScaledWidth, windowDpiScaledHeight;
    {
        int windowDpiUnscaledWidth = 1280, windowDpiUnscaledHeight = 720;

        float hdpi, vdpi, defaultDpi;
        MySDL_GetDisplayDPI(0, &hdpi, &vdpi, &defaultDpi);

        windowDpiScaledWidth = int(windowDpiUnscaledWidth * hdpi / defaultDpi);
        windowDpiScaledHeight = int(windowDpiUnscaledHeight * vdpi / defaultDpi);
    }

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifdef _WIN32
    // highdpi doesn't work on Mac yet because we have to set the NSHighResolutionCapable Info.plist property
    windowFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif

    SDL_Window* window = SDL_CreateWindow(
        "fictional-doodle",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowDpiScaledWidth, windowDpiScaledHeight,
        windowFlags);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    if (!glctx)
    {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        exit(1);
    }

    InitGL();

    Renderer renderer{};
    InitRenderer(&renderer);

    ImGui_ImplSdlGL3_Init(window);

    // Initial resize to create framebuffers
    {
        int drawableWidth, drawableHeight;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        int numSamples;
        SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &numSamples);

        ResizeRenderer(&renderer,windowWidth, windowHeight, drawableWidth, drawableHeight, numSamples);
    }

    Scene scene{};
    InitScene(&scene);

    bool guiFocusEnabled = true;
    auto updateGuiFocus = [&] 
    {
        if (guiFocusEnabled)
        {
            SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
            SDL_SetRelativeMouseMode(SDL_FALSE);
            scene.EnableCamera = false;
        }
        else
        {
            // Warping mouse seems necessary to acquire mouse focus for OS X track pad.
            SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
            SDL_SetRelativeMouseMode(SDL_TRUE);

            // Prevent initial mouse warp state change from reorienting the camera.
            SDL_GetRelativeMouseState(NULL, NULL);
            scene.EnableCamera = true;
        }
    };

    updateGuiFocus();

    Uint32 lastTicks = SDL_GetTicks();

    // main loop
    for (;;)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (guiFocusEnabled)
            {
                ImGui_ImplSdlGL3_ProcessEvent(&ev);
            }

            if (ev.type == SDL_QUIT)
            {
                goto endmainloop;
            }
            else if (ev.type == SDL_WINDOWEVENT)
            {
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    int drawableWidth, drawableHeight;
                    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

                    int windowWidth, windowHeight;
                    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

                    int numSamples;
                    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &numSamples);

                    ResizeRenderer(&renderer, windowWidth, windowHeight, drawableWidth, drawableHeight, numSamples);
                }
            }
            else if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.keysym.sym == SDLK_ESCAPE)
                {
                    guiFocusEnabled = !guiFocusEnabled;
                    updateGuiFocus();
                }
                else if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    if (ev.key.keysym.mod & KMOD_ALT)
                    {
                        Uint32 wflags = SDL_GetWindowFlags(window);
                        if (wflags & SDL_WINDOW_FULLSCREEN_DESKTOP)
                        {
                            SDL_SetWindowFullscreen(window, 0);
                        }
                        else
                        {
                            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        }
                    }
                }
            }
        }

        ImGui_ImplSdlGL3_NewFrame(guiFocusEnabled);

        Uint32 currTicks = SDL_GetTicks();
        Uint32 deltaTicks = currTicks - lastTicks;

        UpdateScene(&scene, window, deltaTicks);
        PaintRenderer(&renderer, window, &scene);

        // Bind 0 to the draw framebuffer before swapping the window, because otherwise in Mac OS X nothing will happen.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        SDL_GL_SwapWindow(window);

        lastTicks = currTicks;
    }
    endmainloop:

    ImGui_ImplSdlGL3_Shutdown();
    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
