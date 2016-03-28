#include "scene.h"

#include "animation.h"
#include "dynamics.h"
#include "runtimecpp.h"
#include "mysdl_dpi.h"

#include "sceneloader.h"

#define QFPC_IMPLEMENTATION
#include <qfpc.h>

#include "imgui/imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL.h>

#include <functional>
#include <algorithm>
#include <numeric>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

static int AddAnimatedSkeleton(
    Scene* scene,
    int initialAnimSequenceID)
{
    AnimatedSkeleton animatedSkeleton;

    const AnimSequence& animSequence = scene->AnimSequences[initialAnimSequenceID];
    Skeleton& skeleton = scene->Skeletons[animSequence.SkeletonID];

    // Skinning

    glGenBuffers(1, &animatedSkeleton.BoneTransformTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, animatedSkeleton.BoneTransformTBO);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(glm::mat3x4) * skeleton.NumBones, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    glGenTextures(1, &animatedSkeleton.BoneTransformTO);
    glBindTexture(GL_TEXTURE_BUFFER, animatedSkeleton.BoneTransformTO);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, animatedSkeleton.BoneTransformTBO);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    // Skeleton

    glGenBuffers(1, &animatedSkeleton.SkeletonVBO);
    glBindBuffer(GL_ARRAY_BUFFER, animatedSkeleton.SkeletonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * skeleton.NumBones, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &animatedSkeleton.SkeletonVAO);
    glBindVertexArray(animatedSkeleton.SkeletonVAO);

    glBindBuffer(GL_ARRAY_BUFFER, animatedSkeleton.SkeletonVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skeleton.BoneEBO);

    glBindVertexArray(0);

    animatedSkeleton.CurrAnimSequenceID = initialAnimSequenceID;
    animatedSkeleton.CurrTimeMillisecond = 0;
    animatedSkeleton.TimeMultiplier = 1.0f;
    animatedSkeleton.InterpolateFrames = true;
    animatedSkeleton.BoneTransformDualQuats.resize(skeleton.NumBones);
    animatedSkeleton.BoneTransformMatrices.resize(skeleton.NumBones);
    animatedSkeleton.BoneControls.resize(skeleton.NumBones, BONECONTROL_ANIMATION);
    animatedSkeleton.BoneVertices.resize(skeleton.NumBones);

    scene->AnimatedSkeletons.push_back(std::move(animatedSkeleton));
    return (int)scene->AnimatedSkeletons.size() - 1;
}

static int AddSkinnedMesh(
    Scene* scene,
    int bindPoseMeshID,
    int animatedSkeletonID)
{
    SkinnedMesh skinnedMesh;

    const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[bindPoseMeshID];

    if (bindPoseMesh.SkeletonID != scene->AnimSequences[scene->AnimatedSkeletons[animatedSkeletonID].CurrAnimSequenceID].SkeletonID)
    {
        fprintf(stderr, "Incompatible BindPoseMesh and AnimatedSkeleton\n");
        return -1;
    }

    skinnedMesh.BindPoseMeshID = bindPoseMeshID;
    skinnedMesh.AnimatedSkeletonID = animatedSkeletonID;

    glGenBuffers(1, &skinnedMesh.PositionTFBO);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, skinnedMesh.PositionTFBO);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bindPoseMesh.NumVertices * sizeof(PositionVertex), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    glGenBuffers(1, &skinnedMesh.DifferentialTFBO);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, skinnedMesh.DifferentialTFBO);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bindPoseMesh.NumVertices * sizeof(DifferentialVertex), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    glGenTransformFeedbacks(1, &skinnedMesh.SkinningTFO);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, skinnedMesh.SkinningTFO);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, skinnedMesh.PositionTFBO);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, skinnedMesh.DifferentialTFBO);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    glGenVertexArrays(1, &skinnedMesh.SkinnedVAO);
    glBindVertexArray(skinnedMesh.SkinnedVAO);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedMesh.PositionTFBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PositionVertex), (GLvoid*)offsetof(PositionVertex, Position));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.TexCoordVBO);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexCoordVertex), (GLvoid*)offsetof(TexCoordVertex, TexCoord));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, skinnedMesh.DifferentialTFBO);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Normal));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Tangent));
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Bitangent));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bindPoseMesh.EBO);

    glBindVertexArray(0);

    scene->SkinnedMeshes.push_back(std::move(skinnedMesh));
    return (int)scene->SkinnedMeshes.size() - 1;
}

