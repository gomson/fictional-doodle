#pragma once

#include "opengl.h"
#include "shaderreloader.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#include <string>
#include <vector>
#include <unordered_map>

struct SDL_Window;

struct PositionVertex
{
    glm::vec3 Position;
};

struct TexCoordVertex
{
    glm::vec2 TexCoord0;
};

struct DifferentialVertex
{
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

struct BoneWeightVertex
{
    glm::u8vec4 BoneIDs;
    glm::vec4 Weights;
};

// Each bone is either controlled by skinned animation or the physics simulation (ragdoll)
enum BoneControlMode
{
    BONECONTROL_ANIMATION,
    BONECONTROL_DYNAMICS
};

// Different types of scene nodes need to be rendered slightly differently.
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
    // Per-mesh skinning data. Shared by many instances of animated meshes.
    std::vector<GLuint> BindPoseMesh_SkinningVAO; // Vertex arrays for input to skinning transform feedback
    std::vector<GLuint> BindPoseMesh_PositionsVBO; // vertices of meshes in bind pose
    std::vector<GLuint> BindPoseMesh_TexCoordsVBO; // texture coordinates of mesh
    std::vector<GLuint> BindPoseMesh_DifferentialsVBO; // differential geometry of bind pose mesh (n,t,b)
    std::vector<GLuint> BindPoseMesh_BonesVBO; // bone IDs and bone weights of mesh
    std::vector<GLuint> BindPoseMesh_EBO; // indices of mesh in bind pose
    std::vector<std::unordered_map<std::string,int>> BindPoseMesh_BoneNameToID; // Lookup bone IDs from their name
    std::vector<std::vector<glm::mat4>> BindPoseMesh_BoneInverseBindPoseTransforms; // Transforms a vertex from model space to bone space
    std::vector<std::vector<int>> BindPoseMesh_BoneParents; // Bone parent index, or -1 if root
    std::vector<int> BindBoseMesh_NumVertices; // Number of vertices in each bind pose mesh

    // Per-object skinning data. These arrays store the data for every skinned mesh instance in the scene.
    std::vector<GLuint> SkinnedMesh_BoneTransformsTBO; // The matrices used to transform the bones
    std::vector<GLuint> SkinnedMesh_BoneTransformsTexture; // Texture descriptor for the palette
    std::vector<std::vector<glm::mat4>> SkinnedMesh_CPUBoneTransforms; // Transforms a vertex in bone space.
    std::vector<GLuint> SkinnedMesh_TFO; // Transform feedback for skinning
    std::vector<GLuint> SkinnedMesh_VAO; // Vertex array for rendering skinned meshes.
    std::vector<GLuint> SkinnedMesh_PositionTFBO; // Positions created from transform feedback
    std::vector<GLuint> SkinnedMesh_DifferentialTFBO; // Differential geometry from transform feedback
    std::vector<int> SkinnedMesh_BindPoseIDs; // The index of the bind pose of this skinned mesh
    std::vector<int> SkinnedMesh_NodeIDs; // The scene node ID corresponding to the skinned object
    std::vector<int> SkinnedMesh_CurrAnimSequenceIDs; // The currently playing animation sequence for each skinned mesh
    std::vector<int> SkinnedMesh_CurrTimeMilliseconds; // The current time in the current animation sequence in milliseconds
    std::vector<std::vector<BoneControlMode>> SkinnedMesh_BoneControls; // How each bone is animated

    // Animation sequence data. Lists of all animation sequences in use.
    std::vector<std::string> AnimSequence_Names; // The human readable name of each animation sequence
    std::vector<std::vector<SQT>> AnimSequence_BoneBaseFrames; // The base frame for each bone, which defines the initial transform.
    std::vector<std::vector<uint8_t>> AnimSequence_BoneChannelBits; // Which animation channels are present in frame data for each bone
    std::vector<std::vector<int>> AnimSequence_BoneFrameDataOffset; // The offset in floats in the frame data for this bone
    std::vector<std::vector<std::vector<float>>> AnimSequence_FrameData; // Data for each frame allocated according to the channel bits.
    std::vector<int> AnimSequence_FramesPerSecond; // Frames per second for each animation sequence

