#include "scene.h"

#include "dynamics.h"
#include "runtimecpp.h"
#include "mysdl_dpi.h"

#include "sceneloader.h"

#define QFPC_IMPLEMENTATION
#include <qfpc.h>

#include "imgui/imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <SDL.h>

#include <functional>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

static int AddSkinnedMesh(
    Scene* scene,
    int bindPoseMeshID,
    int initialAnimSequenceID)
{
    SkinnedMesh skinnedMesh;

    BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[bindPoseMeshID];
    int skeletonID = bindPoseMesh.SkeletonID;
    Skeleton& skeleton = scene->Skeletons[skeletonID];

    skinnedMesh.BindPoseMeshID = bindPoseMeshID;
    skinnedMesh.CurrAnimSequenceID = initialAnimSequenceID;
    skinnedMesh.CurrTimeMillisecond = 0;
    skinnedMesh.CPUBoneTransforms.resize(skeleton.NumBones);
    skinnedMesh.BoneControls.resize(skeleton.NumBones);

    glGenBuffers(1, &skinnedMesh.BoneTransformTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, skinnedMesh.BoneTransformTBO);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(SkinningMatrix) * skeleton.NumBones, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    glGenTextures(1, &skinnedMesh.BoneTransformTO);
    glBindTexture(GL_TEXTURE_BUFFER, skinnedMesh.BoneTransformTO);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, skinnedMesh.BoneTransformTBO);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexCoordVertex), (GLvoid*)offsetof(TexCoordVertex, TexCoord0));
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
    int skinnedMeshID)
{
    Ragdoll ragdoll;
    ragdoll.SkinnedMeshID = skinnedMeshID;
    ragdoll.OldBufferIndex = -1;

    SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[skinnedMeshID];
    int bindPoseMeshID = skinnedMesh.BindPoseMeshID;
    BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[bindPoseMeshID];
    int skeletonID = bindPoseMesh.SkeletonID;
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

void InitScene(Scene* scene)
{
    std::string assetFolder = "assets/";

    std::string hellknight_modelFolder = "hellknight/";
    std::string hellknight_meshFile = "hellknight.md5mesh";
    std::vector<std::string> hellknight_animFiles{
        "attack3.md5anim",
        "chest.md5anim",
        "headpain.md5anim",
        "idle2.md5anim",
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

    LoadMD5Mesh(
        scene, 
        assetFolder.c_str(), hellknight_modelFolder.c_str(),
        hellknight_meshFile.c_str());
    int hellknightSkeletonID = -1; // TODO: query this
    for (const std::string& animFile : hellknight_animFiles)
    {
        LoadMD5Anim(
            scene, 
            hellknightSkeletonID, 
            assetFolder.c_str(), hellknight_modelFolder.c_str(),
            animFile.c_str());
    }

    int hellknightBindPoseMeshID = -1; // TODO: query this
    int hellknightInitialAnimSequenceID = -1; // TODO: query this
    int hellknightSkinnedMeshID = AddSkinnedMesh(scene, hellknightBindPoseMeshID, hellknightInitialAnimSequenceID);

    int hellknightRagdollID = AddRagdoll(scene, hellknightSkinnedMeshID);

    scene->AllShadersOK = false;

    // initial camera position
    scene->CameraPosition = glm::vec3(85.9077225f, 200.844162f, 140.049072f);
    scene->CameraQuaternion = glm::vec4(-0.351835f, 0.231701f, 0.090335f, 0.902411f);
    scene->EnableCamera = true;
}

static void ReloadShaders(Scene* scene)
{
    // Convenience functions to make things more concise
    GLuint sp;
    bool programZero = false;
    auto reload = [&sp, &programZero](ReloadableProgram* program)
    {
        bool newProgram = ReloadProgram(program);
        sp = program->Handle;
        if (sp == 0) programZero = true;
        return newProgram;
    };

    auto getU = [&sp](GLint* result, const char* name)
    {
        *result = glGetUniformLocation(sp, name);
        if (*result == -1)
        {
            fprintf(stderr, "Couldn't find uniform %s\n", name);
        }
        return *result == -1;
    };

    scene->AllShadersOK = false;

    // Reload shaders & uniforms
    if (reload(&scene->SkinningSP))
    {
        if (getU(&scene->SkinningSP_BoneTransformsLoc, "BoneTransforms"))
        {
            return;
        }
    }

    if (reload(&scene->SceneSP))
    {
        if (getU(&scene->SceneSP_ModelViewLoc, "ModelView") ||
            getU(&scene->SceneSP_ModelViewProjectionLoc, "ModelViewProjection") ||
            getU(&scene->SceneSP_WorldViewLoc, "WorldView") ||
            getU(&scene->SceneSP_DiffuseTextureLoc, "DiffuseTexture") ||
            getU(&scene->SceneSP_SpecularTextureLoc, "SpecularTexture") ||
            getU(&scene->SceneSP_NormalTextureLoc, "NormalTexture"))
        {
            return;
        }
    }

    if (!programZero)
    {
        scene->AllShadersOK = true;
    }
}

#if 0
static void UpdateSceneDynamics(Scene* scene, uint32_t dt_ms)
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
        int currSelectedSkinnedMesh = 0;

        if (currSelectedSkinnedMesh < scene->SkinnedMeshes.size())
        {
            SkinnedMesh& skinnedMesh = scene->SkinnedMeshes[currSelectedSkinnedMesh];
            int bindPoseMeshID = skinnedMesh.BindPoseMeshID;
            BindPoseMesh& bindPoseMesh = scene->BindPoseMeshes[bindPoseMeshID];
            int skeletonID = bindPoseMesh.SkeletonID;

            // find all animations compatible with the skeleton
            std::vector<const char*> animSequenceNames;
            std::vector<int> animSequenceIDs;
            int currAnimSequenceIndexInListbox = -1;
            for (int animSequenceID = 0; animSequenceID < (int)scene->AnimSequences.size(); animSequenceID++)
            {
                const AnimSequence& animSequence = scene->AnimSequences[animSequenceID];
                if (animSequence.SkeletonID == skeletonID)
                {
                    animSequenceNames.push_back(animSequence.Name.c_str());
                    animSequenceIDs.push_back(animSequenceID);
                    if (animSequenceID == skinnedMesh.CurrAnimSequenceID)
                    {
                        currAnimSequenceIndexInListbox = (int)animSequenceNames.size() - 1;
                    }
                }
            }

            // Display list to select animation
            if (!animSequenceNames.empty())
            {
                ImGui::Text("Animation Sequence");
                if (ImGui::ListBox("##animsequences", &currAnimSequenceIndexInListbox, animSequenceNames.data(), (int)animSequenceNames.size()))
                {
                    skinnedMesh.CurrAnimSequenceID = animSequenceIDs[currAnimSequenceIndexInListbox];
                    skinnedMesh.CurrTimeMillisecond = 0;
                }
            }
        }
    }
    ImGui::End();
}