static int AddRagdoll(
    Scene* scene,
    int animatedSkeletonID)
{
    Ragdoll ragdoll;
    ragdoll.AnimatedSkeletonID = animatedSkeletonID;
    ragdoll.OldBufferIndex = -1;

    const AnimatedSkeleton& animatedSkeleton = scene->AnimatedSkeletons[animatedSkeletonID];
    int animSequenceID = animatedSkeleton.CurrAnimSequenceID;
    const AnimSequence& animSequence = scene->AnimSequences[animSequenceID];
    int skeletonID = animSequence.SkeletonID;
    const Skeleton& skeleton = scene->Skeletons[skeletonID];

    for (std::vector<glm::vec3>& bonePositions : ragdoll.BonePositions)
    {
        bonePositions.resize(skeleton.NumBones);
    }

    for (std::vector<glm::vec3>& boneVelocities : ragdoll.BoneVelocities)
    {
        boneVelocities.resize(skeleton.NumBones);
    }

    // Constraints for all the bones
    // FIXME: Actually do want the number of bones here. Not joints.
    int numJoints = skeleton.NumBones;
    // ignore root joint since it doesn't link back to a parent
    ragdoll.BoneConstraints.resize(numJoints - 1);
    ragdoll.BoneConstraintParticleIDs.resize(numJoints - 1);
    for (int jointIdx = 1; jointIdx < numJoints; jointIdx++)
    {
        int parentJointIdx = skeleton.BoneParents[jointIdx];

        glm::ivec2& particles = ragdoll.BoneConstraintParticleIDs[jointIdx - 1];
        particles = glm::ivec2(jointIdx, parentJointIdx);

        Constraint& constraint = ragdoll.BoneConstraints[jointIdx - 1];
        constraint.Func = CONSTRAINTFUNC_DISTANCE;
        constraint.NumParticles = 2;
        constraint.ParticleIDs = value_ptr(particles);
        constraint.Stiffness = scene->RagdollBoneStiffness;
        constraint.Type = CONSTRAINTTYPE_EQUALITY;
        constraint.Distance.Distance = skeleton.BoneLengths[jointIdx];
    }

    scene->Ragdolls.push_back(std::move(ragdoll));
    return (int)scene->Ragdolls.size() - 1;
}

static int AddTransformSceneNode(
    Scene* scene)
{
    SceneNode sceneNode;
    sceneNode.Type = SCENENODETYPE_TRANSFORM;
    sceneNode.TransformParentNodeID = -1;

    scene->SceneNodes.push_back(std::move(sceneNode));
    return (int)scene->SceneNodes.size() - 1;
}

static int AddStaticMeshSceneNode(
    Scene* scene,
    int staticMeshID)
{
    SceneNode sceneNode;
    sceneNode.Type = SCENENODETYPE_STATICMESH;
    sceneNode.TransformParentNodeID = -1;
    sceneNode.AsStaticMesh.StaticMeshID = staticMeshID;

    scene->SceneNodes.push_back(std::move(sceneNode));
    return (int)scene->SceneNodes.size() - 1;
}

static int AddSkinnedMeshSceneNode(
    Scene* scene,
    int skinnedMeshID)
{
    SceneNode sceneNode;
    sceneNode.Type = SCENENODETYPE_SKINNEDMESH;
    sceneNode.TransformParentNodeID = -1;
    sceneNode.AsSkinnedMesh.SkinnedMeshID = skinnedMeshID;

    scene->SceneNodes.push_back(std::move(sceneNode));
    return (int)scene->SceneNodes.size() - 1;
}

