#include "sceneloader.h"

#include "scene.h"

// assimp includes
#include <cimport.h>
// assimp also has a scene.h. weird.
#include <scene.h>
#include <postprocess.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <vector>
#include <string>
#include <functional>

static void LoadMD5Materials(
    Scene* scene,
    const char* assetFolder, const char* modelFolder,
    aiMaterial** materials, int numMaterials,
    int* materialIDMapping)
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
        for (int textureToLoadIdx = 0; textureToLoadIdx < (int)texturesToLoad[textureTypeIdx].size(); textureToLoadIdx++)
        {
            const std::string& fullpath = assetFolder + texturesToLoad[textureTypeIdx][textureToLoadIdx];

            int width, height, comp;
            int req_comp = 4;
            stbi_set_flip_vertically_on_load(1); // because GL
            stbi_uc* img = stbi_load(fullpath.c_str(), &width, &height, &comp, req_comp);
            if (!img)
            {
                fprintf(stderr, "stbi_load (%s): %s\n", fullpath.c_str(), stbi_failure_reason());
            }
            else
            {
                bool hasTransparency = false;
                for (int i = 0; i < width * height; i++)
                {
                    if (img[i * 4 + 3] != 255)
                    {
                        hasTransparency = true;
                        break;
                    }
                }

                bool isSRGB = false;
                if (textureTypes[textureTypeIdx] == aiTextureType_DIFFUSE)
                {
                    isSRGB = true;
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
                {
                    isSRGB = false;
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_NORMALS)
                {
                    isSRGB = false;
                }
                else
                {
                    fprintf(stderr, "%s: Unhandled texture type %d\n", fullpath.c_str(), textureTypes[textureTypeIdx]);
                    exit(1);
                }

                // premultiply teh alphas
                if (hasTransparency)
                {
                    for (int i = 0; i < width * height; i++)
                    {
                        float alpha = glm::clamp(img[i * 4 + 3] / 255.0f, 0.0f, 1.0f);
                        if (isSRGB)
                        {
                            alpha = glm::clamp(std::pow(alpha, 1.0f / 2.2f), 0.0f, 1.0f);
                        }
                        img[i * 4 + 0] = stbi_uc(img[i * 4 + 0] * alpha);
                        img[i * 4 + 1] = stbi_uc(img[i * 4 + 1] * alpha);
                        img[i * 4 + 2] = stbi_uc(img[i * 4 + 2] * alpha);
                    }
                }

                if (req_comp != 0)
                {
                    comp = req_comp;
                }

                GLenum srcDataFormat[4] = {
                    GL_RED, GL_RG, GL_RGB, GL_RGBA
                };

                GLuint texture;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);

                if (textureTypes[textureTypeIdx] == aiTextureType_DIFFUSE)
                {
                    float anisotropy;
                    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropy);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, width, height, 0, srcDataFormat[comp - 1], GL_UNSIGNED_BYTE, img);
                }
                else if (textureTypes[textureTypeIdx] == aiTextureType_SPECULAR)
                {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, srcDataFormat[comp - 1], GL_UNSIGNED_BYTE, img);
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
                    d.HasTransparency = hasTransparency;
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

        // Potential improvement: 
        // Look for an existing material with the same properties,
        // instead of creating a new one.
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

        if (materialIDMapping)
        {
            materialIDMapping[materialIdx] = (int)scene->Materials.size();
        }

        scene->Materials.push_back(std::move(newMat));
    }
}