    // Bitsets to say which components of the animation changes every frame
    // For example if an object does not rotate then the Q (Quaternion) channels are turned off, which saves space.
    // When a channel is not set in an animation sequence, then the baseFrame setting is used instead.
    static const int ANIM_CHANNEL_TX_BIT = 1; // X component of translation
    static const int ANIM_CHANNEL_TY_BIT = 2; // Y component of translation
    static const int ANIM_CHANNEL_TZ_BIT = 4; // Z component of translation
    static const int ANIM_CHANNEL_QX_BIT = 8; // X component of quaternion
    static const int ANIM_CHANNEL_QY_BIT = 16; // Y component of quaternion
    static const int ANIM_CHANNEL_QZ_BIT = 32; // Z component of quaternion
    // There is no quaternion W, since the quaternions are normalized so it can be deduced from the x/y/z.

    // Positions/velocities of the last update for dynamics animation
    // The backbuffer is written every frame with updated positions based on the dynamics simulation.
    // The frontbuffer is read every frame and considered as the "old position" and "old velocity"
    int BoneDynamicsBackBufferIndex;
    static const int NUM_BONE_DYNAMICS_BUFFERS = 2;
    std::vector<std::vector<glm::vec3>> SkinnedMesh_BoneDynamicsPositions[NUM_BONE_DYNAMICS_BUFFERS];
    std::vector<std::vector<glm::vec3>> SkinnedMesh_BoneDynamicsVelocities[NUM_BONE_DYNAMICS_BUFFERS];

    // Materials information
    std::vector<GLuint> DiffuseTextures; // All diffuse textures in the scene
    std::vector<int> Material_Diffuse0TextureID; // Diffuse texture to use for texcoord 0. -1 if there is not present for this material.

    // The nodes of the "scenegraph". The renderer reads these to render the scene.
    std::vector<GLDrawElementsIndirectCommand> Node_DrawCommands; // The draw arguments to draw the node
    std::vector<glm::mat4> Node_ModelWorldTransforms; // The modelworld matrix to place the node in the world.
    std::vector<int> Node_MaterialIDs; // The material to use to render the node.
    std::vector<SceneNodeType> Node_Types; // For each node in the scene, what type of node it is.

    // Skinning shader. Used to skin vertices on GPU, and outputs vertices in world space.
    // These vertices are stored using transform feedback and fed into the rendering later.
    ReloadableShader SkinningVS{ "skinning.vert" };
    ReloadableProgram SkinningSP = ReloadableProgram(&SkinningVS)
        .WithVaryings({"oPosition", "gl_NextBuffer", "oNormal", "oTangent", "oBitangent" }, GL_INTERLEAVED_ATTRIBS);
    GLint SkinningSP_ModelWorldLoc;
    GLint SkinningSP_BoneTransformsLoc;
    GLint SkinningSP_BoneOffsetLoc;

    // Scene shader. Used to render objects in the scene which have their geometry defined in world space.
    ReloadableShader SceneVS{ "scene.vert" };
    ReloadableShader SceneFS{ "scene.frag" };
    ReloadableProgram SceneSP{ &SceneVS, &SceneFS };
    GLint SceneSP_WorldViewLoc;
    GLint SceneSP_WorldViewProjectionLoc;
    GLint SceneSP_Diffuse0Loc;

    // true if all shaders in the scene are compiling/linking successfully.
    // Scene updates will stop if not all shaders are working, since it will likely crash.
    bool AllShadersOK;

    // Camera placement, updated each frame.
    glm::vec3 CameraPosition;
    glm::vec4 CameraQuaternion;
    glm::mat3 CameraRotation; // updated from quaternion every frame

    // The camera only reads user input when it is enabled.
    // Needed to implement menu navigation without the camera moving due to mouse/keyboard action.
    bool EnableCamera;
};

void InitScene(Scene* scene);

void UpdateScene(Scene* scene, SDL_Window* window, uint32_t deltaMilliseconds);
