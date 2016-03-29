#pragma once

#include "opengl.h"

#include <glm/glm.hpp>

struct SDL_Window;
struct Scene;

struct Renderer
{
    // Framebuffer stuff
    GLuint BackbufferFBO;
    GLuint BackbufferColorTexture;
    GLuint BackbufferDepthTexture;

    GLuint ShadowMapFBO;
    GLuint ShadowMapTexture;
    int ShadowMapSize;

    bool GUIFocusEnabled;
};

void InitRenderer(Renderer* renderer);

void ResizeRenderer(
    Renderer* renderer,
    int windowWidth, 
    int windowHeight, 
    int drawableWidth, 
    int drawableHeight, 
    int numSamples);

void PaintRenderer(
    Renderer* renderer,
    SDL_Window* window,
    Scene* scene);