void InitScene(Scene* scene)
{
    // Initial values
    scene->AllShadersOK = false;
    scene->CameraPosition = glm::vec3(85.9077225f, 200.844162f, 140.049072f);
    scene->CameraQuaternion = glm::vec4(-0.351835f, 0.231701f, 0.090335f, 0.902411f);
    scene->EnableCamera = true;
    scene->MeshSkinningMethod = SKINNING_DLB;
    scene->IsPlaying = true;
    scene->ShouldStep = false;
    // Cornflower blue
    /*scene->BackgroundColor = glm::vec3(
        std::pow(100.0f / 255.0f, 2.2f),
        std::pow(149.0f / 255.0f, 2.2f),
        std::pow(237.0f / 255.0f, 2.2f));*/
    // Dark grey
    scene->BackgroundColor = glm::vec3(
        std::pow(25.0f / 255.0f, 2.2f),
        std::pow(25.0f / 255.0f, 2.2f),
        std::pow(25.0f / 255.0f, 2.2f));
    scene->ShowBindPoses = false;
    scene->ShowSkeletons = false;
    scene->RagdollDampingK = 0.5f;
    scene->RagdollBoneStiffness = 0.5f;

    std::string assetFolder = "assets/";

    std::string hellknight_modelFolder = "hellknight/";
    std::string hellknight_meshFile = "hellknight.md5mesh";
    std::vector<std::string> hellknight_animFiles{
        "idle2.md5anim",
        "attack3.md5anim",
        "chest.md5anim",
        "headpain.md5anim",
        "leftslash.md5anim",
        "pain_luparm.md5anim",
        "pain_ruparm.md5anim",
        "pain1.md5anim",
        "range_attack2.md5anim",
        "roar1.md5anim",
        "stand.md5anim",
        "turret_attack.md5anim",
        "walk7.md5anim",
        "walk7_left.md5anim"
    };

    std::vector<int> hellknightBindPoseMeshIDs;
    int hellknightSkeletonID;
    LoadMD5Mesh(
        scene,
        assetFolder.c_str(), hellknight_modelFolder.c_str(),
        hellknight_meshFile.c_str(),
        NULL, &hellknightSkeletonID, &hellknightBindPoseMeshIDs);

    std::vector<int> hellknightAnimSequenceIDs;
    for (const std::string& animFile : hellknight_animFiles)
    {
        int animSequenceID;
        LoadMD5Anim(
            scene,
            hellknightSkeletonID,
            assetFolder.c_str(), hellknight_modelFolder.c_str(),
            animFile.c_str(),
            &animSequenceID);
        hellknightAnimSequenceIDs.push_back(animSequenceID);
    }

    int hellknightInitialAnimSequenceID = hellknightAnimSequenceIDs[0];
    int hellknightAnimatedSkeletonID = AddAnimatedSkeleton(scene, hellknightInitialAnimSequenceID);

    int hellknightTransformNodeID = AddTransformSceneNode(scene);
    scene->HellknightTransformNodeID = hellknightTransformNodeID;

    for (int hellknightMeshIdx = 0; hellknightMeshIdx < (int)hellknightBindPoseMeshIDs.size(); hellknightMeshIdx++)
    {
        int hellknightBindPoseMeshID = hellknightBindPoseMeshIDs[hellknightMeshIdx];
        int hellknightSkinnedMeshID = AddSkinnedMesh(scene, hellknightBindPoseMeshID, hellknightAnimatedSkeletonID);
        int hellknightRagdollID = AddRagdoll(scene, hellknightAnimatedSkeletonID);
        int hellknightSceneNode = AddSkinnedMeshSceneNode(scene, hellknightSkinnedMeshID);
        scene->SceneNodes[hellknightSceneNode].TransformParentNodeID = hellknightTransformNodeID;
    }

    std::vector<int> floorStaticMeshIDs;
    LoadOBJMesh(scene, assetFolder.c_str(), "floor/", "floor.obj", NULL, &floorStaticMeshIDs);

    int floorTransformNodeID = AddTransformSceneNode(scene);
    for (int floorMeshIdx = 0; floorMeshIdx < (int)floorStaticMeshIDs.size(); floorMeshIdx++)
    {
        int floorSceneNode = AddStaticMeshSceneNode(scene, floorStaticMeshIDs[floorMeshIdx]);
        scene->SceneNodes[floorSceneNode].TransformParentNodeID = floorTransformNodeID;
    }
}

static void ReloadShaders(Scene* scene)
{
    // Convenience functions to make things more concise
    GLuint sp;
    bool anyProgramOutOfDate = false;
    bool anyProgramRelinkFailed = false;
    auto reload = [&sp, &anyProgramOutOfDate, &anyProgramRelinkFailed](ReloadableProgram* program)
    {
        bool wasOutOfDate, newProgramLinked;
        ReloadProgram(program, &wasOutOfDate, &newProgramLinked);
        sp = program->Handle;
        if (wasOutOfDate)
        {
            anyProgramOutOfDate = true;
        }
        if (wasOutOfDate && !newProgramLinked)
        {
            anyProgramRelinkFailed = true;
        }
        return newProgramLinked;
    };

    // Get uniform and do nothing if not found
    auto getUOpt = [&sp](GLint* result, const char* name)
    {
        *result = glGetUniformLocation(sp, name);
        return false;
    };

    // Get uniform and fail if not found
    auto getU = [&getUOpt](GLint* result, const char* name)
    {
        getUOpt(result, name);
        if (*result == -1)
        {
            fprintf(stderr, "Couldn't find uniform %s\n", name);
        }
        return *result == -1;
    };

    // Reload shaders & uniforms
    if (reload(&scene->SkinningSPs[scene->MeshSkinningMethod]))
    {
        if (getU(&scene->SkinningSP_BoneTransformsLoc, "BoneTransforms"))
        {
            return;
        }
    }

    if (reload(&scene->SceneSP))
    {
        if (getUOpt(&scene->SceneSP_ModelWorldLoc, "ModelWorld") ||
            getUOpt(&scene->SceneSP_WorldModelLoc, "WorldModel") ||
            getUOpt(&scene->SceneSP_ModelViewLoc, "ModelView") ||
            getUOpt(&scene->SceneSP_ModelViewProjectionLoc, "ModelViewProjection") ||
            getUOpt(&scene->SceneSP_WorldViewLoc, "WorldView") ||
            getUOpt(&scene->SceneSP_CameraPositionLoc, "CameraPosition") ||
            getUOpt(&scene->SceneSP_DiffuseTextureLoc, "DiffuseTexture") ||
            getUOpt(&scene->SceneSP_SpecularTextureLoc, "SpecularTexture") ||
            getUOpt(&scene->SceneSP_NormalTextureLoc, "NormalTexture") ||
            getUOpt(&scene->SceneSP_IlluminationModelLoc, "IlluminationModel") ||
            getUOpt(&scene->SceneSP_HasNormalMapLoc, "HasNormalMap") ||
            getUOpt(&scene->SceneSP_BackgroundColorLoc, "BackgroundColor"))
        {
            return;
        }
    }

    if (reload(&scene->SkeletonSP))
    {
        if (getU(&scene->SkeletonSP_ColorLoc, "Color") ||
            getU(&scene->SkeletonSP_ModelViewProjectionLoc, "ModelViewProjection"))
        {
            return;
        }
    }

    if (anyProgramOutOfDate)
    {
        scene->AllShadersOK = !anyProgramRelinkFailed;
    }
}

