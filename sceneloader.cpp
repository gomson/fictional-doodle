#include "sceneloader.h"

#include "scene.h"

// assimp includes
#include <cimport.h>
// assimp also has a scene.h. weird.
#include <scene.h>
#include <postprocess.h>

#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vector>
#include <string>
#include <functional>

static void LoadMaterials(
    Scene* scene,
    const char* assetFolder, const char* modelFolder,
    aiMaterial** materials, int numMaterials)
{
    // Only the texture types we care about
    aiTextureType textureTypes[] = {
        aiTextureType_DIFFUSE,
        aiTextureType_SPECULAR,
        aiTextureType_NORMALS
    };
    static const int numTextureTypes = sizeof(textureTypes) / sizeof(*textureTypes);

    // find all textures that need to be loaded
    std::vector<std::vector<std::string>> texturesToLoad(numTextureTypes);
    for (int materialIdx = 0; materialIdx < numMaterials; materialIdx++)
    {
        aiMaterial* material = materials[materialIdx];

        for (int textureTypeIdx = 0; textureTypeIdx < (int)std::size(textureTypes); textureTypeIdx++)
        {
            int textureCount = (int)aiGetMaterialTextureCount(material, textureTypes[textureTypeIdx]);

            for (int textureIdxInStack = 0; textureIdxInStack < (int)textureCount; textureIdxInStack++)
            {
                aiString path;
                aiReturn result = aiGetMaterialTexture(material, textureTypes[textureTypeIdx], textureIdxInStack, &path, NULL, NULL, NULL, NULL, NULL, NULL);
                if (result != AI_SUCCESS)
                {
                    fprintf(stderr, "aiGetMaterialTexture failed: %s\n", aiGetErrorString());
                    exit(1);
                }

                texturesToLoad[textureTypeIdx].push_back(std::string(modelFolder) + path.C_Str());
            }
        }
    }

    std::vector<std::unordered_map<std::string, int>*> textureNameToIDs
    {
        &scene->DiffuseTextureNameToID,
        &scene->SpecularTextureNameToID,
        &scene->NormalTextureNameToID,
    };
    assert(textureNameToIDs.size() == numTextureTypes);

    // keep only the unique textures
    for (int textureTypeIdx = 0; textureTypeIdx < numTextureTypes; textureTypeIdx++)
    {
        // remove textures that appear more than once in this list
        std::sort(begin(texturesToLoad[textureTypeIdx]), end(texturesToLoad[textureTypeIdx]));
        texturesToLoad[textureTypeIdx].erase(
            std::unique(begin(texturesToLoad[textureTypeIdx]), end(texturesToLoad[textureTypeIdx])), 
            end(texturesToLoad[textureTypeIdx]));

        // remove textures that were loaded by previous meshes
        texturesToLoad[textureTypeIdx].erase(
            std::remove_if(begin(texturesToLoad[textureTypeIdx]), end(texturesToLoad[textureTypeIdx]),
                [&textureNameToIDs, textureTypeIdx](const std::string& s) {
            return textureNameToIDs[textureTypeIdx]->find(s) != textureNameToIDs[textureTypeIdx]->end();
        }), end(texturesToLoad[textureTypeIdx]));
    }

    // load all the unique textures
    for (int textureTypeIdx = 0; textureTypeIdx < numTextureTypes; textureTypeIdx++)
    {
        for (int textureToLoadIdx = 0; textureToLoadIdx < (int)texturesToLoad.size(); textureToLoadIdx++)
        {
            const std::string& fullpath = assetFolder + texturesToLoad[textureTypeIdx][textureToLoadIdx];

            int req_comp = -1;
            if (textureTypes[textureTypeIdx] == aiTextureType_DIFFUSE)
            {
                req_comp = 4;
            }
            else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
            {
                req_comp = 0;
            }
            else if (textureTypes[textureTypeIdx] == aiTextureType_NORMALS)
            {
                req_comp = 4;
            }
            else
            {
                fprintf(stderr, "%s: Unhandled texture type %d\n", fullpath.c_str(), textureTypes[textureTypeIdx]);
                exit(1);
            }

            int width, height, comp;
            stbi_set_flip_vertically_on_load(1); // because GL
            stbi_uc* img = stbi_load(fullpath.c_str(), &width, &height, &comp, req_comp);
            if (!img)
            {
                fprintf(stderr, "stbi_load (%s): %s\n", fullpath.c_str(), stbi_failure_reason());
            }
            else
            {
                GLenum srcDataFormat[4] = {
                    GL_RED, GL_RG, GL_RGB, GL_RGBA
                };

                GLuint texture;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);

                if (textureTypes[textureTypeIdx] == aiTextureType_DIFFUSE)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, srcDataFormat[comp - 1], GL_UNSIGNED_BYTE, img);
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, srcDataFormat[comp - 1], GL_UNSIGNED_BYTE, img);
                    GLint swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
                    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_NORMALS)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8_SNORM, width, height, 0, srcDataFormat[comp - 1], GL_UNSIGNED_BYTE, img);
                }
                else
                {
                    fprintf(stderr, "%s: Unhandled texture type %d\n", fullpath.c_str(), textureTypes[textureTypeIdx]);
                    exit(1);
                }

                glGenerateMipmap(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);

                int id = -1;

                if (textureTypes[textureTypeIdx] == aiTextureType_DIFFUSE)
                {
                    DiffuseTexture d;
                    d.TO = texture;
                    scene->DiffuseTextures.push_back(d);
                    id = (int)scene->DiffuseTextures.size() - 1;
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
                {
                    SpecularTexture s;
                    s.TO = texture;
                    scene->SpecularTextures.push_back(s);
                    id = (int)scene->SpecularTextures.size() - 1;
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_NORMALS)
                {
                    NormalTexture n;
                    n.TO = texture;
                    scene->NormalTextures.push_back(n);
                    id = (int)scene->NormalTextures.size() - 1;
                }
                else
                {
                    fprintf(stderr, "%s: Unhandled texture type %d\n", fullpath.c_str(), textureTypes[textureTypeIdx]);
                    exit(1);
                }

                textureNameToIDs[textureTypeIdx]->emplace(texturesToLoad[textureTypeIdx][textureToLoadIdx], id);

                stbi_image_free(img);
            }

            stbi_set_flip_vertically_on_load(0);
        }
    }

    // hook up list of materials
    for (int materialIdx = 0; materialIdx < numMaterials; materialIdx++)
    {
        aiMaterial* material = materials[materialIdx];

        Material newMat;

        for (int textureTypeIdx = 0; textureTypeIdx < (int)std::size(textureTypes); textureTypeIdx++)
        {
            int textureCount = (int)aiGetMaterialTextureCount(material, textureTypes[textureTypeIdx]);

            for (int textureIdxInStack = 0; textureIdxInStack < (int)textureCount; textureIdxInStack++)
            {
                aiString path;
                aiReturn result = aiGetMaterialTexture(material, textureTypes[textureTypeIdx], textureIdxInStack, &path, NULL, NULL, NULL, NULL, NULL, NULL);
                if (result != AI_SUCCESS)
                {
                    fprintf(stderr, "aiGetMaterialTexture failed: %s\n", aiGetErrorString());
                    exit(1);
                }

                std::string modelpath = std::string(modelFolder) + path.C_Str();
                auto foundNameToID = textureNameToIDs[textureTypeIdx]->find(modelpath);
                if (foundNameToID != textureNameToIDs[textureTypeIdx]->end())
                {
                    int textureID = foundNameToID->second;

                    if (textureTypes[textureTypeIdx] == aiTextureType_DIFFUSE)
                    {
                        newMat.DiffuseTextureIDs.push_back(textureID);
                    }
                    else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
                    {
                        newMat.SpecularTextureIDs.push_back(textureID);
                    }
                    else if (textureTypes[textureTypeIdx] == aiTextureType_NORMALS)
                    {
                        newMat.NormalTextureIDs.push_back(textureID);
                    }
                    else
                    {
                        fprintf(stderr, "%s: Unhandled texture type %d\n", modelpath.c_str(), textureTypes[textureTypeIdx]);
                        exit(1);
                    }
                }
            }
        }

        scene->Materials.push_back(newMat);
    }
}

