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

static void LoadMD5Materials(
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
                    scene->DiffuseTextures.push_back(std::move(d));
                    id = (int)scene->DiffuseTextures.size() - 1;
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
                {
                    SpecularTexture s;
                    s.TO = texture;
                    scene->SpecularTextures.push_back(std::move(s));
                    id = (int)scene->SpecularTextures.size() - 1;
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_NORMALS)
                {
                    NormalTexture n;
                    n.TO = texture;
                    scene->NormalTextures.push_back(std::move(n));
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

        scene->Materials.push_back(std::move(newMat));
    }
}

static int LoadMD5SkeletonNode(
    Scene* scene,
    aiNode* ainode,
    std::unordered_map<std::string, glm::mat4> invBindPoseTransforms)
{
    if (strcmp(ainode->mName.C_Str(), "<MD5_Hierarchy>") != 0)
    {
        fprintf(stderr, "Expected <MD5_Hierarchy>, got %s\n", ainode->mName.C_Str());
        exit(1);
    }

    // traverse skeleton and flatten it
    std::vector<aiNode*> boneNodes;
    std::vector<int> boneParentIDs;
    std::vector<std::pair<aiNode*, int>> skeletonDFSStack;

    // Initialize stack with parentless bones
    for (int childIdx = (int)ainode->mNumChildren - 1; childIdx >= 0; childIdx--)
    {
        skeletonDFSStack.emplace_back(ainode->mChildren[childIdx], -1);
    }

    while (!empty(skeletonDFSStack))
    {
        aiNode* node = skeletonDFSStack.back().first;
        int parentID = skeletonDFSStack.back().second;
        skeletonDFSStack.pop_back();

        int myBoneID = (int)boneNodes.size();
        boneNodes.push_back(node);
        boneParentIDs.push_back(parentID);

        for (int childIdx = (int)node->mNumChildren - 1; childIdx >= 0; childIdx--)
        {
            skeletonDFSStack.emplace_back(node->mChildren[childIdx], myBoneID);
        }
    }

    int boneCount = (int)boneNodes.size();
    
    Skeleton skeleton;
    skeleton.BoneNames.resize(boneCount);
    skeleton.BoneInverseBindPoseTransforms.resize(boneCount);
    skeleton.BoneParents = std::move(boneParentIDs);
    skeleton.NumBones = boneCount;

    for (int boneID = 0; boneID < boneCount; boneID++)
    {
        skeleton.BoneNames[boneID] = boneNodes[boneID]->mName.C_Str();
        skeleton.BoneNameToID.emplace(skeleton.BoneNames[boneID], boneID);
        skeleton.BoneInverseBindPoseTransforms[boneID] = invBindPoseTransforms[skeleton.BoneNames[boneID]];
    }

    scene->Skeletons.push_back(std::move(skeleton));

    return (int)scene->Skeletons.size() - 1;
}

static int LoadMD5Skeleton(
    Scene* scene,
    const aiScene* aiscene)
{
    aiNode* root = aiscene->mRootNode;

    if (strcmp(root->mName.C_Str(), "<MD5_Root>") != 0)
    {
        fprintf(stderr, "Expected <MD5_Root>, got %s\n", root->mName.C_Str());
        exit(1);
    }

    std::unordered_map<std::string, glm::mat4> invBindPoseTransforms;

    // Retrieve inverse bind pose transforms from mesh data
    for (int meshIdx = 0; meshIdx < (int)aiscene->mNumMeshes; meshIdx++)
    {
        for (int boneIdx = 0; boneIdx < (int)aiscene->mMeshes[meshIdx]->mNumBones; boneIdx++)
        {
            const aiBone* bone = aiscene->mMeshes[meshIdx]->mBones[boneIdx];
            std::string boneName = bone->mName.C_Str();
            invBindPoseTransforms[boneName] = glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1));
        }
    }

    // traverse all children
    for (int childIdx = 0; childIdx < (int)root->mNumChildren; childIdx++)
    {
        aiNode* child = root->mChildren[childIdx];

        if (strcmp(child->mName.C_Str(), "<MD5_Hierarchy>") == 0)
        {
            // Found skeleton
            return LoadMD5SkeletonNode(scene, child, invBindPoseTransforms);
        }
    }

    fprintf(stderr, "Failed to find skeleton in scene\n");
    exit(1);
    return -1;
}

