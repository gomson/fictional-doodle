#include "scene.h"

#include "dynamics.h"
#include "runtimecpp.h"
#include "mysdl_dpi.h"

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

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

void LoadScene(Scene* scene)
{
#if 1
    std::string scenepath = "assets/hellknight/";
    std::string modelname = "hellknight.md5mesh";
    std::vector<std::string> animnames{
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
#else
    std::string scenepath = "assets/teapot/";
    std::string modelname = "teapot.obj";
    std::vector<std::string> animnames;
#endif

    std::string modelpath = scenepath + modelname;
    const aiScene* aiscene = aiImportFile(modelpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
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
        std::string fullpath = scenepath + diffuseTexturesToLoad[textureIdx].data;
        stbi_uc* img = stbi_load(fullpath.c_str(), &width, &height, &comp, 4);
        if (!img)
        {
            fprintf(stderr, "stbi_load (%s): %s\n", fullpath.c_str(), stbi_failure_reason());
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
        glGenTransformFeedbacks(1, & scene->SkinningTFO);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, scene->SkinningTFO);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, scene->SkinningTFBO);
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

    // Load animations
    for (int animToLoadIdx = 0; animToLoadIdx < (int)animnames.size(); animToLoadIdx++)
    {
        std::string fullpath = scenepath + animnames[animToLoadIdx];
        const aiScene* animScene = aiImportFile(fullpath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
        if (!animScene)
        {
            fprintf(stderr, "Failed to load animation %s: %s\n", fullpath.c_str(), aiGetErrorString());
            exit(1);
        }

        for (int animIdx = 0; animIdx < (int)animScene->mNumAnimations; animIdx++)
        {
            aiAnimation* animation = animScene->mAnimations[animIdx];
            
            scene->AnimSequenceNames.push_back(animation->mName.length == 0 ? animnames[animToLoadIdx].c_str() : animation->mName.C_Str());

            scene->AnimSequenceBoneBaseFrames.emplace_back(animation->mNumChannels);
            scene->AnimSequenceBoneChannelBits.emplace_back(animation->mNumChannels);
            scene->AnimSequenceBoneFrameDataOffset.emplace_back(animation->mNumChannels);

            std::vector<SQT>& baseFrame = scene->AnimSequenceBoneBaseFrames.back();
            std::vector<uint8_t>& channelBits = scene->AnimSequenceBoneChannelBits.back();
            std::vector<int>& frameDataOffsets = scene->AnimSequenceBoneFrameDataOffset.back();

            int numFrames = 0;
            int numFloatsPerFrame = 0;

            for (int bone = 0; bone < (int)animation->mNumChannels; bone++)
            {
                numFrames = std::max(numFrames, (int)animation->mChannels[bone]->mNumScalingKeys);
                numFrames = std::max(numFrames, (int)animation->mChannels[bone]->mNumPositionKeys);
                numFrames = std::max(numFrames, (int)animation->mChannels[bone]->mNumRotationKeys);

                aiVector3D basePos = animation->mChannels[bone]->mPositionKeys[0].mValue;
                baseFrame[bone].T = glm::vec3(basePos.x, basePos.y, basePos.z);

                aiQuaternion baseQuat = animation->mChannels[bone]->mRotationKeys[0].mValue;
                baseFrame[bone].Q = glm::quat(baseQuat.w, baseQuat.x, baseQuat.y, baseQuat.z);

                // undo assimp's expansion...
                glm::bvec3 allSameT(false);
                if (animation->mChannels[bone]->mNumPositionKeys == 1)
                {
                    allSameT = glm::bvec3(true);
                }
                else
                {
                    glm::bvec3 allSame = glm::bvec3(true);
                    for (int i = 1; i < (int)animation->mChannels[bone]->mNumPositionKeys; i++)
                    {
                        aiVector3D v1 = animation->mChannels[bone]->mPositionKeys[i].mValue;
                        aiVector3D v0 = animation->mChannels[bone]->mPositionKeys[i - 1].mValue;
                        if (v0.x != v1.x) allSame.x = false;
                        if (v0.y != v1.y) allSame.y = false;
                        if (v0.z != v1.z) allSame.z = false;
                        if (!any(allSame))
                        {
                            break;
                        }
                    }
                    allSameT = allSame;
                }

                glm::bvec3 allSameQ(false);
                if (animation->mChannels[bone]->mNumRotationKeys == 1)
                {
                    allSameQ = glm::bvec3(true);
                }
                else
                {
                    glm::bvec3 allSame = glm::bvec3(true);
                    for (int i = 1; i < (int)animation->mChannels[bone]->mNumRotationKeys; i++)
                    {
                        aiQuaternion v1 = animation->mChannels[bone]->mRotationKeys[i].mValue;
                        aiQuaternion v0 = animation->mChannels[bone]->mRotationKeys[i - 1].mValue;
                        if (v0.x != v1.x) allSame.x = false;
                        if (v0.y != v1.y) allSame.y = false;
                        if (v0.z != v1.z) allSame.z = false;
                        // don't need to check for w, since it's derived from xyz
                        if (!any(allSame))
                        {
                            break;
                        }
                    }
                    allSameQ = allSame;
                }

                channelBits[bone] |= allSameT.x ? 0 : Scene::ANIM_CHANNEL_TX_BIT;
                channelBits[bone] |= allSameT.y ? 0 : Scene::ANIM_CHANNEL_TY_BIT;
                channelBits[bone] |= allSameT.z ? 0 : Scene::ANIM_CHANNEL_TZ_BIT;
                channelBits[bone] |= allSameQ.x ? 0 : Scene::ANIM_CHANNEL_QX_BIT;
                channelBits[bone] |= allSameQ.y ? 0 : Scene::ANIM_CHANNEL_QY_BIT;
                channelBits[bone] |= allSameQ.z ? 0 : Scene::ANIM_CHANNEL_QZ_BIT;

                if (!channelBits[bone])
                {
                    frameDataOffsets[bone] = 0;
                }
                else
                {
                    frameDataOffsets[bone] = numFloatsPerFrame;
                }

                for (uint8_t bits = channelBits[bone]; bits != 0; bits = bits & (bits - 1))
                {
                    numFloatsPerFrame++;
                }
            }
            
            // build frame data
            scene->AnimSequenceFrameData.emplace_back(std::vector<std::vector<float>>(numFrames, std::vector<float>(numFloatsPerFrame)));
            std::vector<std::vector<float>>& frameDatas = scene->AnimSequenceFrameData.back();
            for (int bone = 0; bone < (int)animation->mNumChannels; bone++)
            {
                for (int frameIdx = 0; frameIdx < numFrames; frameIdx++)
                {
                    int off = frameDataOffsets[bone];
                    uint8_t bits = channelBits[bone];

                    if (bits & Scene::ANIM_CHANNEL_TX_BIT)
                    {
                        frameDatas[frameIdx][off] = animation->mChannels[bone]->mPositionKeys[frameIdx].mValue.x;
                        off++;
                    }
                    if (bits & Scene::ANIM_CHANNEL_TY_BIT)
                    {
                        frameDatas[frameIdx][off] = animation->mChannels[bone]->mPositionKeys[frameIdx].mValue.y;
                        off++;
                    }
                    if (bits & Scene::ANIM_CHANNEL_TZ_BIT)
                    {
                        frameDatas[frameIdx][off] = animation->mChannels[bone]->mPositionKeys[frameIdx].mValue.z;
                        off++;
                    }
                    if (bits & Scene::ANIM_CHANNEL_QX_BIT)
                    {
                        frameDatas[frameIdx][off] = animation->mChannels[bone]->mRotationKeys[frameIdx].mValue.x;
                        off++;
                    }
                    if (bits & Scene::ANIM_CHANNEL_QY_BIT)
                    {
                        frameDatas[frameIdx][off] = animation->mChannels[bone]->mRotationKeys[frameIdx].mValue.y;
                        off++;
                    }
                    if (bits & Scene::ANIM_CHANNEL_QZ_BIT)
                    {
                        frameDatas[frameIdx][off] = animation->mChannels[bone]->mRotationKeys[frameIdx].mValue.z;
                        off++;
                    }
                }
            }
        }

        aiReleaseImport(animScene);
    }
}

void InitScene(Scene* scene)
{
    LoadScene(scene);

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
        if (getU(&scene->SkinningSP_ModelWorldLoc, "ModelWorld") ||
            getU(&scene->SkinningSP_BoneTransformsLoc, "BoneTransforms") ||
            getU(&scene->SkinningSP_BoneOffsetLoc, "BoneOffset"))
        {
            return;
        }
    }

    if (reload(&scene->SceneSP))
    {
        if (getU(&scene->SceneSP_WorldViewLoc, "WorldView") ||
            getU(&scene->SceneSP_WorldViewProjectionLoc, "WorldViewProjection") ||
            getU(&scene->SceneSP_Diffuse0Loc, "Diffuse0"))
        {
            return;
        }
    }

    if (!programZero)
    {
        scene->AllShadersOK = true;
    }
}

void UpdateSceneDynamics(Scene* scene, uint32_t dt_ms)
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

void ShowSystemInfoGUI(Scene* scene)
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

void ShowToolboxGUI(Scene* scene, SDL_Window* window)
{
    ImGuiIO& io = ImGui::GetIO();
    int w = int(io.DisplaySize.x / io.DisplayFramebufferScale.x);
    int h = int(io.DisplaySize.y / io.DisplayFramebufferScale.y);

    int toolboxW = 300, toolboxH = 400;

    ImGui::SetNextWindowSize(ImVec2((float)toolboxW, (float)toolboxH), ImGuiSetCond_Always);
    ImGui::SetNextWindowPos(ImVec2((float)w - toolboxW, 0), ImGuiSetCond_Always);
    if (ImGui::Begin("Toolbox", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        std::vector<const char*> animSequenceNames(scene->AnimSequenceNames.size());
        for (int i = 0; i < (int)animSequenceNames.size(); i++)
        {
            animSequenceNames[i] = scene->AnimSequenceNames[i].c_str();
        }
        if (!animSequenceNames.empty())
        {
            ImGui::Text("Animation Sequence");
            ImGui::ListBox("##animsequences", &scene->CurrAnimSequence, &animSequenceNames[0], (int)animSequenceNames.size());
        }
    }
    ImGui::End();
}

void UpdateSceneSkinnedGeometry(Scene* scene, uint32_t dt_ms)
{
    glUseProgram(scene->SkinningSP.Handle);
    glBindVertexArray(scene->SkinningVAO);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, scene->SkinningTFO);
    glBeginTransformFeedback(GL_POINTS); // capture points so triangles aren't unfolded

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, scene->SkinningMatrixPaletteTexture);
    glUniform1i(scene->SkinningSP_BoneTransformsLoc, 0);

    // skin every mesh in the scene one by one
    for (int skinnedMeshIdx = 0; skinnedMeshIdx < (int)scene->SkinnedMeshDrawCommands.size(); skinnedMeshIdx++)
    {
        int nodeID = scene->SkinnedMeshNodeIDs[skinnedMeshIdx];
        
        glm::mat4 modelworld = scene->NodeModelWorldTransforms[nodeID];
        glUniformMatrix4fv(scene->SkinningSP_ModelWorldLoc, 1, GL_FALSE, glm::value_ptr(modelworld));

        int baseBone = scene->SkinnedMeshBaseBones[skinnedMeshIdx];
        glUniform1i(scene->SkinningSP_BoneOffsetLoc, baseBone);

        GLDrawElementsIndirectCommand draw = scene->SkinnedMeshDrawCommands[skinnedMeshIdx];
        int bindPoseID = scene->SkinnedMeshBindPoseIDs[skinnedMeshIdx];
        int numVertices = scene->BindBoseMeshNumVertices[bindPoseID];
        glDrawArrays(GL_POINTS, draw.baseVertex, numVertices);
    }

    glEndTransformFeedback();
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    glBindVertexArray(0);
    glUseProgram(0);
}

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

    UpdateSceneSkinnedGeometry(scene, dt_ms);

    UpdateSceneDynamics(scene, dt_ms);
}