#if 0
static void UpdateSceneSkinnedGeometry(Scene* scene, uint32_t dt_ms)
{
    // Update matrix palette based on current sequence/frame of animation
    {
        for (int skinnedMeshIdx = 0; skinnedMeshIdx < (int)scene->SkinnedMeshDrawCommands.size(); skinnedMeshIdx++)
        {

        }
    }

    // Skin vertices using the matrix palette and store them with transform feedback
    {
        glUseProgram(scene->SkinningSP.Handle);
        glBindVertexArray(scene->SkinningVAO);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, scene->SkinningTFO);
        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS); // capture points so triangles aren't unfolded

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, scene->SkinningMatrixPaletteTexture);
        glUniform1i(scene->SkinningSP_BoneTransformsLoc, 0);

        // skin every mesh in the scene one by one
        for (int skinnedMeshIdx = 0; skinnedMeshIdx < (int)scene->SkinnedMeshDrawCommands.size(); skinnedMeshIdx++)
        {
            int nodeID = scene->SkinnedMeshNodeIDs[skinnedMeshIdx];

            GLDrawElementsIndirectCommand draw = scene->SkinnedMeshDrawCommands[skinnedMeshIdx];
            int bindPoseID = scene->SkinnedMeshBindPoseIDs[skinnedMeshIdx];
            int numVertices = scene->BindBoseMeshNumVertices[bindPoseID];
            glDrawArrays(GL_POINTS, draw.baseVertex, numVertices);
        }

        glEndTransformFeedback();
        glDisable(GL_RASTERIZER_DISCARD);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

        glBindVertexArray(0);
        glUseProgram(0);
    }
}
#endif

void UpdateScene(Scene* scene, SDL_Window* window, uint32_t dt_ms)
{
    ReloadShaders(scene);
    
    if (!scene->AllShadersOK)
    {
        return;
    }

    ShowSystemInfoGUI(scene);
    ShowToolboxGUI(scene, window);

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

#if 0
    UpdateSceneSkinnedGeometry(scene, dt_ms);

    UpdateSceneDynamics(scene, dt_ms);
#endif
}
