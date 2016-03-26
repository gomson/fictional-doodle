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

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

static int AddAnimatedSkeleton(
    Scene* scene,
    int initialAnimSequenceID)
{
    AnimatedSkeleton animatedSkeleton;
    
    const AnimSequence& animSequence = scene->AnimSequences[initialAnimSequenceID];
    int skeletonID = animSequence.SkeletonID;
    Skeleton& skeleton = scene->Skeletons[skeletonID];

    glGenBuffers(1, &animatedSkeleton.BoneTransformTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, animatedSkeleton.BoneTransformTBO);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(glm::mat3x4) * skeleton.NumBones, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    glGenTextures(1, &animatedSkeleton.BoneTransformTO);
    glBindTexture(GL_TEXTURE_BUFFER, animatedSkeleton.BoneTransformTO);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, animatedSkeleton.BoneTransformTBO);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    animatedSkeleton.CurrAnimSequenceID = initialAnimSequenceID;
    animatedSkeleton.CurrTimeMillisecond = 0;
    animatedSkeleton.TimeMultiplier = 1.0f;
    animatedSkeleton.InterpolateFrames = true;
    animatedSkeleton.BoneTransformDualQuats.resize(skeleton.NumBones);
    animatedSkeleton.BoneTransformMatrices.resize(skeleton.NumBones);
    animatedSkeleton.BoneControls.resize(skeleton.NumBones, BONECONTROL_ANIMATION);

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
    Skeleton& skeleton = scene->Skeletons[skeletonID];

    for (std::vector<glm::vec3>& bonePositions : ragdoll.BonePositions)
    {
        bonePositions.resize(skeleton.NumBones);
    }

    for (std::vector<glm::vec3>& boneVelocities : ragdoll.BoneVelocities)
    {
        boneVelocities.resize(skeleton.NumBones);
    }

    scene->Ragdolls.push_back(std::move(ragdoll));
    return (int)scene->Ragdolls.size() - 1;
}

static int AddSkinnedMeshSceneNode(
    Scene* scene,
    int skinnedMeshID)
{
    SceneNode sceneNode;
    sceneNode.ModelWorldTransform = glm::mat4();
    sceneNode.Type = SCENENODETYPE_SKINNEDMESH;
    sceneNode.AsSkinnedMesh.SkinnedMeshID = skinnedMeshID;
    
    scene->SceneNodes.push_back(std::move(sceneNode));
    return (int)scene->SceneNodes.size() - 1;
}

