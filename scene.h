#pragma once

#include "opengl.h"

#include <glm/glm.hpp>

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

#define NUM_BONES_PER_VERTEX 4

struct SceneVertexBoneData
{
    GLuint  BoneIDs[NUM_BONES_PER_VERTEX];
    GLfloat Weights[NUM_BONES_PER_VERTEX];
};

struct Scene
{
    // Bones and weights for every vertex in scene
    std::vector<SceneVertexBoneData> VertexBones;
    // Transformation for every bone in scene.
    std::vector<glm::mat4> BoneTransforms;
    // Bone indices by bone name.
    std::unordered_map<std::string, GLuint> BoneIDs;

    // Animation stuff
    GLuint BoneVBO;

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
    GLuint SP;
    GLuint VS;
    GLuint FS;
    GLint ViewLoc;
    GLint MVLoc;
    GLint MVPLoc;
    GLint Diffuse0Loc;

    // Camera stuff
    glm::vec3 CameraPosition;
    glm::vec4 CameraQuaternion;
    glm::mat3 CameraRotation; // updated from quaternion every frame

    bool EnableCamera;
};

void InitScene(Scene* scene);

void UpdateScene(Scene* scene, uint32_t deltaTicks);