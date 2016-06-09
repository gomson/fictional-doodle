#pragma once

#include "opengl.h"
#include "shaderreloader.h"
#include "dynamics.h"
#include "profiler.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/dual_quaternion.hpp>

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
    glm::vec2 TexCoord;
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
    SCENENODETYPE_TRANSFORM,
    SCENENODETYPE_STATICMESH,
    SCENENODETYPE_SKINNEDMESH
};

struct SQT
{
    // No S, lol. md5 doesn't use scale.
    glm::vec3 T;
    glm::quat Q;
};

// Used for array indices, don't change!
enum SkinningMethod
{
    SKINNING_DLB, // Dual quaternion linear blending
    SKINNING_LBS  // Linear blend skinning
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

// StaticMesh Table
// All unique static meshes.
struct StaticMesh
{
    GLuint MeshVAO; // vertex array for drawing the mesh
    GLuint PositionVBO; // position only buffer
    GLuint TexCoordVBO; // texture coordinates buffer
    GLuint DifferentialVBO; // differential geometry buffer (t,n,b)
    GLuint MeshEBO; // Index buffer for the mesh
    int NumIndices; // Number of indices in the static mesh
    int NumVertices; // Number of vertices in the static mesh
    int MaterialID; // The material this mesh was designed for
};

// Skeleton Table
// All unique static skeleton definitions.
struct Skeleton
{
    GLuint BoneEBO; // Indices of bones used for rendering the skeleton
    glm::mat4 Transform; // Global skeleton transformation to correct bind pose orientation
    std::vector<std::string> BoneNames; // Name of each bone
    std::unordered_map<std::string, int> BoneNameToID; // Bone ID lookup from name
    std::vector<glm::mat4> BoneInverseBindPoseTransforms; // Transforms a vertex from model space to bone space
    std::vector<int> BoneParents; // Bone parent index, or -1 if root
    std::vector<float> BoneLengths; // Length of each bone
    int NumBones; // Number of bones in the skeleton
    int NumBoneIndices; // Number of indices for rendering the skeleton as a line mesh
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
    int MaterialID; // The material this mesh was designed for
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
    int NumFrames; // The number of key frames in this animation sequence
    int NumFrameComponents; // The number of floats per frame.
    int SkeletonID; // The skeleton that this animation sequence animates
    int FramesPerSecond; // Frames per second for each animation sequence
};

// AnimatedSkeleton Table
// Each animated skeleton instance is associated to an animation sequence, which is associated to one skeleton.
struct AnimatedSkeleton
{
    GLuint BoneTransformTBO; // The matrices used to transform the bones
    GLuint BoneTransformTO; // Texture descriptor for the palette
    GLuint SkeletonVAO; // Vertex array for rendering animated skeletons
    GLuint SkeletonVBO; // Vertex buffer object for the skeleton vertices
    int CurrAnimSequenceID; // The currently playing animation sequence for each skinned mesh
    int CurrTimeMillisecond; // The current time in the current animation sequence in milliseconds
    float TimeMultiplier; // Controls the speed of animation
    bool InterpolateFrames; // If true, interpolate animation frames
    std::vector<glm::dualquat> BoneTransformDualQuats; // Skinning palette for DLB
    std::vector<glm::mat3x4> BoneTransformMatrices; // Skinning palette for LBS
    std::vector<BoneControlMode> BoneControls; // How each bone is animated

    // Joint physical properties
    std::vector<glm::vec3> JointPositions;
    std::vector<glm::vec3> JointVelocities;
};

// SkinnedMesh Table
// All instances of skinned meshes in the scene.
// Each skinned mesh instance is associated to one bind pose, which is associated to one skeleton.
// The skeleton of the bind pose and the skeleton of the animated skeleton must be the same one.
struct SkinnedMesh
{
    GLuint SkinningTFO; // Transform feedback for skinning
    GLuint SkinnedVAO; // Vertex array for rendering skinned meshes.
    GLuint PositionTFBO; // Positions created from transform feedback
    GLuint DifferentialTFBO; // Differential geometry from transform feedback
    int BindPoseMeshID; // The ID of the bind pose of this skinned mesh
    int AnimatedSkeletonID; // The animated skeleton used to transform this mesh
};

// Ragdoll Table
// All instances of ragdoll simulations in the scene.
// Each ragdoll simulation is associatd to one animated skeleton.
struct Ragdoll
{
    int AnimatedSkeletonID; // The animated skeleton that is being animated physically
    std::vector<Constraint> BoneConstraints; // all constraints in the simulation used to stop the skeleton from separating
    std::vector<glm::ivec2> BoneConstraintParticleIDs; // particle IDs used in the bone distance constraints 
    std::vector<glm::ivec3> JointConstraintParticleIDs; // particles IDs used in the joint angular constraints
    std::vector<Hull> JointHulls; // the collision hulls associated to every joint
};

// DiffuseTexture Table
struct DiffuseTexture
{
    bool HasTransparency; // If the alpha of this texture has some values < 1.0
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
// Each material is associated to DiffuseTextures (or -1 if not present)
// Each material is associated to SpecularTextures (or -1 if not present)
// Each material is associated to NormalTextures (or -1 if not present)
struct Material
{
    std::vector<int> DiffuseTextureIDs; // Diffuse textures (if present)
    std::vector<int> SpecularTextureIDs; // Specular textures (if present)
    std::vector<int> NormalTextureIDs; // Normal textures (if present)
};

struct TransformSceneNode
{
    // Empty node with no purpose other than making nodes relative to it
};

struct StaticMeshSceneNode
{
    int StaticMeshID; // The static mesh to render
};

struct SkinnedMeshSceneNode
{
    int SkinnedMeshID; // The skinned mesh to render
};

// SceneNode Table
struct SceneNode
{
    glm::mat4 LocalTransform; // Transform relative to parent (or relative to world if no parent exists)
    glm::mat4 WorldTransform; // Transform relative to world (updated from RelativeTransform)

