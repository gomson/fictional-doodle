#include "scene.h"

#include "dynamics.h"
#include "runtimecpp.h"

// assimp includes
#include <cimport.h>
#include <scene.h> // assimp also has a scene.h :/ weird
#include <postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define QFPC_IMPLEMENTATION
#include <qfpc.h>

#include "imgui/imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <SDL.h>

#include <vector>
#include <fstream>
#include <functional>

void LoadScene(Scene* scene)
{
#if 1
    std::string mtlpath = "assets/hellknight/";
    std::string scenepath = "assets/hellknight/hellknight.md5mesh";
#else
    std::string mtlpath = "assets/teapot/";
    std::string scenepath = "assets/teapot/teapot.obj";
#endif

    const aiScene* aiscene = aiImportFile(scenepath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!aiscene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }

    // find all textures that need to be loaded
    std::vector<aiString> diffuseTexturesToLoad;
    for (int materialIdx = 0; materialIdx < (int)aiscene->mNumMaterials; materialIdx++)
    {
        aiMaterial* material = aiscene->mMaterials[materialIdx];
        unsigned int texturecount = aiGetMaterialTextureCount(material, aiTextureType_DIFFUSE);
        if (texturecount > 1)
        {
            fprintf(stderr, "Expecting at most 1 diffuse texture per material, but found %u.\n", texturecount);
            exit(1);
        }

        for (int textureIdxInStack = 0; textureIdxInStack < (int)texturecount; textureIdxInStack++)
        {
            aiString path;
            aiReturn result = aiGetMaterialTexture(material, aiTextureType_DIFFUSE, textureIdxInStack, &path, NULL, NULL, NULL, NULL, NULL, NULL);
            if (result != AI_SUCCESS)
            {
                fprintf(stderr, "aiGetMaterialTexture failed: %s\n", aiGetErrorString());
                exit(1);
            }

            diffuseTexturesToLoad.push_back(path);
        }
    }

    // keep only the unique textures
    std::sort(begin(diffuseTexturesToLoad), end(diffuseTexturesToLoad), [](const aiString& a, const aiString& b) { return strcmp(a.data, b.data) < 0; });
    diffuseTexturesToLoad.erase(std::unique(begin(diffuseTexturesToLoad), end(diffuseTexturesToLoad), [](const aiString& a, const aiString& b) { return strcmp(a.data, b.data) == 0; }), end(diffuseTexturesToLoad));

    // load all the unique textures
    scene->DiffuseTextures.resize(diffuseTexturesToLoad.size());
    for (int textureIdx = 0; textureIdx < (int)diffuseTexturesToLoad.size(); textureIdx++)
    {
        int width, height, comp;
        stbi_set_flip_vertically_on_load(1); // because GL
        std::string fullpath = mtlpath + diffuseTexturesToLoad[textureIdx].data;
        stbi_uc* img = stbi_load(fullpath.c_str(), &width, &height, &comp, 4);
        if (!img)
        {
            fprintf(stderr, "stbi_load: %s\n", stbi_failure_reason());
            exit(1);
        }

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        scene->DiffuseTextures[textureIdx] = texture;

        stbi_image_free(img);
    }

    // hook up list of materials
    for (int materialIdx = 0; materialIdx < (int)aiscene->mNumMaterials; materialIdx++)
    {
        aiMaterial* material = aiscene->mMaterials[materialIdx];

        unsigned int texturecount = aiGetMaterialTextureCount(material, aiTextureType_DIFFUSE);
        if (texturecount == 0)
        {
            scene->MaterialDiffuse0TextureIndex.push_back(-1);
            continue;
        }

        aiString path;
        aiReturn result = aiGetMaterialTexture(material, aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL, NULL);
        if (result != AI_SUCCESS)
        {
            fprintf(stderr, "aiGetMaterialTexture failed: %s\n", aiGetErrorString());
            exit(1);
        }

        auto textureIt = std::lower_bound(begin(diffuseTexturesToLoad), end(diffuseTexturesToLoad), path, [](const aiString& a, const aiString& b) { return strcmp(a.data, b.data) < 0; });
        scene->MaterialDiffuse0TextureIndex.push_back((int)std::distance(begin(diffuseTexturesToLoad), textureIt));
    }

    // figure out how much memory is needed for the scene and check assumptions about data
    int totalNumVertices = 0;
    int totalNumIndices = 0;
    std::vector<GLDrawElementsIndirectCommand> meshDraws(aiscene->mNumMeshes);
    for (int sceneMeshIdx = 0; sceneMeshIdx < (int)aiscene->mNumMeshes; sceneMeshIdx++)
    {
        aiMesh* mesh = aiscene->mMeshes[sceneMeshIdx];

        if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
        {
            fprintf(stderr, "Mesh %d was not made out of triangles\n", sceneMeshIdx);
            exit(1);
        }

        if (!mesh->mTextureCoords[0])
        {
            fprintf(stderr, "Mesh %d didn't have TexCoord0\n", sceneMeshIdx);
            exit(1);
        }

        if (!mesh->mNormals)
        {
            fprintf(stderr, "Mesh %d didn't have normals\n", sceneMeshIdx);
            exit(1);
        }

        if (!mesh->mTangents)
        {
            fprintf(stderr, "Mesh %d didn't have tangents\n", sceneMeshIdx);
            exit(1);
        }

        if (!mesh->mBitangents)
        {
            fprintf(stderr, "Mesh %d didn't have bitangents\n", sceneMeshIdx);
            exit(1);
        }

        meshDraws[sceneMeshIdx].count = mesh->mNumFaces * 3;
        meshDraws[sceneMeshIdx].primCount = 1;
        meshDraws[sceneMeshIdx].firstIndex = totalNumIndices;
        meshDraws[sceneMeshIdx].baseVertex = totalNumVertices;
        meshDraws[sceneMeshIdx].baseInstance = 0; // cannot use since we don't have GL 4.2 on OS X

        totalNumVertices += mesh->mNumVertices;
        totalNumIndices += mesh->mNumFaces * 3;
    }

    // allocate memory for scene's meshes
    std::vector<SceneVertex> meshVertices(totalNumVertices);
    std::vector<glm::uvec3> meshIndices(totalNumIndices);

    // allocate memory for vertex bones
    scene->VertexBones.resize(totalNumVertices);

    // read mesh geometry data
    for (int sceneMeshIdx = 0, currVertexIdx = 0, currIndexIdx = 0; sceneMeshIdx < (int)aiscene->mNumMeshes; sceneMeshIdx++)
    {
        aiMesh* mesh = aiscene->mMeshes[sceneMeshIdx];

        for (int vertexIdx = 0; vertexIdx < (int)mesh->mNumVertices; vertexIdx++)
        {
            SceneVertex vertex;
            vertex.Position = glm::make_vec3(&mesh->mVertices[vertexIdx][0]);
            vertex.TexCoord0 = glm::make_vec2(&mesh->mTextureCoords[0][vertexIdx][0]);
            vertex.Normal = glm::make_vec3(&mesh->mNormals[vertexIdx][0]);
            vertex.Tangent = glm::make_vec3(&mesh->mTangents[vertexIdx][0]);
            vertex.Bitangent = glm::make_vec3(&mesh->mBitangents[vertexIdx][0]);

            meshVertices[currVertexIdx] = vertex;
            currVertexIdx++;
        }

        for (int faceIdx = 0; faceIdx < (int)mesh->mNumFaces; faceIdx++)
        {
            meshIndices[currIndexIdx] = glm::make_vec3(&mesh->mFaces[faceIdx].mIndices[0]);
            currIndexIdx++;
        }

        for (int boneIdx = 0; boneIdx < (int)mesh->mNumBones; boneIdx++)
        {
            aiBone* bone = mesh->mBones[boneIdx];
            std::string boneName(bone->mName.data);

            // Create bone name to ID mapping if none exists.
            if (scene->BoneIDs.find(boneName) == scene->BoneIDs.end())
            {
                // Store the bone's inverse bind pose matrix.
                glm::mat4 transform = glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1));
                scene->BoneIDs[boneName] = (GLuint)scene->BoneInverseBindPoseTransforms.size();
                scene->BoneInverseBindPoseTransforms.push_back(transform);
            }

            GLuint boneID = scene->BoneIDs[boneName];

            for (int weightIdx = 0; weightIdx < (int)bone->mNumWeights; weightIdx++)
            {
                int vertexIdx = meshDraws[sceneMeshIdx].baseVertex + bone->mWeights[weightIdx].mVertexId;

                for (int i = 0; i < scene->VertexBones[vertexIdx].Weights.length(); i++)
                {
                    if (scene->VertexBones[vertexIdx].Weights[i] == 0.0)
                    {
                        scene->VertexBones[vertexIdx].Weights[i] = bone->mWeights[weightIdx].mWeight;
                        scene->VertexBones[vertexIdx].BoneIDs[i] = boneID;
                        break;
                    }
                }
            }
        }
    }

    // Upload geometry data
    glGenBuffers(1, &scene->VBO);
    glBindBuffer(GL_ARRAY_BUFFER, scene->VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(meshVertices[0]), meshVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &scene->EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshIndices.size() * sizeof(meshIndices[0]), meshIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Upload animation data
    glGenBuffers(1, &scene->BoneVBO);
    glBindBuffer(GL_ARRAY_BUFFER, scene->BoneVBO);
    glBufferData(GL_ARRAY_BUFFER, scene->VertexBones.size() * sizeof(scene->VertexBones[0]), scene->VertexBones.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create texture buffer for skinning transformation
    glGenBuffers(1, &scene->BoneTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, scene->BoneTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    // Create texture and attach buffer object to texture
    glGenTextures(1, &scene->BoneTex);
    glBindTexture(GL_TEXTURE_BUFFER, scene->BoneTex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, scene->BoneTBO);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    // Setup VAO
    glGenVertexArrays(1, &scene->VAO);
    glBindVertexArray(scene->VAO);

    // Vertex geometry
    glBindBuffer(GL_ARRAY_BUFFER, scene->VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SceneVertex), (GLvoid*)offsetof(SceneVertex, Position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SceneVertex), (GLvoid*)offsetof(SceneVertex, TexCoord0));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SceneVertex), (GLvoid*)offsetof(SceneVertex, Normal));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(SceneVertex), (GLvoid*)offsetof(SceneVertex, Tangent));
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(SceneVertex), (GLvoid*)offsetof(SceneVertex, Bitangent));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Vertex bone weights
    glBindBuffer(GL_ARRAY_BUFFER, scene->BoneVBO);
    glVertexAttribIPointer(5, 4, GL_UNSIGNED_BYTE, sizeof(VertexWeights), (GLvoid*)offsetof(VertexWeights, BoneIDs));
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(VertexWeights), (GLvoid*)offsetof(VertexWeights, Weights));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // texcoord0
    glEnableVertexAttribArray(2); // normal
    glEnableVertexAttribArray(3); // tangent
    glEnableVertexAttribArray(4); // bitangent
    glEnableVertexAttribArray(5); // bone IDs
    glEnableVertexAttribArray(6); // bone weights
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->EBO);
    glBindVertexArray(0);

    // Create storage for skinning transformations
    scene->BoneSkinningTransforms.resize(scene->BoneInverseBindPoseTransforms.size());

    // Compute inverse model transformation used to compute skinning transformation
    glm::mat4 inverseModelTransform = glm::inverseTranspose(glm::make_mat4(&aiscene->mRootNode->mTransformation.a1));

    // depth first traversal of scene to build draws and skinning transformations
    std::function<void(const aiNode*, glm::mat4)> addNode;
    addNode = [&addNode, &meshDraws, &aiscene, &scene, &inverseModelTransform](const aiNode* node, glm::mat4 transform)
    {
        transform = transform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
        std::string boneName(node->mName.data);

        // if the node is a bone, compute and store its skinning transformation
        if (scene->BoneIDs.find(boneName) != scene->BoneIDs.end())
        {
            GLubyte boneID = scene->BoneIDs[boneName];
            scene->BoneSkinningTransforms[boneID] = inverseModelTransform * transform * scene->BoneInverseBindPoseTransforms[boneID];
        }

        // add draws for all meshes assigned to this node
        for (int nodeMeshIdx = 0; nodeMeshIdx < (int)node->mNumMeshes; nodeMeshIdx++)
        {
            GLDrawElementsIndirectCommand draw = meshDraws[node->mMeshes[nodeMeshIdx]];
            scene->NodeDrawCmds.push_back(draw);
            scene->NodeModelWorldTransforms.push_back(transform);
            scene->NodeMaterialIDs.push_back(aiscene->mMeshes[node->mMeshes[nodeMeshIdx]]->mMaterialIndex);
        }

        // traverse all children
        for (int childIdx = 0; childIdx < (int)node->mNumChildren; childIdx++)
        {
            addNode(node->mChildren[childIdx], transform);
        }
    };

    addNode(aiscene->mRootNode, glm::mat4());

    aiReleaseImport(aiscene);

    // Upload initial skinning transformations
    glBindBuffer(GL_TEXTURE_BUFFER, scene->BoneTBO);
    glBufferData(GL_TEXTURE_BUFFER, scene->BoneSkinningTransforms.size() * sizeof(scene->BoneSkinningTransforms[0]), scene->BoneSkinningTransforms.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void InitScene(Scene* scene)
{
    LoadScene(scene);

    scene->AllShadersOK = false;

    // initial camera position
    scene->CameraPosition = glm::vec3(100, 100, 100);
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
    if (reload(&scene->SceneSP))
    {
        if (getU(&scene->SceneSP_ViewLoc, "View") ||
            getU(&scene->SceneSP_MVLoc, "ModelView") ||
            getU(&scene->SceneSP_MVPLoc, "ModelViewProjection") ||
            getU(&scene->SceneSP_Diffuse0Loc, "Diffuse0") ||
            getU(&scene->SceneSP_BoneTransformsLoc, "BoneTransforms"))
        {
            return;
        }
    }

    if (!programZero)
    {
        scene->AllShadersOK = true;
    }
}

void UpdateScene(Scene* scene, uint32_t dt_ms)
{
    ReloadShaders(scene);
    
    if (!scene->AllShadersOK)
    {
        return;
    }

    float dt_s = dt_ms / 1000.0f;

    // for testing the gui
    ImGui::ShowTestWindow();
    ImGui::ShowMetricsWindow();

    // Update camera
    {
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
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_E],
            !scene->EnableCamera ? 0 : keyboardState[SDL_SCANCODE_Q]);
    }

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

    if (pfnSimulateDynamics)
    {
        pfnSimulateDynamics(0, NULL, NULL, NULL, NULL, 0, 0, NULL, 0, NULL, NULL);
    }
}