static int LoadMD5SkeletonNode(
    Scene* scene,
    const aiNode* ainode,
    const std::unordered_map<std::string, glm::mat4>& invBindPoseTransforms)
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

    // Generate bone indices for rendering
    std::vector<glm::uvec2> boneIndices(boneCount - 1);
    for (int boneIdx = 1, indexIdx = 0; boneIdx < boneCount; boneIdx++, indexIdx++)
    {
        boneIndices[indexIdx] = glm::uvec2(boneParentIDs[boneIdx], boneIdx);
    }

    Skeleton skeleton;
    skeleton.BoneNames.resize(boneCount);
    skeleton.BoneInverseBindPoseTransforms.resize(boneCount);
    skeleton.BoneLengths.resize(boneCount);
    skeleton.BoneParents = std::move(boneParentIDs);
    skeleton.NumBones = boneCount;
    skeleton.NumBoneIndices = 2 * (int)boneIndices.size();

    for (int boneID = 0; boneID < boneCount; boneID++)
    {
        skeleton.BoneNames[boneID] = boneNodes[boneID]->mName.C_Str();
        skeleton.BoneNameToID.emplace(skeleton.BoneNames[boneID], boneID);

        int parentBoneID = skeleton.BoneParents[boneID];

        // Unused bones won't have an inverse bind pose transform to use
        auto it = invBindPoseTransforms.find(skeleton.BoneNames[boneID]);
        if (it != invBindPoseTransforms.end())
        {
            skeleton.BoneInverseBindPoseTransforms[boneID] = it->second;
        }
        else
        {
            // Missing inverse bind pose implies no local transformation
            printf("Bone %s has no inverse bind pose transform, assigning from ", skeleton.BoneNames[boneID].c_str());
            if (parentBoneID >= 0)
            {
                // Same absolute transform as parent
                printf("%s\n", skeleton.BoneNames[parentBoneID].c_str());
                skeleton.BoneInverseBindPoseTransforms[boneID] = skeleton.BoneInverseBindPoseTransforms[parentBoneID];
            }
            else
            {
                // No absolute transform
                printf("identity\n");
                skeleton.BoneInverseBindPoseTransforms[boneID] = glm::mat4(1.0);
            }
        }

        if (parentBoneID != -1)
        {
            glm::mat4 childInvBindPose = skeleton.BoneInverseBindPoseTransforms[boneID];
            glm::mat4 childBindPose = inverse(childInvBindPose);
            glm::vec3 childPosition = glm::vec3(childBindPose[3]);

            glm::mat4 parentInvBindPose = skeleton.BoneInverseBindPoseTransforms[parentBoneID];
            glm::mat4 parentBindPose = inverse(parentInvBindPose);
            glm::vec3 parentPosition = glm::vec3(parentBindPose[3]);

            skeleton.BoneLengths[boneID] = length(childPosition - parentPosition);
        }
    }

    // Upload bone indices
    glGenBuffers(1, &skeleton.BoneEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skeleton.BoneEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, boneIndices.size() * sizeof(boneIndices[0]), boneIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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

    // Global skeleton transformation
    glm::mat4 skeletonTransform = glm::transpose(glm::make_mat4(&root->mTransformation.a1));

    // Retrieve inverse bind pose transformation for each mesh's bones
    std::unordered_map<std::string, glm::mat4> invBindPoseTransforms;
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
            int skeletonID = LoadMD5SkeletonNode(scene, child, invBindPoseTransforms);
            scene->Skeletons[skeletonID].Transform = skeletonTransform;
            return skeletonID;
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
    aiMesh** meshes, int numMeshes,
    const int* materialIDMapping, // 1-1 mapping with assimp scene materials
    int* bindPoseMeshIDMapping)
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
        bindPoseMesh.MaterialID = materialIDMapping[mesh->mMaterialIndex];

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

        if (bindPoseMeshIDMapping)
        {
            bindPoseMeshIDMapping[meshIdx] = (int)scene->BindPoseMeshes.size();
        }
        scene->BindPoseMeshes.push_back(std::move(bindPoseMesh));
    }
}

void LoadMD5Mesh(
    Scene* scene,
    const char* assetFolder, const char* modelFolder,
    const char* meshFile,
    std::vector<int>* loadedMaterialIDs,
    int* loadedSkeletonID,
    std::vector<int>* loadedBindPoseMeshIDs)
{
    std::string meshpath = std::string(assetFolder) + modelFolder + meshFile;
    const aiScene* aiscene = aiImportFile(meshpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!aiscene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }

    std::vector<int> materialIDMapping(aiscene->mNumMaterials);
    LoadMD5Materials(
        scene,
        assetFolder, modelFolder,
        aiscene->mMaterials, (int)aiscene->mNumMaterials,
        materialIDMapping.data());
    
    int skeletonID = LoadMD5Skeleton(scene, aiscene);
    if (loadedSkeletonID) *loadedSkeletonID = skeletonID;

    if (loadedBindPoseMeshIDs)
    {
        loadedBindPoseMeshIDs->resize(aiscene->mNumMeshes);
    }

    LoadMD5Meshes(
        scene,
        skeletonID,
        modelFolder, meshFile,
        &aiscene->mMeshes[0], (int)aiscene->mNumMeshes,
        materialIDMapping.data(),
        loadedBindPoseMeshIDs ? loadedBindPoseMeshIDs->data() : NULL);

    if (loadedMaterialIDs)
    {
        *loadedMaterialIDs = std::move(materialIDMapping);
    }

    aiReleaseImport(aiscene);
}