static void LoadMeshes(
    Scene* scene,
    const char* modelFolder,
    aiMesh** meshes, int numMeshes)
{
    std::vector<int> indexCounts(numMeshes);
    std::vector<int> vertexCounts(numMeshes);

    for (int meshIdx = 0; meshIdx < numMeshes; meshIdx++)
    {
        aiMesh* mesh = meshes[meshIdx];

        if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
        {
            fprintf(stderr, "Mesh %s was not made out of triangles\n", mesh->mName.C_Str());
            exit(1);
        }

        if (!mesh->mTextureCoords[0])
        {
            fprintf(stderr, "Mesh %s didn't have TexCoord0\n", mesh->mName.C_Str());
            exit(1);
        }

        if (!mesh->mNormals)
        {
            fprintf(stderr, "Mesh %s didn't have normals\n", mesh->mName.C_Str());
            exit(1);
        }

        if (!mesh->mTangents)
        {
            fprintf(stderr, "Mesh %s didn't have tangents\n", mesh->mName.C_Str());
            exit(1);
        }

        if (!mesh->mBitangents)
        {
            fprintf(stderr, "Mesh %s didn't have bitangents\n", mesh->mName.C_Str());
            exit(1);
        }
    }
}

static void ParseMD5MeshNode(
    Scene* scene,
    aiNode* ainode)
{
    if (strcmp(ainode->mName.C_Str(), "<MD5_Mesh>") != 0)
    {
        fprintf(stderr, "Expected <MD5_Mesh>, got %s\n", ainode->mName.C_Str());
        exit(1);
    }
}

