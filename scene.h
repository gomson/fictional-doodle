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

// Bitsets to say which components of the animation changes every frame
// For example if an object does not rotate then the Q (Quaternion) channels are turned off, which saves space.
// When a channel is not set in an animation sequence, then the baseFrame setting is used instead.
enum AnimChannel
{
    ANIMCHANNEL_TX_BIT = 1, // X component of translation
    ANIMCHANNEL_TY_BIT = 2, // Y component of translation
    ANIMCHANNEL_TZ_BIT = 4, // Z component of translation
    ANIMCHANNEL_QX_BIT = 8, // X component of quaternion
    ANIMCHANNEL_QY_BIT = 16, // Y component of quaternion
    ANIMCHANNEL_QZ_BIT = 32 // Z component of quaternion
    // There is no quaternion W, since the quaternions are normalized so it can be deduced from the x/y/z.
};

// Skeleton Table
// All unique static skeleton definitions.
struct Skeleton
{
    std::vector<std::string> BoneNames; // Name of each bone
    std::vector<glm::mat4> BoneInverseBindPoseTransforms; // Transforms a vertex from model space to bone space
    std::vector<int> BoneParents; // Bone parent index, or -1 if root
};

// BindPoseMesh Table
// All unique static bind pose meshes.
// Each bind pose mesh is compatible with one skeleton.
struct BindPoseMesh
{
    GLuint SkinningVAO; // Vertex arrays for input to skinning transform feedback
    GLuint PositionVBO; // vertices of meshes in bind pose
    GLuint TexCoordVBO; // texture coordinates of mesh
    GLuint DifferentialVBO; // differential geometry of bind pose mesh (n,t,b)
    GLuint BoneVBO; // bone IDs and bone weights of mesh
    GLuint EBO; // indices of mesh in bind pose 
    int NumIndices; // Number of indices in the bind pose
    int NumVertices; // Number of vertices in the bind pose
    int SkeletonID; // Skeleton used to skin this mesh
};

// AnimSequence Table
// All unique static animation sequences.
// Each animation sequence is compatible with one skeleton.
struct AnimSequence
{
    std::string Name; // The human readable name of each animation sequence
    std::vector<SQT> BoneBaseFrame; // The base frame for each bone, which defines the initial transform.
    std::vector<uint8_t> BoneChannelBits; // Which animation channels are present in frame data for each bone
    std::vector<int> BoneFrameDataOffsets; // The offset in floats in the frame data for this bone
    std::vector<float> BoneFrameData; // All frame data for each bone allocated according to the channel bits.
    int SkeletonID; // The skeleton that this animation sequence animates
    int FramesPerSecond; // Frames per second for each animation sequence
};

// SkinnedMesh Table
// All instances of skinned meshes in the scene.
// Each skinned mesh instance is associated to one bind pose, which is associated to one skeleton.
// Each skinned mesh instance is asssociated to a scene node.
// Each skinned mesh instance is associated to an animation sequence, which is associated to one skeleton.
// The skeleton of the bind pose and the skeleton of the animation sequence must be the same one.
struct SkinnedMesh
{
    GLuint BoneTransformTBO; // The matrices used to transform the bones
    GLuint BoneTransformTO; // Texture descriptor for the palette
    GLuint SkinningTFO; // Transform feedback for skinning
    GLuint SkinnedVAO; // Vertex array for rendering skinned meshes.
    GLuint PositionTFBO; // Positions created from transform feedback
    GLuint DifferentialTFBO; // Differential geometry from transform feedback
    int BindPoseMeshID; // The ID of the bind pose of this skinned mesh
    int CurrAnimSequenceID; // The currently playing animation sequence for each skinned mesh
    int CurrTimeMillisecond; // The current time in the current animation sequence in milliseconds
    std::vector<glm::mat4> CPUBoneTransforms; // Transforms a vertex in bone space.
    std::vector<BoneControlMode> BoneControls; // How each bone is animated
};

// Ragdoll Table
// All instances of ragdoll simulations in the scene.
// Each ragdoll simulation is associatd to one skinned mesh.
struct Ragdoll
{
    int BindPoseMeshID; // The skinned mesh that is being animated physically
    int OldBufferIndex; // Which of the 2 buffers contains old data
    std::vector<glm::vec3> BonePositions[2]; // Old and new positions of the bone
    std::vector<glm::vec3> BoneVelocities[2]; // Old and new velocities of the bone
};

// DiffuseTexture Table
struct DiffuseTexture
{
    GLuint TO; // Texture object
};

// SpecularTexture Table
struct SpecularTexture
{
    GLuint TO; // Texture object
};

// NormalTexture Table
struct NormalTexture
{
    GLuint TO; // Texture object
};

// Material Table
// Each material is associated to a DiffuseTexture (or -1)
struct Material
{
    std::vector<int> DiffuseTextureIDs; // Diffuse textures (if present)
    std::vector<int> SpecularTextureIDs; // Specular textures (if present)
    std::vector<int> NormalTextureIDs; // Normal textures (if present)
};

struct SkinnedMeshSceneNode
{
    int SkinnedMeshID;
};

// SceneNode Table
// Each node is associated to one material.
struct SceneNode
{
    glm::mat4 ModelWorldTransform; // The modelworld matrix to place the node in the world.
    int MaterialID; // The material to use to render the node.
    SceneNodeType Type; // What type of node this is.

    union
    {
        SkinnedMeshSceneNode AsSkinnedMesh;
    };
};

struct Scene
{
    std::vector<Skeleton> Skeletons;
    std::vector<BindPoseMesh> BindPoseMeshes;
    std::vector<AnimSequence> AnimSequences;
    std::vector<SkinnedMesh> SkinnedMeshes;
    std::vector<Ragdoll> Ragdolls;
    
    std::vector<DiffuseTexture> DiffuseTextures;
    std::vector<SpecularTexture> SpecularTextures;
    std::vector<NormalTexture> NormalTextures;
    std::unordered_map<std::string, int> DiffuseTextureNameToID;
    std::unordered_map<std::string, int> SpecularTextureNameToID;
    std::unordered_map<std::string, int> NormalTextureNameToID;

    std::vector<Material> Materials;
    std::vector<SceneNode> SceneNodes;
  
    // Skinning shader. Used to skin vertices on GPU, and outputs vertices in world space.
    // These vertices are stored using transform feedback and fed into the rendering later.
    ReloadableShader SkinningVS{ "skinning.vert" };
    ReloadableProgram SkinningSP = ReloadableProgram(&SkinningVS)
        .WithVaryings({"oPosition", "gl_NextBuffer", "oNormal", "oTangent", "oBitangent" }, GL_INTERLEAVED_ATTRIBS);
    GLint SkinningSP_BoneTransformsLoc;

    // Scene shader. Used to render objects in the scene which have their geometry defined in world space.
    ReloadableShader SceneVS{ "scene.vert" };
    ReloadableShader SceneFS{ "scene.frag" };
    ReloadableProgram SceneSP{ &SceneVS, &SceneFS };
    GLint SceneSP_ModelViewLoc;
    GLint SceneSP_ModelViewProjectionLoc;
    GLint SceneSP_WorldViewLoc;
    GLint SceneSP_DiffuseTextureLoc;
    GLint SceneSP_SpecularTextureLoc;
    GLint SceneSP_NormalTextureLoc;

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