    SceneNodeType Type; // What type of node this is.
    int TransformParentNodeID; // The node this node is placed relative to, or -1 if none

    union
    {
        TransformSceneNode AsTransform;
        StaticMeshSceneNode AsStaticMesh;
        SkinnedMeshSceneNode AsSkinnedMesh;
    };
};

struct Scene
{
    std::vector<StaticMesh> StaticMeshes;

    std::vector<Skeleton> Skeletons;

    std::vector<BindPoseMesh> BindPoseMeshes;

    std::vector<AnimSequence> AnimSequences;

    std::vector<AnimatedSkeleton> AnimatedSkeletons;

    std::vector<SkinnedMesh> SkinnedMeshes;

    std::vector<Ragdoll> Ragdolls;

    std::vector<DiffuseTexture> DiffuseTextures;
    std::unordered_map<std::string, int> DiffuseTextureNameToID;

    std::vector<SpecularTexture> SpecularTextures;
    std::unordered_map<std::string, int> SpecularTextureNameToID;

    std::vector<NormalTexture> NormalTextures;
    std::unordered_map<std::string, int> NormalTextureNameToID;

    std::vector<Material> Materials;
    std::vector<SceneNode> SceneNodes;

    // Skinning shader programs that output skinned vertices using transform feedback.
    std::vector<const char*> SkinningOutputs;
    ReloadableShader SkinningDLB{ "skinning_dlb.vert" };
    ReloadableShader SkinningLBS{ "skinning_lbs.vert" };
    ReloadableProgram SkinningSPs[2];
    GLint SkinningSP_BoneTransformsLoc;

    // Scene shader. Used to render objects in the scene which have their geometry defined in world space.
    ReloadableShader SceneVS{ "scene.vert" };
    ReloadableShader SceneFS{ "scene.frag" };
    ReloadableProgram SceneSP{ &SceneVS, &SceneFS };
    GLint SceneSP_ModelWorldLoc;
    GLint SceneSP_WorldModelLoc;
    GLint SceneSP_ModelViewLoc;
    GLint SceneSP_ModelViewProjectionLoc;
    GLint SceneSP_WorldViewLoc;
    GLint SceneSP_CameraPositionLoc;
    GLint SceneSP_LightPositionLoc;
    GLint SceneSP_WorldLightProjectionLoc;
    GLint SceneSP_DiffuseTextureLoc;
    GLint SceneSP_SpecularTextureLoc;
    GLint SceneSP_NormalTextureLoc;
    GLint SceneSP_ShadowMapTextureLoc;
    GLint SceneSP_IlluminationModelLoc;
    GLint SceneSP_HasNormalMapLoc;
    GLint SceneSP_BackgroundColorLoc;

    // Skeleton shader program used to render bones.
    ReloadableShader SkeletonVS{ "skeleton.vert" };
    ReloadableShader SkeletonFS{ "skeleton.frag" };
    ReloadableProgram SkeletonSP{ &SkeletonVS, &SkeletonFS };
    GLint SkeletonSP_ColorLoc;
    GLint SkeletonSP_ModelViewProjectionLoc;

    // Shadow shader program to render vertices into the shadow map
    ReloadableShader ShadowVS{ "shadow.vert" };
    ReloadableShader ShadowFS{ "shadow.frag" };
    ReloadableProgram ShadowSP{ &ShadowVS, &ShadowFS };
    GLint ShadowSP_ModelLightProjectionLoc;

    // true if all shaders in the scene are compiling/linking successfully.
    // Scene updates will stop if not all shaders are working, since it will likely crash.
    bool AllShadersOK;

    // Camera placement, updated each frame.
    glm::vec3 CameraPosition;
    glm::vec4 CameraQuaternion;
    glm::mat3 CameraRotation; // updated from quaternion every frame

    glm::vec3 BackgroundColor;

    // The camera only reads user input when it is enabled.
    // Needed to implement menu navigation without the camera moving due to mouse/keyboard action.
    bool EnableCamera;

    // The current skinning method used to skin all meshes in the scene
    SkinningMethod MeshSkinningMethod;

    // For debugging skeletal animations
    bool ShowBindPoses;
    bool ShowSkeletons;

    // Placing the hellknight (for testing)
    int HellknightTransformNodeID;
    glm::vec3 HellknightPosition;

    bool IsPlaying;
    bool ShouldStep;

    // Damping coefficient for ragdolls
    // 1.0 = rigid body
    float RagdollBoneStiffness;
    float RagdollJointStiffness;
    float RagdollDampingK;

    float Gravity;

    glm::vec3 LightPosition;

    Profiler Profiling;

    // Exponential weighted moving averages for profiling statistics
    std::unordered_map<std::string,float> ProfilingEMAs;
};

void InitScene(Scene* scene);

void UpdateScene(Scene* scene, SDL_Window* window, uint32_t deltaMilliseconds);