static void ParseMD5HierarchyNode(
    Scene* scene,
    aiNode* ainode)
{
    if (strcmp(ainode->mName.C_Str(), "<MD5_Hierarchy>") != 0)
    {
        fprintf(stderr, "Expected <MD5_Hierarchy>, got %s\n", ainode->mName.C_Str());
        exit(1);
    }
}

static void ParseMD5RootNode(
    Scene* scene,
    aiNode* ainode)
{
    if (strcmp(ainode->mName.C_Str(), "<MD5_Root>") != 0)
    {
        fprintf(stderr, "Expected <MD5_Root>, got %s\n", ainode->mName.C_Str());
        exit(1);
    }

    // traverse all children
    for (int childIdx = 0; childIdx < (int)ainode->mNumChildren; childIdx++)
    {
        aiNode* child = ainode->mChildren[childIdx];

        if (strcmp(child->mName.C_Str(), "<MD5_Mesh>") == 0)
        {
            ParseMD5MeshNode(scene, child);
        }
        else if (strcmp(child->mName.C_Str(), "<MD5_Hierarchy>") == 0)
        {
            ParseMD5HierarchyNode(scene, child);
        }
        else
        {
            fprintf(stderr, "Unexpected root node child: %s\n", child->mName.C_Str());
            exit(1);
        }
    }
}

void LoadMD5Mesh(
    Scene* scene,
    const char* assetFolder, const char* modelFolder,
    const char* meshFile)
{
    std::string meshpath = std::string(assetFolder) + modelFolder + meshFile;
    const aiScene* aiscene = aiImportFile(meshpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!aiscene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }

    LoadMaterials(scene, assetFolder, modelFolder, aiscene->mMaterials, (int)aiscene->mNumMaterials);

    ParseMD5RootNode(scene, aiscene->mRootNode);
    
    // LoadMeshes(scene, modelFolder, aiscene->mMeshes, (int)aiscene->mNumMeshes);

    aiReleaseImport(aiscene);
}

