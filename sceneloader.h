#pragma once

struct Scene;

// Adds the contents of a md5mesh file to the scene.
// Appends new skeletons to Skeleton Table
// Appends new bind pose meshes to BindPoseMesh Table
// Appends new materials to Material Table
// Appends new diffuse textures to DiffuseTexture table
void LoadMD5Mesh(
    Scene* scene, 
    const char* folder, const char* meshfile);

// Adds the contents of a md5anim file to the scene.
// Assumes skeletons are already in the Skeleton Table
// Appends new animation sequences to AnimSequence Table 
void LoadMD5Anim(
    Scene* scene, 
    int skeletonID,
    const char* folder, const char* animfile);