static void ShowSystemInfoGUI(Scene* scene)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiSetCond_Always);
    if (ImGui::Begin("Info", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        char cpuBrandString[0x40];
        memset(cpuBrandString, 0, sizeof(cpuBrandString));

        // Get CPU brand string.
#ifdef _WIN32
        int cpuInfo[4] = { -1 };
        __cpuid(cpuInfo, 0x80000002);
        memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000003);
        memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000004);
        memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));
#elif __APPLE__
        size_t len = sizeof(cpuBrandString);
        sysctlbyname("machdep.cpu.brand_string", &cpuBrandString, &len, NULL, 0);
#endif

        ImGui::Text("CPU: %s\n", cpuBrandString);
        ImGui::Text("GL_VENDOR: %s", glGetString(GL_VENDOR));
        ImGui::Text("GL_RENDERER: %s", glGetString(GL_RENDERER));
        ImGui::Text("GL_VERSION: %s", glGetString(GL_VERSION));
        ImGui::Text("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    }
    ImGui::End();
}

static void ShowToolboxGUI(Scene* scene, SDL_Window* window)
{
    ImGuiIO& io = ImGui::GetIO();
    int w = int(io.DisplaySize.x / io.DisplayFramebufferScale.x);
    int h = int(io.DisplaySize.y / io.DisplayFramebufferScale.y);

    int toolboxW = 300, toolboxH = 600;

    ImGui::SetNextWindowSize(ImVec2((float)toolboxW, (float)toolboxH), ImGuiSetCond_Always);
    ImGui::SetNextWindowPos(ImVec2((float)w - toolboxW, 0), ImGuiSetCond_Always);
    if (ImGui::Begin("Toolbox", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImVec4 shaderStatusColor = scene->AllShadersOK ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
        ImGui::ColorButton(shaderStatusColor); ImGui::SameLine(); ImGui::Text("Shader compilation status");

        int currSelectedAnimatedSkeleton = 0;

        if (currSelectedAnimatedSkeleton < (int)scene->AnimatedSkeletons.size())
        {
            AnimatedSkeleton& animatedSkeleton = scene->AnimatedSkeletons[currSelectedAnimatedSkeleton];
            int currAnimSequenceID = animatedSkeleton.CurrAnimSequenceID;
            const AnimSequence& animSequence = scene->AnimSequences[currAnimSequenceID];
            int skeletonID = animSequence.SkeletonID;

            // find all animations compatible with the skeleton
            std::vector<const char*> animSequenceNames;
            std::vector<int> animSequenceIDs;
            int currAnimSequenceIndexInCombo = -1;
            for (int animSequenceID = 0; animSequenceID < (int)scene->AnimSequences.size(); animSequenceID++)
            {
                const AnimSequence& animSequence = scene->AnimSequences[animSequenceID];
                if (animSequence.SkeletonID == skeletonID)
                {
                    animSequenceNames.push_back(animSequence.Name.c_str());
                    animSequenceIDs.push_back(animSequenceID);
                    if (animSequenceID == currAnimSequenceID)
                    {
                        currAnimSequenceIndexInCombo = (int)animSequenceNames.size() - 1;
                    }
                }
            }

            // Display list to select animation
            if (!animSequenceNames.empty())
            {
                // Right-align items to increase width of text
                ImGui::PushItemWidth(-1.0f);

                ImGui::Checkbox("Show Bind Poses", &scene->ShowBindPoses);
                ImGui::Checkbox("Show Skeletons", &scene->ShowSkeletons);
                ImGui::Checkbox("Interpolate Frames", &animatedSkeleton.InterpolateFrames);

                ImGui::Text("Animation Sequence");
                if (ImGui::Combo("##animsequences", &currAnimSequenceIndexInCombo, animSequenceNames.data(), (int)animSequenceNames.size()))
                {
                    animatedSkeleton.CurrAnimSequenceID = animSequenceIDs[currAnimSequenceIndexInCombo];
                    animatedSkeleton.CurrTimeMillisecond = 0;
                }

                ImGui::Text("Animation Speed");
                ImGui::SliderFloat("##animspeed", &animatedSkeleton.TimeMultiplier, 0.0f, 2.0f);

                ImGui::Text("Hellknight Position");
                if (ImGui::SliderFloat3("##hellknightposition", value_ptr(scene->HellknightPosition), -100.0f, 100.0f))
                {
                    scene->SceneNodes[scene->HellknightTransformNodeID].LocalTransform = translate(glm::mat4(), scene->HellknightPosition);
                }

                ImGui::Text("Skinning Method");
                if (ImGui::RadioButton("Dual Quaternion Linear Blending", scene->MeshSkinningMethod == SKINNING_DLB))
                {
                    scene->MeshSkinningMethod = SKINNING_DLB;
                    ReloadShaders(scene);
                }
                if (ImGui::RadioButton("Linear Blend Skinning", scene->MeshSkinningMethod == SKINNING_LBS))
                {
                    scene->MeshSkinningMethod = SKINNING_LBS;
                    ReloadShaders(scene);
                }

                ImGui::Text("Ragdoll Damping (1.0 = rigid)");
                ImGui::SliderFloat("##ragdolldamping", &scene->RagdollDampingK, 0.0f, 1.0f);

                ImGui::Text("Ragdoll Bone Stiffness");
                if (ImGui::SliderFloat("##bonestiffness", &scene->RagdollBoneStiffness, 0.0f, 1.0f))
                {
                    for (int ragdollIdx = 0; ragdollIdx < (int)scene->Ragdolls.size(); ragdollIdx++)
                    {
                        for (Constraint& c : scene->Ragdolls[ragdollIdx].BoneConstraints)
                        {
                            c.Stiffness = scene->RagdollBoneStiffness;
                        }
                    }
                }

                ImGui::Text("Bone Control");
                if (ImGui::RadioButton("Skeletal Animation", animatedSkeleton.BoneControls[0] == BONECONTROL_ANIMATION))
                {
                    std::fill(begin(animatedSkeleton.BoneControls), end(animatedSkeleton.BoneControls), BONECONTROL_ANIMATION);
                }

                if (ImGui::RadioButton("Ragdoll Physics", animatedSkeleton.BoneControls[0] == BONECONTROL_DYNAMICS))
                {
                    std::fill(begin(animatedSkeleton.BoneControls), end(animatedSkeleton.BoneControls), BONECONTROL_DYNAMICS);
                    for (int ragdollIdx = 0; ragdollIdx < (int)scene->Ragdolls.size(); ragdollIdx++)
                    {
                        Ragdoll& ragdoll = scene->Ragdolls[ragdollIdx];
                        if (ragdoll.AnimatedSkeletonID != currSelectedAnimatedSkeleton)
                        {
                            continue;
                        }

                        ragdoll.OldBufferIndex = 0;

                        for (int boneIdx = 0; boneIdx < (int)ragdoll.BoneVelocities[ragdoll.OldBufferIndex].size(); boneIdx++)
                        {
                            ragdoll.BonePositions[ragdoll.OldBufferIndex][boneIdx] = animatedSkeleton.BoneVertices[boneIdx];
                        }

                        for (int boneIdx = 0; boneIdx < (int)ragdoll.BoneVelocities[ragdoll.OldBufferIndex].size(); boneIdx++)
                        {
                            ragdoll.BoneVelocities[ragdoll.OldBufferIndex][boneIdx] = glm::vec3(0.0f);
                        }
                    }
                }

                ImGui::PopItemWidth();

                if (scene->IsPlaying)
                {
                    if (ImGui::Button("Pause"))
                    {
                        scene->IsPlaying = false;
                        scene->ShouldStep = false;
                    }
                }
                else
                {
                    if (ImGui::Button("Play"))
                    {
                        scene->IsPlaying = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Step"))
                    {
                        scene->ShouldStep = true;
                    }
                }
            }
        }
    }
    ImGui::End();
}

static void UpdateAnimatedSkeletons(Scene* scene, uint32_t dt_ms)
{
    // Storage for animation frame
    std::vector<SQT> frame;

    for (int animSkeletonIdx = 0; animSkeletonIdx < (int)scene->AnimatedSkeletons.size(); animSkeletonIdx++)
    {
        AnimatedSkeleton& animSkeleton = scene->AnimatedSkeletons[animSkeletonIdx];

        // Pause the animation if viewing in bind pose
        if (!scene->ShowBindPoses)
        {
            animSkeleton.CurrTimeMillisecond += (uint32_t)(animSkeleton.TimeMultiplier * dt_ms);
        }

        // Get new animation frame
        GetFrameAtTime(scene, animSkeleton.CurrAnimSequenceID, animSkeleton.CurrTimeMillisecond, animSkeleton.InterpolateFrames, frame);

        const AnimSequence& animSequence = scene->AnimSequences[animSkeleton.CurrAnimSequenceID];
        Skeleton& skeleton = scene->Skeletons[animSequence.SkeletonID];

        // Calculate skinning transformations and bone vertices
        for (int boneIdx = 0; boneIdx < skeleton.NumBones; boneIdx++)
        {
            if (!scene->ShowBindPoses && animSkeleton.BoneControls[boneIdx] != BONECONTROL_ANIMATION)
            {
                continue;
            }

            // Final joint transformation
            glm::mat4 boneTransform = skeleton.Transform;

            // TODO: Remove if we continue using bone rest lengths for the dynamic constraints.
            // Set the bone lengths to their current length in the current animation frame.
            // See inner conditions below for usage.
            int parentBoneIdx = skeleton.BoneParents[boneIdx];

            if (scene->ShowBindPoses)
            {
                glm::mat4 bindPose = skeleton.Transform * inverse(skeleton.BoneInverseBindPoseTransforms[boneIdx]);
                glm::vec3 bindPosePos = glm::vec3(bindPose[3]);

                // Bind pose joint position
                animSkeleton.BoneVertices[boneIdx] = bindPosePos;

                if (parentBoneIdx != -1)
                {
                    glm::mat4 parentBindPose = skeleton.Transform * inverse(skeleton.BoneInverseBindPoseTransforms[parentBoneIdx]);
                    glm::vec3 parentBindPosePos = glm::vec3(parentBindPose[3]);

                    skeleton.BoneLengths[boneIdx] = length(bindPosePos - parentBindPosePos);
                }
            }
            else
            {
                // Add animation frame transformation to final joint transformation
                glm::mat4 translation = translate(frame[boneIdx].T);
                glm::mat4 orientation = mat4_cast(frame[boneIdx].Q);
                boneTransform *= translation * orientation * skeleton.BoneInverseBindPoseTransforms[boneIdx];

                // Animation frame joint position
                animSkeleton.BoneVertices[boneIdx] = glm::vec3(skeleton.Transform * glm::vec4(frame[boneIdx].T, 1.0));

                if (parentBoneIdx != -1)
                {
                    skeleton.BoneLengths[boneIdx] = length(frame[boneIdx].T - frame[parentBoneIdx].T);
                }
            }

            // Discard bottom row as it contains no pertinent transformation data, then store as columns
            glm::mat3x4 boneTransformRowsAsCols = transpose(glm::mat4x3(boneTransform));

            switch (scene->MeshSkinningMethod)
            {
            case SKINNING_DLB:
                // Convert transform to a dual quaterion for the DLB shader
                animSkeleton.BoneTransformDualQuats[boneIdx] = glm::dualquat(boneTransformRowsAsCols);
                break;
            case SKINNING_LBS:
                // Use upper 3 rows as columns, so the LBS shader can extract each row as a texel
                animSkeleton.BoneTransformMatrices[boneIdx] = boneTransformRowsAsCols;
                break;
            }
        }

        // Upload skeleton bones
        glBindBuffer(GL_ARRAY_BUFFER, animSkeleton.SkeletonVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(animSkeleton.BoneVertices[0]) * animSkeleton.BoneVertices.size(), animSkeleton.BoneVertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        GLvoid *boneTransformsData;
        GLsizeiptr boneTransformsSize;

        switch (scene->MeshSkinningMethod)
        {
        case SKINNING_DLB:
            boneTransformsData = animSkeleton.BoneTransformDualQuats.data();
            boneTransformsSize = sizeof(glm::dualquat) * skeleton.NumBones;
            break;
        case SKINNING_LBS:
            boneTransformsData = animSkeleton.BoneTransformMatrices.data();
            boneTransformsSize = sizeof(glm::mat3x4) * skeleton.NumBones;
            break;
        default:
            boneTransformsData = NULL;
            boneTransformsSize = 0;
        }

        // Upload skinning transformations
        glBindBuffer(GL_TEXTURE_BUFFER, animSkeleton.BoneTransformTBO);
        glBufferSubData(GL_TEXTURE_BUFFER, 0, boneTransformsSize, boneTransformsData);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }
}

static void UpdateSkinnedGeometry(Scene* scene, uint32_t dt_ms)
{
    // Skin vertices using the matrix palette and store them with transform feedback
    glUseProgram(scene->SkinningSPs[scene->MeshSkinningMethod].Handle);
    glUniform1i(scene->SkinningSP_BoneTransformsLoc, 0);
    glEnable(GL_RASTERIZER_DISCARD);
    for (int skinnedMeshIdx = 0; skinnedMeshIdx < (int)scene->SkinnedMeshes.size(); skinnedMeshIdx++)
    {
        const SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshIdx];
        int bindPoseMeshID = skinnedMesh.BindPoseMeshID;
        const BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[bindPoseMeshID];
        int animatedSkeletonID = skinnedMesh.AnimatedSkeletonID;
        const AnimatedSkeleton& animatedSkeleton = scene->AnimatedSkeletons[animatedSkeletonID];

        glBindVertexArray(bindPoseMesh.SkinningVAO);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, skinnedMesh.SkinningTFO);
        glBeginTransformFeedback(GL_POINTS); // capture points so triangles aren't unfolded

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, animatedSkeleton.BoneTransformTO);

        glDrawArrays(GL_POINTS, 0, bindPoseMesh.NumVertices);

        glEndTransformFeedback();
    }
    glDisable(GL_RASTERIZER_DISCARD);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}