void LoadMD5Anim(
    Scene* scene,
    int skeletonID,
    const char* assetFolder, const char* modelFolder,
    const char* animFile)
{
    std::string fullpath = std::string(assetFolder) + modelFolder + animFile;
    const aiScene* animScene = aiImportFile(fullpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);

    // Check if file exists and was successfully parsed
    if (!animScene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        return;
    }

    // Check if file contains an animation
    if (!animScene->mNumAnimations)
    {
        fprintf(stderr, "No animations: %s\n", fullpath.c_str());
        return;
    }

    // TODO: Check if animation is valid for the skeleton

    // One animation per file
    const aiAnimation* animation = animScene->mAnimations[0];

    AnimSequence animSequence;
    animSequence.Name = std::string(modelFolder) + animFile;
    animSequence.SkeletonID = skeletonID;
    animSequence.FramesPerSecond = (int)animation->mTicksPerSecond;
    animSequence.NumFrames = (int)animSequence->mDuration;

    // Allocate storage for each bone
    animSequence.BoneBaseFrame.resize(animation->mNumChannels);
    animSequence.BoneChannelBits.resize(animation->mNumChannels);
    animSequence.BoneFrameDataOffsets.resize(animation->mNumChannels);

    int numFrameComponents = 0;

    // For each bone
    for (int bone = 0; bone < (int)animation->mNumChannels; bone++)
    {
        aiNodeAnim* boneAnim = animation->mChannels[bone];

        // Base frame bone position
        aiVector3D baseT = boneAnim->mPositionKeys[0].mValue;
        animSequence.BoneBaseFrame[bone].T = glm::vec3(baseT.x, baseT.y, baseT.z);

        // Base frame bone orientation
        aiQuaternion baseQ = boneAnim->mRotationKeys[0].mValue;
        animSequence.BoneBaseFrame[bone].Q = glm::quat(baseQ.w, baseQ.x, baseQ.y, baseQ.z);

        // Find which position components of this bone are animated
        glm::bvec3 isAnimatedT(false);
        for (int i = 1; i < (int)boneAnim->mNumPositionKeys; i++)
        {
            aiVector3D v0 = boneAnim->mPositionKeys[i - 1].mValue;
            aiVector3D v1 = boneAnim->mPositionKeys[i].mValue;

            if (v0.x != v1.x) { isAnimatedT.x = true; }
            if (v0.y != v1.y) { isAnimatedT.y = true; }
            if (v0.z != v1.z) { isAnimatedT.z = true; }

            if (all(isAnimatedT))
            {
                break;
            }
        }

        // Find which orientation components of this bone are animated
        glm::bvec3 isAnimatedQ(false);
        for (int i = 1; i < (int)boneAnim->mNumRotationKeys; i++)
        {
            aiQuaternion q0 = boneAnim->mRotationKeys[i - 1].mValue;
            aiQuaternion q1 = boneAnim->mRotationKeys[i].mValue;

            if (q0.x != q1.x) { isAnimatedQ.x = true; }
            if (q0.y != q1.y) { isAnimatedQ.y = true; }
            if (q0.z != q1.z) { isAnimatedQ.z = true; }

            if (all(isAnimatedQ))
            {
                break;
            }
        }

        // Encode which position and orientation components are animated
        animSequence.BoneChannelBits[bone] |= isAnimatedT.x ? ANIMCHANNEL_TX_BIT : 0;
        animSequence.BoneChannelBits[bone] |= isAnimatedT.y ? ANIMCHANNEL_TY_BIT : 0;
        animSequence.BoneChannelBits[bone] |= isAnimatedT.z ? ANIMCHANNEL_TZ_BIT : 0;
        animSequence.BoneChannelBits[bone] |= isAnimatedQ.x ? ANIMCHANNEL_QX_BIT : 0;
        animSequence.BoneChannelBits[bone] |= isAnimatedQ.y ? ANIMCHANNEL_QY_BIT : 0;
        animSequence.BoneChannelBits[bone] |= isAnimatedQ.z ? ANIMCHANNEL_QZ_BIT : 0;

        animSequence.BoneFrameDataOffsets[bone] = numFrameComponents;

        // Update offset for the next bone
        for (uint8_t bits = animSequence.BoneChannelBits[bone]; bits != 0; bits &= (bits - 1))
        {
            numFrameComponents++;
        }
    }

    // Create storage for frame data
    animSequence.BoneFrameData.resize(animSequence.NumFrames * numFrameComponents);
    animSequence.NumFrameComponents = numFrameComponents;

    // Generate encoded frame data
    for (int bone = 0; bone < (int)animation->mNumChannels; bone++)
    {
        for (int frame = 0; frame < animSequence.NumFrames; frame++)
        {
            int off = animSequence.BoneFrameDataOffsets[bone];
            uint8_t bits = animSequence.BoneChannelBits[bone];

            if (bits & ANIMCHANNEL_TX_BIT)
            {
                int index = frame * numFrameComponents + off++;
                animSequence.BoneFrameData[index] = animation->mChannels[bone]->mPositionKeys[frame].mValue.x;
            }
            if (bits & ANIMCHANNEL_TY_BIT)
            {
                int index = frame * numFrameComponents + off++;
                animSequence.BoneFrameData[index] = animation->mChannels[bone]->mPositionKeys[frame].mValue.y;
            }
            if (bits & ANIMCHANNEL_TZ_BIT)
            {
                int index = frame * numFrameComponents + off++;
                animSequence.BoneFrameData[index] = animation->mChannels[bone]->mPositionKeys[frame].mValue.z;
            }
            if (bits & ANIMCHANNEL_QX_BIT)
            {
                int index = frame * numFrameComponents + off++;
                animSequence.BoneFrameData[index] = animation->mChannels[bone]->mRotationKeys[frame].mValue.x;
            }
            if (bits & ANIMCHANNEL_QY_BIT)
            {
                int index = frame * numFrameComponents + off++;
                animSequence.BoneFrameData[index] = animation->mChannels[bone]->mRotationKeys[frame].mValue.y;
            }
            if (bits & ANIMCHANNEL_QZ_BIT)
            {
                int index = frame * numFrameComponents + off++;
                animSequence.BoneFrameData[index] = animation->mChannels[bone]->mRotationKeys[frame].mValue.z;
            }
        }
    }

    scene->AnimSequences.push_back(animSequence);

    int animSequenceID = (int)scene->AnimSequences.size() - 1;

    scene->AnimSequenceNameToID.emplace(scene->AnimSequences.back().Name, animSequenceID);

    aiReleaseImport(animScene);
}