static void LoadMD5Meshes(
    Scene* scene,
    int skeletonID,
    const char* modelFolder, const char* meshFile,
    aiMesh** meshes, int numMeshes)
{
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
            fprintf(stderr, "Mesh %s didn't have TexCoord\n", mesh->mName.C_Str());
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

        if (!mesh->mBones)
        {
            fprintf(stderr, "Mesh %s didn't have bones\n", mesh->mName.C_Str());
            exit(1);
        }

        int vertexCount = (int)mesh->mNumVertices;
        std::vector<PositionVertex> positions(vertexCount);
        std::vector<TexCoordVertex> texCoords(vertexCount);
        std::vector<DifferentialVertex> differentials(vertexCount);
        for (int vertexIdx = 0; vertexIdx < vertexCount; vertexIdx++)
        {
            positions[vertexIdx].Position = glm::make_vec3(&mesh->mVertices[vertexIdx][0]);
            texCoords[vertexIdx].TexCoord = glm::make_vec2(&mesh->mTextureCoords[0][vertexIdx][0]);
            differentials[vertexIdx].Normal = glm::make_vec3(&mesh->mNormals[vertexIdx][0]);
            differentials[vertexIdx].Tangent = glm::make_vec3(&mesh->mTangents[vertexIdx][0]);
            differentials[vertexIdx].Bitangent = glm::make_vec3(&mesh->mBitangents[vertexIdx][0]);
        }

        const Skeleton& skeleton = scene->Skeletons[skeletonID];

        int boneCount = (int)mesh->mNumBones;
        std::vector<BoneWeightVertex> boneWeights(vertexCount);
        std::vector<int> vertexNumBones(vertexCount);
        for (int boneIdx = 0; boneIdx < boneCount; boneIdx++)
        {
            aiBone* bone = mesh->mBones[boneIdx];
            int boneWeightCount = (int)bone->mNumWeights;
            
            auto foundBone = skeleton.BoneNameToID.find(bone->mName.C_Str());
            if (foundBone == end(skeleton.BoneNameToID))
            {
                fprintf(stderr, "Couldn't find bone %s in skeleton\n", bone->mName.C_Str());
                exit(1);
            }

            int boneID = foundBone->second;

            for (int weightIdx = 0; weightIdx < boneWeightCount; weightIdx++)
            {
                aiVertexWeight vertexWeight = bone->mWeights[weightIdx];
                int vertexID = vertexWeight.mVertexId;
                float weight = vertexWeight.mWeight;

                if (vertexNumBones[vertexID] < 4)
                {
                    boneWeights[vertexID].BoneIDs[vertexNumBones[vertexID]] = boneID;
                    boneWeights[vertexID].Weights[vertexNumBones[vertexID]] = weight;
                    vertexNumBones[vertexID]++;
                }
                else if (boneWeights[vertexID].Weights[3] < weight)
                {
                    // Keep the top 4 influencing weights in sorted order. bubble down.
                    boneWeights[vertexID].Weights[3] = weight;
                    for (int nextWeight = 2; nextWeight >= 0; nextWeight--)
                    {
                        if (boneWeights[vertexID].Weights[nextWeight] >= boneWeights[vertexID].Weights[nextWeight + 1])
                        {
                            break;
                        }
                        
                        std::swap(boneWeights[vertexID].Weights[nextWeight], boneWeights[vertexID].Weights[nextWeight + 1]);
                    }
                }
            }
        }

        int faceCount = (int)mesh->mNumFaces;
        std::vector<glm::uvec3> indices(faceCount);
        for (int faceIdx = 0; faceIdx < faceCount; faceIdx++)
        {
            indices[faceIdx] = glm::make_vec3(&mesh->mFaces[faceIdx].mIndices[0]);
        }

        BindPoseMesh bindPoseMesh;
        bindPoseMesh.NumVertices = vertexCount;
        bindPoseMesh.NumIndices = faceCount * 3;
        bindPoseMesh.SkeletonID = skeletonID;

        glGenBuffers(1, &bindPoseMesh.PositionVBO);
        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.PositionVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(positions[0]), positions.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &bindPoseMesh.TexCoordVBO);
        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.TexCoordVBO);
        glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(texCoords[0]), texCoords.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &bindPoseMesh.DifferentialVBO);
        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.DifferentialVBO);
        glBufferData(GL_ARRAY_BUFFER, differentials.size() * sizeof(differentials[0]), differentials.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &bindPoseMesh.BoneVBO);
        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.BoneVBO);
        glBufferData(GL_ARRAY_BUFFER, boneWeights.size() * sizeof(boneWeights[0]), boneWeights.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &bindPoseMesh.EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bindPoseMesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glGenVertexArrays(1, &bindPoseMesh.SkinningVAO);
        glBindVertexArray(bindPoseMesh.SkinningVAO);

        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.PositionVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PositionVertex), (GLvoid*)offsetof(PositionVertex, Position));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.TexCoordVBO);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexCoordVertex), (GLvoid*)offsetof(TexCoordVertex, TexCoord));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.DifferentialVBO);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Normal));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Tangent));
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Bitangent));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);

        glBindBuffer(GL_ARRAY_BUFFER, bindPoseMesh.BoneVBO);
        glVertexAttribIPointer(5, 4, GL_UNSIGNED_BYTE, sizeof(BoneWeightVertex), (GLvoid*)offsetof(BoneWeightVertex, BoneIDs));
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(BoneWeightVertex), (GLvoid*)offsetof(BoneWeightVertex, Weights));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(5);
        glEnableVertexAttribArray(6);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bindPoseMesh.EBO);

        glBindVertexArray(0);

        scene->BindPoseMeshes.push_back(std::move(bindPoseMesh));

        std::string bindPoseMeshName = std::string(modelFolder) + meshFile + "[" + std::to_string(meshIdx) + "]";
        int bindPoseMeshID = (int)scene->BindPoseMeshes.size() - 1;
        scene->BindPoseMeshNameToID.emplace(bindPoseMeshName,bindPoseMeshID);
    }
}

