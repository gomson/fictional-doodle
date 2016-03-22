#include "renderer.h"

#include "scene.h"

#include "imgui/imgui.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>

void InitRenderer(Renderer* renderer)
{
}

void ResizeRenderer(
    Renderer* renderer,
    int windowWidth, 
    int windowHeight, 
    int drawableWidth, 
    int drawableHeight, 
    int numSamples)
{
    // Init rendertargets/depthstencils
    glDeleteTextures(1, &renderer->BackbufferColorTexture);
    glGenTextures(1, &renderer->BackbufferColorTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, renderer->BackbufferColorTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_SRGB8_ALPHA8, drawableWidth, drawableHeight, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    glDeleteTextures(1, &renderer->BackbufferDepthTexture);
    glGenTextures(1, &renderer->BackbufferDepthTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, renderer->BackbufferDepthTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_DEPTH_COMPONENT32F, drawableWidth, drawableHeight, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    // Init framebuffer
    glDeleteFramebuffers(1, &renderer->BackbufferFBO);
    glGenFramebuffers(1, &renderer->BackbufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->BackbufferFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderer->BackbufferColorTexture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, renderer->BackbufferDepthTexture, 0);
    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(sizeof(drawBufs) / sizeof(*drawBufs), &drawBufs[0]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Framebuffer status error: %s\n", FramebufferStatusToStringGL(fboStatus));
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PaintRenderer(
    Renderer* renderer, 
    SDL_Window* window, 
    Scene* scene)
{
    if (!scene->AllShadersOK)
    {
        return;
    }

    int drawableWidth, drawableHeight;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    glm::mat4 projection = glm::perspective(70.0f, (float)drawableWidth / drawableHeight, 0.01f, 1000.0f);
    glm::mat4 worldView = glm::translate(glm::mat4(scene->CameraRotation), -scene->CameraPosition);
    glm::mat4 worldViewProjection = projection * worldView;

    // Scene rendering
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->BackbufferFBO);

        glViewport(0, 0, drawableWidth, drawableHeight);

        // Clear color is already SRGB encoded, so don't enable GL_FRAMEBUFFER_SRGB before it.
        glClearColor(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_DEPTH_TEST);

        glUseProgram(scene->SceneSP.Handle);
        glUniform1i(scene->SceneSP_Diffuse0Loc, 0);
        glUniformMatrix4fv(scene->SceneSP_WorldViewLoc, 1, GL_FALSE, glm::value_ptr(worldView));
        glUniformMatrix4fv(scene->SceneSP_WorldViewProjectionLoc, 1, GL_FALSE, glm::value_ptr(worldViewProjection));

        for (int drawIdx = 0; drawIdx < (int)scene->NodeDrawCmds.size(); drawIdx++)
        {
            GLDrawElementsIndirectCommand cmd = scene->NodeDrawCmds[drawIdx];
            assert(cmd.baseInstance == 0); // no base instance because OS X
            assert(cmd.primCount == 1); // assuming no instancing cuz lack of baseInstance makes it boring

            SceneNodeType type = scene->NodeTypes[drawIdx];

            if (type == SCENENODETYPE_SKINNEDMESH)
            {
                glBindVertexArray(scene->SkinnedMeshVAO);
            }
            else
            {
                assert(false && "Unknown scene node type");
            }

            int materialID = scene->NodeMaterialIDs[drawIdx];
            int diffuseTexture0Index = scene->MaterialDiffuse0TextureIndex[materialID];
            glActiveTexture(GL_TEXTURE0);
            if (diffuseTexture0Index == -1)
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, scene->DiffuseTextures[diffuseTexture0Index]);
            }

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT, (GLvoid*)(cmd.firstIndex * sizeof(GLuint)), cmd.primCount, cmd.baseVertex);
        }

        glBindVertexArray(0);
        glUseProgram(0);

        glDisable(GL_DEPTH_TEST);

        glDisable(GL_FRAMEBUFFER_SRGB);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // GUI rendering
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->BackbufferFBO);
        glEnable(GL_FRAMEBUFFER_SRGB);
        ImGui::Render();
        glDisable(GL_FRAMEBUFFER_SRGB);
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->BackbufferFBO);
    }

    // Draw to window's framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->BackbufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO
    glBlitFramebuffer(
        0, 0, drawableWidth, drawableHeight,
        0, 0, drawableWidth, drawableHeight,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
