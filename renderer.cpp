#include "renderer.h"

#include "scene.h"

#include "imgui/imgui.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

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
    struct DrawCmd
    {
        int HasTransparency;
        int MaterialID;
        int NodeID;

        bool operator<(const DrawCmd& other) const
        {
            if (HasTransparency < other.HasTransparency) return true;
            if (other.HasTransparency < HasTransparency) return false;

            if (MaterialID < other.MaterialID) return true;
            if (other.MaterialID < MaterialID) return false;

            if (NodeID < other.NodeID) return true;
            if (other.NodeID < NodeID) return false;

            return false;
        }
    };

    // Encode draws to sort them
    std::vector<DrawCmd> draws(scene->SceneNodes.size());
    for (int nodeID = 0; nodeID < (int)scene->SceneNodes.size(); nodeID++)
    {
        const SceneNode& sceneNode = scene->SceneNodes[nodeID];

        DrawCmd cmd;

        if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
        {
            const SkinnedMeshSceneNode& skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
            const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
            const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[skinnedMesh.BindPoseMeshID];

            int materialID = bindPoseMesh.MaterialID;
            const Material& material = scene->Materials[materialID];

            int hasTransparency = false;
            for (int diffuseTextureIdx = 0; diffuseTextureIdx < (int)material.DiffuseTextureIDs.size(); diffuseTextureIdx++)
            {
                if (scene->DiffuseTextures[material.DiffuseTextureIDs[diffuseTextureIdx]].HasTransparency)
                {
                    hasTransparency = 1;
                    break;
                }
            }

            cmd.HasTransparency = hasTransparency;
            cmd.MaterialID = materialID;
            cmd.NodeID = nodeID;
        }
        else
        {
            fprintf(stderr, "Unknown scene node type %d\n", sceneNode.Type);
            exit(1);
        }

        draws[nodeID] = std::move(cmd);
    }

    std::sort(begin(draws), end(draws));

    int drawableWidth, drawableHeight;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    glm::mat4 projection = glm::perspective(70.0f, (float)drawableWidth / drawableHeight, 0.01f, 1000.0f);
    glm::mat4 worldView = glm::translate(glm::mat4(scene->CameraRotation), -scene->CameraPosition);
    glm::mat4 worldViewProjection = projection * worldView;

    // Scene rendering
    if (scene->AllShadersOK)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->BackbufferFBO);

        glViewport(0, 0, drawableWidth, drawableHeight);

        // Clear color is already SRGB encoded, so don't enable GL_FRAMEBUFFER_SRGB before it.
        glClearColor(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_DEPTH_TEST);

        glUseProgram(scene->SceneSP.Handle);
        glUniformMatrix4fv(scene->SceneSP_WorldViewLoc, 1, GL_FALSE, glm::value_ptr(worldView));
        glUniform1i(scene->SceneSP_DiffuseTextureLoc, 0);
        glUniform1i(scene->SceneSP_SpecularTextureLoc, 1);
        glUniform1i(scene->SceneSP_NormalTextureLoc, 2);
        glUniform3fv(scene->SceneSP_CameraPositionLoc, 1, glm::value_ptr(scene->CameraPosition));
        
        for (int drawIdx = 0; drawIdx < (int)draws.size(); drawIdx++)
        {
            DrawCmd cmd = draws[drawIdx];

            if (!cmd.HasTransparency)
            {
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LESS);
                glDisable(GL_BLEND);
                glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
                glUniform1i(scene->SceneSP_IlluminationModelLoc, 1);
            }
            else
            {
                glDepthMask(GL_FALSE);
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_BLEND);
                glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
                glUniform1i(scene->SceneSP_IlluminationModelLoc, 2);
            }

            int materialID = cmd.MaterialID;
            const Material& material = scene->Materials[materialID];

            // Set diffuse texture
            glActiveTexture(GL_TEXTURE0);
            if (material.DiffuseTextureIDs.size() < 1 || material.DiffuseTextureIDs[0] == -1)
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, scene->DiffuseTextures[material.DiffuseTextureIDs[0]].TO);
            }

            // Set specular texture
            glActiveTexture(GL_TEXTURE1);
            if (material.SpecularTextureIDs.size() < 1 || material.SpecularTextureIDs[0] == -1)
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, scene->SpecularTextures[material.SpecularTextureIDs[0]].TO);
            }

            // Set normal map texture
            glActiveTexture(GL_TEXTURE2);
            if (material.NormalTextureIDs.size() < 1 || material.NormalTextureIDs[0] == -1)
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, scene->NormalTextures[material.NormalTextureIDs[0]].TO);
            }

            const SceneNode& sceneNode = scene->SceneNodes[cmd.NodeID];

            // Draw node
            if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
            {
                const SkinnedMeshSceneNode& skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
                const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
                const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[skinnedMesh.BindPoseMeshID];

                glBindVertexArray(skinnedMesh.SkinnedVAO);

                glm::mat4 modelWorld = sceneNode.ModelWorldTransform;
                glm::mat4 modelView = worldView * modelWorld;
                glm::mat4 modelViewProjection = worldViewProjection * modelWorld;

                glUniformMatrix4fv(scene->SceneSP_ModelWorldLoc, 1, GL_FALSE, value_ptr(modelWorld));
                glUniformMatrix4fv(scene->SceneSP_WorldModelLoc, 1, GL_FALSE, value_ptr(glm::inverse(modelWorld)));
                glUniformMatrix4fv(scene->SceneSP_ModelViewLoc, 1, GL_FALSE, value_ptr(modelView));
                glUniformMatrix4fv(scene->SceneSP_ModelViewProjectionLoc, 1, GL_FALSE, value_ptr(modelViewProjection));
                
                glDrawElements(GL_TRIANGLES, bindPoseMesh.NumIndices, GL_UNSIGNED_INT, NULL);
            }
            else
            {
                fprintf(stderr, "Unknown scene node type %d\n", sceneNode.Type);
                exit(1);
            }
        }

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisable(GL_FRAMEBUFFER_SRGB);

        // Overlay spooky skeletons
        glClear(GL_DEPTH_BUFFER_BIT);

        glUseProgram(scene->SkeletonSP.Handle);
        glPointSize(3.0); // Make rendered joints visible

        for (const SceneNode& sceneNode : scene->SceneNodes)
        {
            if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
            {
                const SkinnedMeshSceneNode skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
                const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
                const AnimatedSkeleton& animatedSkeleton = scene->AnimatedSkeletons[skinnedMesh.AnimatedSkeletonID];
                const AnimSequence& animSequence = scene->AnimSequences[animatedSkeleton.CurrAnimSequenceID];
                const Skeleton& skeleton = scene->Skeletons[animSequence.SkeletonID];

                glBindVertexArray(animatedSkeleton.SkeletonVAO);

                const glm::mat4& modelWorld = sceneNode.ModelWorldTransform;
                glm::mat4 modelViewProjection = worldViewProjection * modelWorld;

                glUniformMatrix4fv(scene->SkeletonSP_ModelViewProjectionLoc, 1, GL_FALSE, value_ptr(modelViewProjection));

                // Draw white bones
                glUniform3fv(scene->SkeletonSP_ColorLoc, 1, value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
                glDrawElements(GL_LINES, skeleton.NumBoneIndices, GL_UNSIGNED_INT, NULL);

                // Draw green points at joints
                glUniform3fv(scene->SkeletonSP_ColorLoc, 1, value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
                glDrawArrays(GL_POINTS, 0, skeleton.NumBones);
            }
        }

        glBindVertexArray(0);
        glUseProgram(0);

        glDisable(GL_DEPTH_TEST);

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
