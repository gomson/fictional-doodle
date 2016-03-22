#pragma once

#include "opengl.h"
#include "shaderreloader.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#include <string>
#include <vector>
#include <unordered_map>

struct SDL_Window;

struct SkinningVertex
{
    glm::vec3 Position;
    glm::vec2 TexCoord0;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    glm::u8vec4 BoneIDs;
    glm::vec4 Weights;
};

struct SkinnedVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

enum BoneControlMode
{
    BONECONTROL_ANIMATION,
    BONECONTROL_DYNAMICS
};

enum SceneNodeType
{
    SCENENODETYPE_SKINNEDMESH
};

struct SQT
{
    // No S, lol. md5 doesn't use scale.
    glm::vec3 T;
    glm::quat Q;
};

struct Scene
{
    // Bone indices by name.
    std::unordered_map<std::string, int> BoneIDs;
    // Transforms a vertex from model space to bone space.
    std::vector<glm::mat4> BoneInverseBindPoseTransforms;
    // Transforms a vertex in bone space.
    std::vector<glm::mat4> BoneSkinningTransforms;
    // Bone parent index, or -1 if root
#pragma message("BoneParent not yet implemented")
    std::vector<int> BoneParent;
    // How the bone is animated
    std::vector<BoneControlMode> BoneControls;

    // The human readable name of each animation sequence
    std::vector<std::string> AnimSequenceNames;
    // For each animation sequence, the base frame for each bone
    std::vector<std::vector<SQT>> AnimSequenceBoneBaseFrames;
    // For each animation sequence, which animation channels are present in frame data for each bone
    std::vector<std::vector<uint8_t>> AnimSequenceBoneChannelBits;
    // For each animation sequence, the offset in floats in the frame data for this bone
    std::vector<std::vector<int>> AnimSequenceBoneFrameDataOffset;
    // For each animation sequence, data for each frame according to the channel bits
    // If a channel bit is not set for a channel, then that data is not present in the frame.
    // In that case, the baseFrame setting is used instead.
    std::vector<std::vector<std::vector<float>>> AnimSequenceFrameData;

    static const int ANIM_CHANNEL_TX_BIT = 1;
    static const int ANIM_CHANNEL_TY_BIT = 2;
    static const int ANIM_CHANNEL_TZ_BIT = 4;
    static const int ANIM_CHANNEL_QX_BIT = 8;
    static const int ANIM_CHANNEL_QY_BIT = 16;
    static const int ANIM_CHANNEL_QZ_BIT = 32;
    
    // The currently playing animation sequence
    int CurrAnimSequence;

    // Positions/velocities of the last update for dynamics animation
    int BoneDynamicsBackBufferIndex;
    static const int NUM_BONE_DYNAMICS_BUFFERS = 2;
    std::vector<glm::vec3> BoneDynamicsPositions[NUM_BONE_DYNAMICS_BUFFERS];
    std::vector<glm::vec3> BoneDynamicsVelocities[NUM_BONE_DYNAMICS_BUFFERS];

    // Per-mesh skinning data. Shared by many instances of animated meshes.
    GLuint SkinningVAO; // Vertex arrays for input to skinning transform feedback
    GLuint BindPoseMeshVBO; // vertices of meshes in bind pose
    GLuint BindPoseMeshEBO; // indices of mesh in bind pose
    std::vector<int> BindBoseMeshNumVertices; // Number of vertices in each bind pose mesh
    
    // Per-object skinning data. These arrays store the data for every skinned mesh instance in the scene.
    GLuint SkinningMatrixPaletteTBO; // The matrices used to transform the bones
    GLuint SkinningMatrixPaletteTexture; // Texture descriptor for the palette
    GLuint SkinningTFO; // Transform feedback for skinning
    GLuint SkinningTFBO; // Output of skinning transform feedback
    GLuint SkinnedMeshVAO; // Vertex array for rendering skinned meshes.
    std::vector<int> SkinnedMeshBindPoseIDs; // The index of the bind pose of this skinned mesh
    std::vector<GLDrawElementsIndirectCommand> SkinnedMeshDrawCommands; // Draw commands for skinned meshes
    std::vector<int> SkinnedMeshBaseBones; // The index of the first bone in this mesh
    std::vector<int> SkinnedMeshNodeIDs; // The scene node ID corresponding to the skinned object

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
    // For each node in the scene, what type of node it is
    std::vector<SceneNodeType> NodeTypes;

    // Skinning shader
    ReloadableShader SkinningVS{ "skinning.vert" };
    ReloadableProgram SkinningSP{ &SkinningVS, { "oPosition", "oNormal", "oTangent", "oBitangent" } };
    GLint SkinningSP_ModelWorldLoc;
    GLint SkinningSP_BoneTransformsLoc;
    GLint SkinningSP_BoneOffsetLoc;

    // Scene shader
    ReloadableShader SceneVS{ "scene.vert" };
    ReloadableShader SceneFS{ "scene.frag" };
    ReloadableProgram SceneSP{ &SceneVS, &SceneFS };
    GLint SceneSP_WorldViewLoc;
    GLint SceneSP_WorldViewProjectionLoc;
    GLint SceneSP_Diffuse0Loc;

    bool AllShadersOK;

    // Camera stuff
    glm::vec3 CameraPosition;
    glm::vec4 CameraQuaternion;
    glm::mat3 CameraRotation; // updated from quaternion every frame

    bool EnableCamera;
};

void InitScene(Scene* scene);

void UpdateScene(Scene* scene, SDL_Window* window, uint32_t deltaMilliseconds);