static void UpdateDynamics(Scene* scene, uint32_t dt_ms)
{
    static PFNSIMULATEDYNAMICSPROC pfnSimulateDynamics = NULL;

#ifdef _MSC_VER
    static RuntimeCpp runtimeSimulateDynamics(L"SimulateDynamics.dll", { "SimulateDynamics" });
    if (PollDLLs(&runtimeSimulateDynamics)) {
        runtimeSimulateDynamics.GetProc(pfnSimulateDynamics, "SimulateDynamics");
    }
#else
    pfnSimulateDynamics = SimulateDynamics;
#endif

    if (!pfnSimulateDynamics)
    {
        return;
    }

    float dt_s = dt_ms / 1000.0f;

    for (int ragdollIdx = 0; ragdollIdx < (int)scene->Ragdolls.size(); ragdollIdx++)
    {
        Ragdoll& ragdoll = scene->Ragdolls[ragdollIdx];
        AnimatedSkeleton& animatedSkeleton = scene->AnimatedSkeletons[ragdoll.AnimatedSkeletonID];
        AnimSequence& animSequence = scene->AnimSequences[animatedSkeleton.CurrAnimSequenceID];
        Skeleton& skeleton = scene->Skeletons[animSequence.SkeletonID];

        int numBones = (int)ragdoll.BonePositions[0].size();

        if (ragdoll.OldBufferIndex == -1) // First update? initialize from animation
        {
            ragdoll.OldBufferIndex = 0;
            ragdoll.BonePositions[0] = animatedSkeleton.BoneVertices;
            // FIXME: Velocity should actually be based on how much the skeleton moved since the last frame of animation
            std::fill(begin(ragdoll.BoneVelocities[0]), end(ragdoll.BoneVelocities[0]), glm::vec3(0.0f, 0.0f, 0.0f));
        }
        else // update bones that the animation controls
        {
            for (int boneIdx = 0; boneIdx < numBones; boneIdx++)
            {
                if (animatedSkeleton.BoneControls[boneIdx] == BONECONTROL_DYNAMICS)
                {
                    continue;
                }

                ragdoll.BoneVelocities[ragdoll.OldBufferIndex][boneIdx] = (animatedSkeleton.BoneVertices[boneIdx] - ragdoll.BonePositions[ragdoll.OldBufferIndex][boneIdx]) / dt_s;
                ragdoll.BonePositions[ragdoll.OldBufferIndex][boneIdx] = animatedSkeleton.BoneVertices[boneIdx];
            }
        }

        // read from frontbuffer
        const std::vector<glm::vec3>& oldPositions = ragdoll.BonePositions[ragdoll.OldBufferIndex];
        const std::vector<glm::vec3>& oldVelocities = ragdoll.BoneVelocities[ragdoll.OldBufferIndex];

        // write to backbuffer
        std::vector<glm::vec3>& newPositions = ragdoll.BonePositions[(ragdoll.OldBufferIndex + 1) % 2];
        std::vector<glm::vec3>& newVelocities = ragdoll.BoneVelocities[(ragdoll.OldBufferIndex + 1) % 2];
        
        // all unit masses for now
        std::vector<float> masses(numBones, 1.0f);

        // just gravity for now
        std::vector<glm::vec3> externalForces(numBones);
        for (int i = 0; i < numBones; i++)
        {
            externalForces[i] = glm::vec3(0.0f, -9.8f * 100.0f, 0.0f) * masses[i];
        }

        for (int boneIdx = 0; boneIdx < numBones - 1; boneIdx++)
        {
            ragdoll.BoneConstraints[boneIdx].Distance.Distance = skeleton.BoneLengths[boneIdx + 1];
        }

        // do the dynamics dance
        pfnSimulateDynamics(
            dt_s,
            (float*)data(oldPositions),
            (float*)data(oldVelocities),
            (float*)data(masses),
            (float*)data(externalForces),
            NULL, // TODO: Hulls
            numBones, DEFAULT_DYNAMICS_NUM_ITERATIONS,
            data(ragdoll.BoneConstraints), (int)ragdoll.BoneConstraints.size(),
            scene->RagdollDampingK,
            (float*)data(newPositions),
            (float*)data(newVelocities));

        // swap buffers
        ragdoll.OldBufferIndex = (ragdoll.OldBufferIndex + 1) % 2;

        // Update select bones based on dynamics
        for (int boneIdx = 0; boneIdx < numBones; boneIdx++)
        {
            if (animatedSkeleton.BoneControls[boneIdx] != BONECONTROL_DYNAMICS)
            {
                continue;
            }

            glm::vec3 oldPosition = animatedSkeleton.BoneVertices[boneIdx];
            glm::vec3 newPosition = newPositions[boneIdx];
            animatedSkeleton.BoneVertices[boneIdx] = newPosition;
            
            glm::vec3 deltaPosition = newPosition - oldPosition;
            animatedSkeleton.BoneTransformMatrices[boneIdx][0][3] += deltaPosition.x;
            animatedSkeleton.BoneTransformMatrices[boneIdx][1][3] += deltaPosition.y;
            animatedSkeleton.BoneTransformMatrices[boneIdx][2][3] += deltaPosition.z;
        }
    }
}

