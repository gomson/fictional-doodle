#include <SDL/SDL.h>
#include <GL/glcorearb.h>

#include <cstdio>
#include <cassert>
#include <type_traits>

PFNGLCLEARPROC glClear;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLENABLEPROC glEnable;
PFNGLDISABLEPROC glDisable;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;

void InitGL()
{
    // Get GL proc in a type safe way and assert its existence
    auto GetProc = [](auto& proc, const char* name)
    {
        proc = static_cast<std::remove_reference_t<decltype(proc)>>(SDL_GL_GetProcAddress(name));
        assert(proc);
    };

    GetProc(glClear, "glClear");
    GetProc(glClearColor, "glClearColor");
    GetProc(glEnable, "glEnable");
    GetProc(glDisable, "glDisable");
    GetProc(glBindFramebuffer, "glBindFramebuffer");
}

void PaintGL()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
        {
            exit(0);
        }
    }

    glClearColor(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_FRAMEBUFFER_SRGB);

    glDisable(GL_FRAMEBUFFER_SRGB);
}

extern "C"
int main(int argc, char *argv[])
{
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

    SDL_Window* window = SDL_CreateWindow("fictional-doodle", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
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

    // main loop
    for (;;)
    {
        PaintGL();

        // Bind 0 to the draw framebuffer before swapping the window, because otherwise in Mac OS X nothing will happen.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}