void InitScene(Scene* scene)
{
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

    for (int hellknightMeshIdx = 0; hellknightMeshIdx < (int)hellknightBindPoseMeshIDs.size(); hellknightMeshIdx++)
    {
        int hellknightBindPoseMeshID = hellknightBindPoseMeshIDs[hellknightMeshIdx];
        int hellknightSkinnedMeshID = AddSkinnedMesh(scene, hellknightBindPoseMeshID, hellknightAnimatedSkeletonID);
        int hellknightRagdollID = AddRagdoll(scene, hellknightAnimatedSkeletonID);

        // TODO: How do we stop the tongue and body and etc from falling out of sync in terms of position?
        // Need a parent node in the scenegraph to keep them all rooted at the same place?
        int hellknightSceneNode = AddSkinnedMeshSceneNode(scene, hellknightSkinnedMeshID);
    }

    scene->AllShadersOK = false;

    // initial camera position
    scene->CameraPosition = glm::vec3(85.9077225f, 200.844162f, 140.049072f);
    scene->CameraQuaternion = glm::vec4(-0.351835f, 0.231701f, 0.090335f, 0.902411f);
    scene->EnableCamera = true;
    scene->MeshSkinningMethod = SKINNING_DLB;
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
            getUOpt(&scene->SceneSP_NormalTextureLoc, "NormalTexture"))
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

    int toolboxW = 300, toolboxH = 400;

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

                ImGui::Checkbox("Interpolate Frames", &animatedSkeleton.InterpolateFrames);

                ImGui::Text("Animation Speed");
                ImGui::SliderFloat("##animspeed", &animatedSkeleton.TimeMultiplier, 0.0f, 2.0f);

                ImGui::Text("Animation Sequence");
                if (ImGui::Combo("##animsequences", &currAnimSequenceIndexInCombo, animSequenceNames.data(), (int)animSequenceNames.size()))
                {
                    animatedSkeleton.CurrAnimSequenceID = animSequenceIDs[currAnimSequenceIndexInCombo];
                    animatedSkeleton.CurrTimeMillisecond = 0;
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

                ImGui::PopItemWidth();
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
        animSkeleton.CurrTimeMillisecond += (uint32_t)(animSkeleton.TimeMultiplier * dt_ms);

        // Get new animation frame
        GetFrameAtTime(scene, animSkeleton.CurrAnimSequenceID, animSkeleton.CurrTimeMillisecond, animSkeleton.InterpolateFrames, frame);

        const AnimSequence& animSequence = scene->AnimSequences[animSkeleton.CurrAnimSequenceID];
        const Skeleton& skeleton = scene->Skeletons[animSequence.SkeletonID];

        // Calculate skinning transformations
        for (int boneIdx = 0; boneIdx < skeleton.NumBones; boneIdx++)
        {
            glm::mat4 translation = translate(frame[boneIdx].T);
            glm::mat4 orientation = mat4_cast(frame[boneIdx].Q);
            glm::mat4 boneTransform = skeleton.Transform * translation * orientation * skeleton.BoneInverseBindPoseTransforms[boneIdx];
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

#if 0
static void UpdateDynamics(Scene* scene, uint32_t dt_ms)
{
    static PFNSIMULATEDYNAMICSPROC pfnSimulateDynamics = NULL;

#ifdef _MSC_VER
    static RuntimeCpp runtimeSimulateDynamics(
        L"SimulateDynamics.dll",
        { "SimulateDynamics" }
    );
    if (PollDLLs(&runtimeSimulateDynamics))
    {
        runtimeSimulateDynamics.GetProc(pfnSimulateDynamics, "SimulateDynamics");
    }
#else
    pfnSimulateDynamics = SimulateDynamics;
#endif

    if (!pfnSimulateDynamics)
    {
        return;
    }

    // read from frontbuffer
    const std::vector<glm::vec3>& oldPositions = scene->BoneDynamicsPositions[(scene->BoneDynamicsBackBufferIndex + 1) % Scene::NUM_BONE_DYNAMICS_BUFFERS];
    const std::vector<glm::vec3>& oldVelocities = scene->BoneDynamicsVelocities[(scene->BoneDynamicsBackBufferIndex + 1) % Scene::NUM_BONE_DYNAMICS_BUFFERS];

    // write to backbuffer
    std::vector<glm::vec3>& newPositions = scene->BoneDynamicsPositions[scene->BoneDynamicsBackBufferIndex];
    std::vector<glm::vec3>& newVelocities = scene->BoneDynamicsVelocities[scene->BoneDynamicsBackBufferIndex];

    int numParticles = (int)oldPositions.size();

    // unit masses for now
    std::vector<float> masses(numParticles, 1.0f);

    // just gravity for now
    std::vector<glm::vec3> externalForces(numParticles);
    for (int i = 0; i < numParticles; i++)
    {
        externalForces[i] = glm::vec3(0.0f, -9.8f, 0.0f) * masses[i];
    }

    // no constraints yet
    std::vector<Constraint> constraints;
    int numConstraints = (int)constraints.size();

    float dt_s = dt_ms / 1000.0f;
    pfnSimulateDynamics(
        dt_s,
        (float*)data(oldPositions),
        (float*)data(oldVelocities),
        (float*)data(masses),
        (float*)data(externalForces),
        numParticles, DEFAULT_DYNAMICS_NUM_ITERATIONS,
        data(constraints), numConstraints,
        (float*)data(newPositions),
        (float*)data(newVelocities));

    // swap buffers
    scene->BoneDynamicsBackBufferIndex = (scene->BoneDynamicsBackBufferIndex + 1) % Scene::NUM_BONE_DYNAMICS_BUFFERS;

    // Update select bones based on dynamics
    for (int b = 0; b < (int)scene->BoneSkinningTransforms.size(); b++)
    {
        if (scene->BoneControls[b] != BONECONTROL_DYNAMICS)
        {
            continue;
        }

        // TODO: Update bone transform
    }
}
#endif

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

    UpdateAnimatedSkeletons(scene, dt_ms);
    UpdateSkinnedGeometry(scene, dt_ms);

#if 0
    UpdateDynamics(scene, dt_ms);
#endif
}