void LoadMD5Anim(
    Scene* scene,
    int skeletonID,
    const char* assetFolder, const char* modelFolder,
    const char* animFile,
    int* loadedAnimSequenceID)
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
    if (loadedAnimSequenceID)
    {
        *loadedAnimSequenceID = animSequenceID;
    }

    aiReleaseImport(animScene);
}

static void LoadOBJMeshes(
    Scene* scene,
    const char* modelFolder, const char* meshFile,
    aiMesh** meshes, int numMeshes,
    const int* materialIDMapping, // 1-1 mapping with assimp scene materials
    int* staticMeshIDMapping)
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

        int faceCount = (int)mesh->mNumFaces;
        std::vector<glm::uvec3> indices(faceCount);
        for (int faceIdx = 0; faceIdx < faceCount; faceIdx++)
        {
            indices[faceIdx] = glm::make_vec3(&mesh->mFaces[faceIdx].mIndices[0]);
        }

        StaticMesh staticMesh;
        staticMesh.NumVertices = vertexCount;
        staticMesh.NumIndices = faceCount * 3;
        staticMesh.MaterialID = materialIDMapping[mesh->mMaterialIndex];

        glGenBuffers(1, &staticMesh.PositionVBO);
        glBindBuffer(GL_ARRAY_BUFFER, staticMesh.PositionVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(positions[0]), positions.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &staticMesh.TexCoordVBO);
        glBindBuffer(GL_ARRAY_BUFFER, staticMesh.TexCoordVBO);
        glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(texCoords[0]), texCoords.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &staticMesh.DifferentialVBO);
        glBindBuffer(GL_ARRAY_BUFFER, staticMesh.DifferentialVBO);
        glBufferData(GL_ARRAY_BUFFER, differentials.size() * sizeof(differentials[0]), differentials.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenBuffers(1, &staticMesh.MeshEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, staticMesh.MeshEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        glGenVertexArrays(1, &staticMesh.MeshVAO);
        glBindVertexArray(staticMesh.MeshVAO);

        glBindBuffer(GL_ARRAY_BUFFER, staticMesh.PositionVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PositionVertex), (GLvoid*)offsetof(PositionVertex, Position));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, staticMesh.TexCoordVBO);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexCoordVertex), (GLvoid*)offsetof(TexCoordVertex, TexCoord));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, staticMesh.DifferentialVBO);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Normal));
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Tangent));
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(DifferentialVertex), (GLvoid*)offsetof(DifferentialVertex, Bitangent));
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, staticMesh.MeshEBO);

        glBindVertexArray(0);

        if (staticMeshIDMapping)
        {
            staticMeshIDMapping[meshIdx] = (int)scene->StaticMeshes.size();
        }
        scene->StaticMeshes.push_back(std::move(staticMesh));
    }
}

void LoadOBJMesh(
    Scene* scene,
    const char* assetFolder, const char* modelFolder,
    const char* objFile,
    std::vector<int>* loadedMaterialIDs,
    std::vector<int>* loadedStaticMeshIDs)
{
    std::string meshpath = std::string(assetFolder) + modelFolder + objFile;
    const aiScene* aiscene = aiImportFile(meshpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!aiscene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }

    std::vector<int> materialIDMapping(aiscene->mNumMaterials);
    LoadMD5Materials(
        scene,
        assetFolder, modelFolder,
        aiscene->mMaterials, (int)aiscene->mNumMaterials,
        materialIDMapping.data());

    std::vector<int> staticMeshIDMapping(aiscene->mNumMeshes);
    LoadOBJMeshes(
        scene,
        modelFolder, objFile,
        aiscene->mMeshes, (int)aiscene->mNumMeshes,
        materialIDMapping.data(),
        staticMeshIDMapping.data());

    if (loadedMaterialIDs)
    {
        *loadedMaterialIDs = std::move(materialIDMapping);
    }

    if (loadedStaticMeshIDs)
    {
        *loadedStaticMeshIDs = std::move(staticMeshIDMapping);
    }

    aiReleaseImport(aiscene);
}
