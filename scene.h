#pragma once

#include "opengl.h"
#include "shaderreloader.h"
#include "skinning.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <unordered_map>

struct SceneVertex
{
    glm::vec3 Position;
    glm::vec2 TexCoord0;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

enum BoneControlMode
{
    BONECONTROL_ANIMATION,
    BONECONTROL_DYNAMICS
};

struct Scene
{
    // Bone weights per vertex.
    std::vector<VertexWeights> VertexBones;

    // Bone indices by name.
    std::unordered_map<std::string, GLubyte> BoneIDs;

    // Transforms a vertex from model space to bone space.
    std::vector<glm::mat4> BoneInverseBindPoseTransforms;

    // Transforms a vertex in bone space.
    std::vector<glm::mat4> BoneSkinningTransforms;

    // How the bone is animated
    std::vector<BoneControlMode> BoneControls;

    // Positions/velocities of the last update for dynamics animation
    int BoneDynamicsBackBufferIndex;
    static const int NUM_BONE_DYNAMICS_BUFFERS = 2;
    std::vector<glm::vec3> BoneDynamicsPositions[NUM_BONE_DYNAMICS_BUFFERS];
    std::vector<glm::vec3> BoneDynamicsVelocities[NUM_BONE_DYNAMICS_BUFFERS];

    // Animation stuff
    GLuint BoneVBO;
    GLuint BoneTBO;
    GLuint BoneTex;

    // Geometry stuff
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;

    // All unique diffuse textures in the scene
    std::vector<GLuint> DiffuseTextures;

    // For each material, gives the index in the DiffuseTextures array for diffuse texture 0. -1 if there is no diffuse texture 0 for this material.
    std::vector<int> MaterialDiffuse0TextureIndex;

    // For each node in the scene, the draw arguments to draw it.
    std::vector<GLDrawElementsIndirectCommand> NodeDrawCmds;
    // For each node in the scene, the modelworld matrix.
    std::vector<glm::mat4> NodeModelWorldTransforms;
    // For each node in the scene, the material ID for it.
    std::vector<int> NodeMaterialIDs;

    // Shader stuff
    ReloadableShader SceneVS{ "scene.vert" };
    ReloadableShader SceneFS{ "scene.frag" };
    ReloadableProgram SceneSP{ &SceneVS, &SceneFS };
    GLint SceneSP_ViewLoc;
    GLint SceneSP_MVLoc;
    GLint SceneSP_MVPLoc;
    GLint SceneSP_Diffuse0Loc;
    GLint SceneSP_BoneTransformsLoc;

    bool AllShadersOK;

    // Camera stuff
    glm::vec3 CameraPosition;
    glm::vec4 CameraQuaternion;
    glm::mat3 CameraRotation; // updated from quaternion every frame

    bool EnableCamera;
};

void InitScene(Scene* scene);

void UpdateScene(Scene* scene, uint32_t deltaMilliseconds);