void LoadMD5Mesh(
    Scene* scene,
    const char* assetFolder, const char* modelFolder,
    const char* meshFile,
    int* numBindPoseMeshesAdded)
{
    std::string meshpath = std::string(assetFolder) + modelFolder + meshFile;
    const aiScene* aiscene = aiImportFile(meshpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!aiscene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }

    LoadMD5Materials(scene, assetFolder, modelFolder, aiscene->mMaterials, (int)aiscene->mNumMaterials);
    
    int skeletonID = LoadMD5Skeleton(scene, aiscene);
    scene->SkeletonNameToID.emplace(std::string(modelFolder) + meshFile, skeletonID);

    LoadMD5Meshes(
        scene,
        skeletonID,
        modelFolder, meshFile,
        &aiscene->mMeshes[0], (int)aiscene->mNumMeshes);

    if (numBindPoseMeshesAdded) *numBindPoseMeshesAdded = (int)aiscene->mNumMeshes;

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
    if (animScene->mNumAnimations != 1)
    {
        fprintf(stderr, "Expected 1 animation in %s, got %d\n", fullpath.c_str(), (int)animScene->mNumAnimations);
        return;
    }

    // TODO: Check if animation is valid for the skeleton

    // One animation per file
    const aiAnimation* animation = animScene->mAnimations[0];

    AnimSequence animSequence;
    animSequence.Name = std::string(modelFolder) + animFile;
    animSequence.SkeletonID = skeletonID;
    animSequence.FramesPerSecond = (int)animation->mTicksPerSecond;
    animSequence.NumFrames = (int)animation->mDuration;

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

    scene->AnimSequences.push_back(std::move(animSequence));

    int animSequenceID = (int)scene->AnimSequences.size() - 1;

    scene->AnimSequenceNameToID.emplace(scene->AnimSequences.back().Name, animSequenceID);

    aiReleaseImport(animScene);
}

// TODO: Remove and replace
#if 0
void LoadScene(Scene* scene)
{
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
}
#endif
