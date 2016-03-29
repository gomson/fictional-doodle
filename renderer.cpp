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
    renderer->ShadowMapSize = 4096;

    glGenTextures(1, &renderer->ShadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, renderer->ShadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, renderer->ShadowMapSize, renderer->ShadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GLfloat shadowBorderColor[] = { INFINITY, INFINITY, INFINITY, INFINITY };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, shadowBorderColor);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &renderer->ShadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->ShadowMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, renderer->ShadowMapTexture, 0);
    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Shadow map status error: %s\n", FramebufferStatusToStringGL(fboStatus));
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ResizeRenderer(
    Renderer* renderer,
    int windowWidth, 
    int windowHeight, 
    int drawableWidth, 
    int drawableHeight, 
    int numSamples)
{
    glFinish();

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
    glm::mat4 worldView = glm::translate(glm::mat4(scene->CameraRotation), -scene->CameraPosition);

    glm::vec3 lightPosition = glm::vec3(0.0f, 300.0f, 100.0f);
    glm::mat4 worldLight = glm::lookAt(lightPosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProjection = glm::ortho(-1000.0f, 1000.0f, -1000.0f, 1000.0f, -1000.0f, 1000.0f);
    glm::mat4 worldLightProjection = lightProjection * worldLight;

    struct DrawCmd
    {
        int HasTransparency;
        float ViewDepth;
        int MaterialID;
        int NodeID;

        bool operator<(const DrawCmd& other) const
        {
            if (HasTransparency < other.HasTransparency) return true;
            if (other.HasTransparency < HasTransparency) return false;

            // Nearer objects are drawn first, since they hide further objects
            // Recall that GL view depth is along the negative Z direction,
            // so nearer objects have a greater Z.
            if (ViewDepth > other.ViewDepth) return true;
            if (other.ViewDepth > ViewDepth) return false;

            if (MaterialID < other.MaterialID) return true;
            if (other.MaterialID < MaterialID) return false;

            if (NodeID < other.NodeID) return true;
            if (other.NodeID < NodeID) return false;

            return false;
        }
    };

    // Produce list of draws to sort them in a good order
    std::vector<DrawCmd> draws;
    std::vector<DrawCmd> shadowDraws;
    for (int nodeID = 0; nodeID < (int)scene->SceneNodes.size(); nodeID++)
    {
        const SceneNode& sceneNode = scene->SceneNodes[nodeID];

        if (sceneNode.Type == SCENENODETYPE_TRANSFORM)
        {
            // These nodes don't draw anything
        }
        else if (sceneNode.Type == SCENENODETYPE_STATICMESH || sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
        {
            int materialID = -1;
            if (sceneNode.Type == SCENENODETYPE_STATICMESH)
            {
                const StaticMesh& staticMesh = scene->StaticMeshes[sceneNode.AsStaticMesh.StaticMeshID];
                materialID = staticMesh.MaterialID;
            }
            else if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
            {
                const SkinnedMeshSceneNode& skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
                const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
                const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[skinnedMesh.BindPoseMeshID];
                materialID = bindPoseMesh.MaterialID;
            }
            else
            {
                fprintf(stderr, "Unknown scene node type %d\n", sceneNode.Type);
                exit(1);
            }
            
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

            float viewDepth = (worldView * sceneNode.WorldTransform * glm::vec4(0, 0, 0, 1)).z;

            DrawCmd cmd;
            cmd.HasTransparency = hasTransparency;
            cmd.ViewDepth = viewDepth;
            cmd.MaterialID = materialID;
            cmd.NodeID = nodeID;
            draws.push_back(cmd);

            if (!hasTransparency)
            {
                float lightDepth = (worldLight * sceneNode.WorldTransform * glm::vec4(0, 0, 0, 1)).z;
                
                DrawCmd shadowCmd;
                shadowCmd.HasTransparency = false;
                shadowCmd.ViewDepth = lightDepth;
                shadowCmd.MaterialID = materialID;
                shadowCmd.NodeID = nodeID;
                shadowDraws.push_back(shadowCmd);
            }
        }
        else
        {
            fprintf(stderr, "Unknown scene node type %d\n", sceneNode.Type);
            exit(1);
        }
    }

    std::sort(begin(draws), end(draws));
    std::sort(begin(shadowDraws), end(shadowDraws));

    int drawableWidth, drawableHeight;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    // Shadow rendering
    if (scene->AllShadersOK)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->ShadowMapFBO);
        glViewport(0, 0, renderer->ShadowMapSize, renderer->ShadowMapSize);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(10.0f, 5.0f);
        glUseProgram(scene->ShadowSP.Handle);

        for (int drawIdx = 0; drawIdx < (int)shadowDraws.size(); drawIdx++)
        {
            const DrawCmd& cmd = shadowDraws[drawIdx];

            const SceneNode& sceneNode = scene->SceneNodes[cmd.NodeID];

            // Draw node
            if (sceneNode.Type == SCENENODETYPE_STATICMESH || sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
            {
                GLuint vao;
                int numIndices;

                if (sceneNode.Type == SCENENODETYPE_STATICMESH)
                {
                    const StaticMesh& staticMesh = scene->StaticMeshes[sceneNode.AsStaticMesh.StaticMeshID];
                    vao = staticMesh.MeshVAO;
                    numIndices = staticMesh.NumIndices;
                }
                else if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
                {
                    const SkinnedMeshSceneNode& skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
                    const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
                    const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[skinnedMesh.BindPoseMeshID];
                    vao = skinnedMesh.SkinnedVAO;
                    numIndices = bindPoseMesh.NumIndices;
                }
                else
                {
                    fprintf(stderr, "Unhandled scene node type %d\n", sceneNode.Type);
                    exit(1);
                }

                glBindVertexArray(vao);

                glm::mat4 modelWorld = sceneNode.WorldTransform;
                glm::mat4 modelView = worldView * modelWorld;
                glm::mat4 modelViewProjection = worldLightProjection * modelWorld;

                glUniformMatrix4fv(scene->ShadowSP_ModelLightProjectionLoc, 1, GL_FALSE, value_ptr(modelViewProjection));

                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, NULL);

                glBindVertexArray(0);
            }
        }

        glUseProgram(0);
        glPolygonOffset(0.0f, 0.0f);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_DEPTH_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Scene rendering
    if (scene->AllShadersOK)
    {
        glm::mat4 projection = glm::perspective(70.0f, (float)drawableWidth / drawableHeight, 0.01f, 1000.0f);
        glm::mat4 worldViewProjection = projection * worldView;

        glBindFramebuffer(GL_FRAMEBUFFER, renderer->BackbufferFBO);

        glViewport(0, 0, drawableWidth, drawableHeight);

        glEnable(GL_FRAMEBUFFER_SRGB);
        glClearColor(scene->BackgroundColor.r, scene->BackgroundColor.g, scene->BackgroundColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int drawIdx = 0; drawIdx < (int)draws.size(); drawIdx++)
        {
            const DrawCmd& cmd = draws[drawIdx];
            const SceneNode& sceneNode = scene->SceneNodes[cmd.NodeID];

            // Draw node
            if (sceneNode.Type == SCENENODETYPE_STATICMESH || sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
            {
                glUseProgram(scene->SceneSP.Handle);
                glUniformMatrix4fv(scene->SceneSP_WorldViewLoc, 1, GL_FALSE, value_ptr(worldView));
                glUniform1i(scene->SceneSP_DiffuseTextureLoc, 0);
                glUniform1i(scene->SceneSP_SpecularTextureLoc, 1);
                glUniform1i(scene->SceneSP_NormalTextureLoc, 2);
                glUniform1i(scene->SceneSP_ShadowMapTextureLoc, 3);
                glUniform3fv(scene->SceneSP_CameraPositionLoc, 1, value_ptr(scene->CameraPosition));
                glUniform3fv(scene->SceneSP_LightPositionLoc, 1, value_ptr(lightPosition));
                glUniform3fv(scene->SceneSP_BackgroundColorLoc, 1, value_ptr(scene->BackgroundColor));

                glEnable(GL_DEPTH_TEST);

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
                    glUniform1i(scene->SceneSP_HasNormalMapLoc, 0);
                }
                else
                {
                    glBindTexture(GL_TEXTURE_2D, scene->NormalTextures[material.NormalTextureIDs[0]].TO);
                    glUniform1i(scene->SceneSP_HasNormalMapLoc, 1);
                }

                // Set shadow map texture
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_2D, renderer->ShadowMapTexture);

                GLuint vao;
                int numIndices;

                if (sceneNode.Type == SCENENODETYPE_STATICMESH)
                {
                    const StaticMesh& staticMesh = scene->StaticMeshes[sceneNode.AsStaticMesh.StaticMeshID];
                    vao = staticMesh.MeshVAO;
                    numIndices = staticMesh.NumIndices;
                }
                else if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH)
                {
                    const SkinnedMeshSceneNode& skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
                    const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
                    const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[skinnedMesh.BindPoseMeshID];
                    vao = skinnedMesh.SkinnedVAO;
                    numIndices = bindPoseMesh.NumIndices;
                }
                else
                {
                    fprintf(stderr, "Unhandled scene node type %d\n", sceneNode.Type);
                    exit(1);
                }

                glBindVertexArray(vao);

                glm::mat4 modelWorld = sceneNode.WorldTransform;
                glm::mat4 modelView = worldView * modelWorld;
                glm::mat4 modelViewProjection = worldViewProjection * modelWorld;

                glUniformMatrix4fv(scene->SceneSP_ModelWorldLoc, 1, GL_FALSE, value_ptr(modelWorld));
                glUniformMatrix4fv(scene->SceneSP_WorldModelLoc, 1, GL_FALSE, value_ptr(glm::inverse(modelWorld)));
                glUniformMatrix4fv(scene->SceneSP_ModelViewLoc, 1, GL_FALSE, value_ptr(modelView));
                glUniformMatrix4fv(scene->SceneSP_ModelViewProjectionLoc, 1, GL_FALSE, value_ptr(modelViewProjection));
                glUniformMatrix4fv(scene->SceneSP_WorldLightProjectionLoc, 1, GL_FALSE, value_ptr(worldLightProjection));

                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, NULL);

                // Restore default state
                glDepthMask(GL_TRUE);
                glDepthFunc(GL_LESS);
                glDisable(GL_BLEND);
                glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);

                glBindVertexArray(0);
                glUseProgram(0);

                if (sceneNode.Type == SCENENODETYPE_SKINNEDMESH && scene->ShowSkeletons)
                {
                    const SkinnedMeshSceneNode& skinnedMeshSceneNode = sceneNode.AsSkinnedMesh;
                    const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshSceneNode.SkinnedMeshID];
                    const AnimatedSkeleton& animatedSkeleton = scene->AnimatedSkeletons[skinnedMesh.AnimatedSkeletonID];
                    const AnimSequence& animSequence = scene->AnimSequences[animatedSkeleton.CurrAnimSequenceID];
                    const Skeleton& skeleton = scene->Skeletons[animSequence.SkeletonID];

                    glUseProgram(scene->SkeletonSP.Handle);
                    glDisable(GL_DEPTH_TEST);

                    glBindVertexArray(animatedSkeleton.SkeletonVAO);
                    glPointSize(3.0f); // Make rendered joints visible

                    glUniformMatrix4fv(scene->SkeletonSP_ModelViewProjectionLoc, 1, GL_FALSE, value_ptr(modelViewProjection));

                    // Draw white bones
                    glUniform3fv(scene->SkeletonSP_ColorLoc, 1, value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
                    glDrawElements(GL_LINES, skeleton.NumBoneIndices, GL_UNSIGNED_INT, NULL);

                    // Draw green points at joints
                    glUniform3fv(scene->SkeletonSP_ColorLoc, 1, value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));
                    glDrawArrays(GL_POINTS, 0, skeleton.NumBones);

                    glPointSize(1.0f);

                    glBindVertexArray(0);
                    glUseProgram(0);
                }
            }
        }

        glDisable(GL_FRAMEBUFFER_SRGB);

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