void UpdateScene(Scene* scene, SDL_Window* window, uint32_t dt_ms)
{
    ReloadShaders(scene);

    ShowSystemInfoGUI(scene);
    ShowToolboxGUI(scene, window);

    if (!scene->AllShadersOK)
    {
        return;
    }

    // Update camera
    {
        float dt_s = dt_ms / 1000.0f;

        int relativeMouseX, relativeMouseY;
        SDL_GetRelativeMouseState(&relativeMouseX, &relativeMouseY);

        const uint8_t* keyboardState = SDL_GetKeyboardState(NULL);

        quatFirstPersonCamera(
            glm::value_ptr(scene->CameraPosition),
            glm::value_ptr(scene->CameraQuaternion),
            glm::value_ptr(scene->CameraRotation),
            0.10f,
            60.0f * dt_s,
            !scene->EnableCamera ? 0 : relativeMouseX,
            !scene->EnableCamera ? 0 : relativeMouseY,
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_W],
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_A],
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_S],
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_D],
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_SPACE],
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_LCTRL] || keyboardState[SDL_SCANCODE_LSHIFT]);
    }

    if (scene->IsPlaying || scene->ShouldStep)
    {
        UpdateAnimatedSkeletons(scene, dt_ms);

        if (!scene->ShowBindPoses)
        {
            UpdateDynamics(scene, 1000 / 60);
        }

        UpdateSkinnedGeometry(scene, dt_ms);

        scene->ShouldStep = false;
    }

    // Update world transforms from local transforms
    {
        // Partial sort nodes according to parent relationship
        std::vector<int> parentSortedNodes(scene->SceneNodes.size());
        std::iota(begin(parentSortedNodes), end(parentSortedNodes), 0);
        std::make_heap(begin(parentSortedNodes), end(parentSortedNodes),
            [&scene](int n0, int n1) {
            return scene->SceneNodes[n0].TransformParentNodeID > scene->SceneNodes[n1].TransformParentNodeID;
        });

        for (int nodeID : parentSortedNodes)
        {
            if (scene->SceneNodes[nodeID].TransformParentNodeID == -1)
            {
                scene->SceneNodes[nodeID].WorldTransform = scene->SceneNodes[nodeID].LocalTransform;
            }
            else
            {
                int parentNodeID = scene->SceneNodes[nodeID].TransformParentNodeID;
                glm::mat4 parentWorldTransform = scene->SceneNodes[parentNodeID].WorldTransform;
                scene->SceneNodes[nodeID].WorldTransform = scene->SceneNodes[nodeID].LocalTransform * parentWorldTransform;
            }
        }
    }
}