// TODO: Remove and replace
#if 0
void LoadScene(Scene* scene)
{
    /*
#if 1
    std::string scenepath = "assets/hellknight/";
    std::string modelname = "hellknight.md5mesh";
#else
    std::string scenepath = "assets/teapot/";
    std::string modelname = "teapot.obj";
#endif

    std::string modelpath = scenepath + modelname;
    const aiScene* aiscene = aiImportFile(modelpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!aiscene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }

    // figure out how much memory is needed for the scene and check assumptions about data
    int totalNumVertices = 0;
    int totalNumIndices = 0;
    scene->SkinnedMeshDrawCommands.resize(aiscene->mNumMeshes);
    for (int skinnedMeshIdx = 0; skinnedMeshIdx < (int)aiscene->mNumMeshes; skinnedMeshIdx++)
    {
        aiMesh* mesh = aiscene->mMeshes[skinnedMeshIdx];

        if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
        {
            fprintf(stderr, "Mesh %d was not made out of triangles\n", skinnedMeshIdx);
            exit(1);
        }

        if (!mesh->mTextureCoords[0])
        {
            fprintf(stderr, "Mesh %d didn't have TexCoord0\n", skinnedMeshIdx);
            exit(1);
        }

        if (!mesh->mNormals)
        {
            fprintf(stderr, "Mesh %d didn't have normals\n", skinnedMeshIdx);
            exit(1);
        }

        if (!mesh->mTangents)
        {
            fprintf(stderr, "Mesh %d didn't have tangents\n", skinnedMeshIdx);
            exit(1);
        }

        if (!mesh->mBitangents)
        {
            fprintf(stderr, "Mesh %d didn't have bitangents\n", skinnedMeshIdx);
            exit(1);
        }

        GLDrawElementsIndirectCommand& draw = scene->SkinnedMeshDrawCommands[skinnedMeshIdx];
        draw.count = mesh->mNumFaces * 3;
        draw.primCount = 1;
        draw.firstIndex = totalNumIndices;
        draw.baseVertex = totalNumVertices;
        draw.baseInstance = 0; // cannot use since we don't have GL 4.2 on OS X

        totalNumVertices += mesh->mNumVertices;
        totalNumIndices += mesh->mNumFaces * 3;
    }

    // allocate memory for scene's skinning meshes
    std::vector<SkinningVertex> skinningVertices(totalNumVertices);
    std::vector<glm::uvec3> skinningIndices(totalNumIndices);

    // read mesh geometry data
    for (int sceneMeshIdx = 0, currVertexIdx = 0, currIndexIdx = 0; sceneMeshIdx < (int)aiscene->mNumMeshes; sceneMeshIdx++)
    {
        aiMesh* mesh = aiscene->mMeshes[sceneMeshIdx];

        for (int vertexIdx = 0; vertexIdx < (int)mesh->mNumVertices; vertexIdx++)
        {
            SkinningVertex vertex;
            vertex.Position = glm::make_vec3(&mesh->mVertices[vertexIdx][0]);
            vertex.TexCoord0 = glm::make_vec2(&mesh->mTextureCoords[0][vertexIdx][0]);
            vertex.Normal = glm::make_vec3(&mesh->mNormals[vertexIdx][0]);
            vertex.Tangent = glm::make_vec3(&mesh->mTangents[vertexIdx][0]);
            vertex.Bitangent = glm::make_vec3(&mesh->mBitangents[vertexIdx][0]);

            skinningVertices[currVertexIdx] = vertex;
            currVertexIdx++;
        }

        for (int faceIdx = 0; faceIdx < (int)mesh->mNumFaces; faceIdx++)
        {
            skinningIndices[currIndexIdx] = glm::make_vec3(&mesh->mFaces[faceIdx].mIndices[0]);
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
                scene->BoneIDs[boneName] = (int)scene->BoneInverseBindPoseTransforms.size();
                scene->BoneInverseBindPoseTransforms.push_back(transform);
            }

            int boneID = scene->BoneIDs[boneName];

            for (int weightIdx = 0; weightIdx < (int)bone->mNumWeights; weightIdx++)
            {
                int vertexIdx = scene->SkinnedMeshDrawCommands[sceneMeshIdx].baseVertex + bone->mWeights[weightIdx].mVertexId;

                for (int i = 0; i < skinningVertices[vertexIdx].Weights.length(); i++)
                {
                    if (skinningVertices[vertexIdx].Weights[i] == 0.0)
                    {
                        skinningVertices[vertexIdx].Weights[i] = bone->mWeights[weightIdx].mWeight;
                        skinningVertices[vertexIdx].BoneIDs[i] = boneID;
                        break;
                    }
                }
            }
        }

        // Number of vertices for this bind pose mesh
        scene->BindBoseMeshNumVertices.push_back(mesh->mNumVertices);
    }

    // Upload bind pose geometry
    glGenBuffers(1, &scene->BindPoseMeshVBO);
    glBindBuffer(GL_ARRAY_BUFFER, scene->BindPoseMeshVBO);
    glBufferData(GL_ARRAY_BUFFER, skinningVertices.size() * sizeof(skinningVertices[0]), skinningVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &scene->BindPoseMeshEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->BindPoseMeshEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, skinningIndices.size() * sizeof(skinningIndices[0]), skinningIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Create texture buffer for skinning transformation
    glGenBuffers(1, &scene->SkinningMatrixPaletteTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, scene->SkinningMatrixPaletteTBO);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    // Create texture and attach buffer object to texture
    glGenTextures(1, &scene->SkinningMatrixPaletteTexture);
    glBindTexture(GL_TEXTURE_BUFFER, scene->SkinningMatrixPaletteTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, scene->SkinningMatrixPaletteTBO);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    // Buffer for output of skinning transformation
    glGenBuffers(1, &scene->SkinningTFBO);
    glBindBuffer(GL_ARRAY_BUFFER, scene->SkinningTFBO);
    glBufferData(GL_ARRAY_BUFFER, skinningVertices.size() * sizeof(SkinnedVertex), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // VAO for skinning
    {
        glGenVertexArrays(1, &scene->SkinningVAO);
        glBindVertexArray(scene->SkinningVAO);

        // Note: texcoords not necessary for skinning (yet?)

        // Vertex geometry
        glBindBuffer(GL_ARRAY_BUFFER, scene->BindPoseMeshVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, Position));
        // glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, TexCoord0));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, Normal));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, Tangent));
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, Bitangent));
        glVertexAttribIPointer(5, 4, GL_UNSIGNED_BYTE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, BoneIDs));
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, Weights));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(0); // position
                                      // glEnableVertexAttribArray(1); // texcoord0
        glEnableVertexAttribArray(2); // normal
        glEnableVertexAttribArray(3); // tangent
        glEnableVertexAttribArray(4); // bitangent
        glEnableVertexAttribArray(5); // bone IDs
        glEnableVertexAttribArray(6); // bone weights

        glBindVertexArray(0);
    }

    // VAO for rendering the final skinned meshes
    {
        glGenVertexArrays(1, &scene->SkinnedMeshVAO);
        glBindVertexArray(scene->SkinnedMeshVAO);

        // Texcoords don't change from skinning
        glBindBuffer(GL_ARRAY_BUFFER, scene->BindPoseMeshVBO);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SkinningVertex), (GLvoid*)offsetof(SkinningVertex, TexCoord0));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, scene->SkinningTFBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (GLvoid*)offsetof(SkinnedVertex, Position));
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (GLvoid*)offsetof(SkinnedVertex, Normal));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (GLvoid*)offsetof(SkinnedVertex, Tangent));
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), (GLvoid*)offsetof(SkinnedVertex, Bitangent));
        glBindBuffer(GL_ARRAY_BUFFER, scene->SkinningTFBO);

        glEnableVertexAttribArray(0); // position
        glEnableVertexAttribArray(1); // texcoord0
        glEnableVertexAttribArray(2); // normal
        glEnableVertexAttribArray(3); // tangent
        glEnableVertexAttribArray(4); // bitangent

                                      // indices don't change from skinning
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene->BindPoseMeshEBO);
        glBindVertexArray(0);
    }

    // Transform feedback for the skinning
    {
        glGenTransformFeedbacks(1, &scene->SkinningTFO);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, scene->SkinningTFO);
        // Transform feedback buffers are indexed and must be bound with glBindBufferBase or glBindBufferRange.
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, scene->SkinningTFBO);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }

    // Create storage for skinning transformations
    scene->BoneSkinningTransforms.resize(scene->BoneInverseBindPoseTransforms.size());
    scene->BoneParent.resize(scene->BoneInverseBindPoseTransforms.size(), -1);

    // Compute inverse model transformation used to compute skinning transformation
    glm::mat4 inverseModelTransform = glm::inverseTranspose(glm::make_mat4(&aiscene->mRootNode->mTransformation.a1));

    // depth first traversal of scene to build draws and skinning transformations
    std::function<void(const aiNode*, glm::mat4)> addNode;
    addNode = [&addNode, &aiscene, &scene, &inverseModelTransform](const aiNode* node, glm::mat4 transform)
    {
        transform = transform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
        std::string boneName(node->mName.C_Str());

        // if the node is a bone, compute and store its skinning transformation
        if (scene->BoneIDs.find(boneName) != scene->BoneIDs.end())
        {
            int boneID = scene->BoneIDs[boneName];
            scene->BoneSkinningTransforms[boneID] = inverseModelTransform * transform * scene->BoneInverseBindPoseTransforms[boneID];
        }

        // add draws for all meshes assigned to this node
        for (int nodeMeshIdx = 0; nodeMeshIdx < (int)node->mNumMeshes; nodeMeshIdx++)
        {
            GLDrawElementsIndirectCommand draw = scene->SkinnedMeshDrawCommands[node->mMeshes[nodeMeshIdx]];
            scene->NodeDrawCmds.push_back(draw);
            scene->NodeModelWorldTransforms.push_back(transform);
            scene->NodeMaterialIDs.push_back(aiscene->mMeshes[node->mMeshes[nodeMeshIdx]]->mMaterialIndex);
            scene->NodeTypes.push_back(SCENENODETYPE_SKINNEDMESH);
            // the offset of the first bone that will be written from loading this mesh
            scene->SkinnedMeshBaseBones.push_back((int)scene->BoneInverseBindPoseTransforms.size());
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
    glBindBuffer(GL_TEXTURE_BUFFER, scene->SkinningMatrixPaletteTBO);
    glBufferData(GL_TEXTURE_BUFFER, scene->BoneSkinningTransforms.size() * sizeof(scene->BoneSkinningTransforms[0]), scene->BoneSkinningTransforms.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    int numBones = (int)scene->BoneInverseBindPoseTransforms.size();

    // Storage for bone controls. Initially animated by the animation engine.
    scene->BoneControls.resize(numBones, BONECONTROL_ANIMATION);

    // Storage for dynamics
    scene->BoneDynamicsBackBufferIndex = 0;
    for (int i = 0; i < Scene::NUM_BONE_DYNAMICS_BUFFERS; i++)
    {
        scene->BoneDynamicsPositions[i].resize(numBones);
        scene->BoneDynamicsVelocities[i].resize(numBones);
    }
    */
}
#endif
