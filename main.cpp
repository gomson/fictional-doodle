// For Windows-specific code
#ifdef _WIN32
#define UNICODE 1
#define NOMINMAX 1
#include <Windows.h>
#include <ShellScalingAPI.h>
#include <comdef.h>
#endif

#include <SDL.h>
#include <GL/glcorearb.h>

#include <cimport.h>
#include <scene.h>
#include <postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define QFPC_IMPLEMENTATION
#include <qfpc.h>

#include "imgui/imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include "opengl.h"

#include <cstdio>
#include <cassert>
#include <cstring>
#include <type_traits>
#include <vector>
#include <sstream>
#include <string>
#include <fstream>
#include <functional>
#include <algorithm>

namespace Renderer
{
    // Framebuffer stuff
    GLuint BackbufferFBO;
    GLuint BackbufferColorTexture;
    GLuint BackbufferDepthTexture;

    bool GUIFocusEnabled;
}

namespace Scene
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec2 TexCoord0;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
    };

    // Geometry stuff
    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    
    // All unique diffuse textures in the scene
    std::vector<GLuint> DiffuseTextures;
    
    // For each material, gives the index in the DiffuseTextures array for diffuse texture 0. -1 if there is no diffuse texture 0 for this material.
    std::vector<int> MaterialDiffuse0TextureIndex;

    // For each node in the scene, the draw arguments to draw it.
    std::vector<GLDrawElementsIndirectCommand> NodeDrawCmds;
    // For each node in the scene, the modelworld matrix.
    std::vector<glm::mat4> NodeModelWorldTransforms;
    // For each node in the scene, the material ID for it.
    std::vector<int> NodeMaterialIDs;

    // Shader stuff
    GLuint SP;
    GLuint VS;
    GLuint FS;
    GLint ViewLoc;
    GLint MVLoc;
    GLint MVPLoc;
    GLint Diffuse0Loc;

    // Camera stuff
    glm::vec3 CameraPosition = glm::vec3(100,100,100);
    glm::vec4 CameraQuaternion = glm::vec4(-0.351835f, 0.231701f, 0.090335f, 0.902411f);
}

void InitRenderer()
{
}

void ResizeRenderer(int windowWidth, int windowHeight, int drawableWidth, int drawableHeight, int numSamples)
{
    // Init rendertargets/depthstencils
    glDeleteTextures(1, &Renderer::BackbufferColorTexture);
    glGenTextures(1, &Renderer::BackbufferColorTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, Renderer::BackbufferColorTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_SRGB8_ALPHA8, drawableWidth, drawableHeight, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    glDeleteTextures(1, &Renderer::BackbufferDepthTexture);
    glGenTextures(1, &Renderer::BackbufferDepthTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, Renderer::BackbufferDepthTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, numSamples, GL_DEPTH_COMPONENT32F, drawableWidth, drawableHeight, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    // Init framebuffer
    glDeleteFramebuffers(1, &Renderer::BackbufferFBO);
    glGenFramebuffers(1, &Renderer::BackbufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, Renderer::BackbufferFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, Renderer::BackbufferColorTexture, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, Renderer::BackbufferDepthTexture, 0);
    GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(sizeof(drawBufs) / sizeof(*drawBufs), &drawBufs[0]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Framebuffer status error: %s\n", FramebufferStatusToStringGL(fboStatus));
        exit(1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void InitScene()
{
    // load scene
#if 1
    std::string mtlpath = "assets/hellknight/";
    std::string scenepath = "assets/hellknight/hellknight.md5mesh";
#else
    std::string mtlpath = "assets/teapot/";
    std::string scenepath = "assets/teapot/teapot.obj";
#endif

    const aiScene* scene = aiImportFile(scenepath.c_str(), aiProcessPreset_TargetRealtime_MaxQuality);
    if (!scene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
    }
    
    // find all textures that need to be loaded
    std::vector<aiString> diffuseTexturesToLoad;
    for (int materialIdx = 0; materialIdx < (int)scene->mNumMaterials; materialIdx++)
    {
        aiMaterial* material = scene->mMaterials[materialIdx];
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
    Scene::DiffuseTextures.resize(diffuseTexturesToLoad.size());
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

        Scene::DiffuseTextures[textureIdx] = texture;

        stbi_image_free(img);
    }

    // hook up list of materials
    for (int materialIdx = 0; materialIdx < (int)scene->mNumMaterials; materialIdx++)
    {
        aiMaterial* material = scene->mMaterials[materialIdx];
        
        unsigned int texturecount = aiGetMaterialTextureCount(material, aiTextureType_DIFFUSE);
        if (texturecount == 0)
        {
            Scene::MaterialDiffuse0TextureIndex.push_back(-1);
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
        Scene::MaterialDiffuse0TextureIndex.push_back((int)std::distance(begin(diffuseTexturesToLoad), textureIt));
    }

    // figure out how much memory is needed for the scene and check assumptions about data
    int totalNumVertices = 0;
    int totalNumIndices = 0;
    std::vector<GLDrawElementsIndirectCommand> meshDraws(scene->mNumMeshes);
    for (int sceneMeshIdx = 0; sceneMeshIdx < (int)scene->mNumMeshes; sceneMeshIdx++)
    {
        aiMesh* mesh = scene->mMeshes[sceneMeshIdx];

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
    std::vector<Scene::Vertex> meshVertices(totalNumVertices);
    std::vector<glm::uvec3> meshIndices(totalNumIndices);

    // read mesh geometry data
    for (int sceneMeshIdx = 0, currVertexIdx = 0, currIndexIdx = 0; sceneMeshIdx < (int)scene->mNumMeshes; sceneMeshIdx++)
    {
        aiMesh* mesh = scene->mMeshes[sceneMeshIdx];

        for (int vertexIdx = 0; vertexIdx < (int)mesh->mNumVertices; vertexIdx++)
        {
            Scene::Vertex vertex;
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
    }

    // Upload geometry data
    glGenBuffers(1, &Scene::VBO);
    glBindBuffer(GL_ARRAY_BUFFER, Scene::VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(meshVertices[0]), meshVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glGenBuffers(1, &Scene::EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Scene::EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshIndices.size() * sizeof(meshIndices[0]), meshIndices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Setup VAO
    glGenVertexArrays(1, &Scene::VAO);
    glBindVertexArray(Scene::VAO);
    glBindBuffer(GL_ARRAY_BUFFER, Scene::VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Scene::Vertex), (GLvoid*)offsetof(Scene::Vertex, Position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Scene::Vertex), (GLvoid*)offsetof(Scene::Vertex, TexCoord0));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Scene::Vertex), (GLvoid*)offsetof(Scene::Vertex, Normal));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Scene::Vertex), (GLvoid*)offsetof(Scene::Vertex, Tangent));
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Scene::Vertex), (GLvoid*)offsetof(Scene::Vertex, Bitangent));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // texcoord0
    glEnableVertexAttribArray(2); // normal
    glEnableVertexAttribArray(3); // tangent
    glEnableVertexAttribArray(4); // bitangent
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Scene::EBO);
    glBindVertexArray(0);

    // depth first traversal of scene to build draws
    std::function<void(const aiNode*, glm::mat4)> addNode;
    addNode = [&addNode, &meshDraws, &scene](const aiNode* node, glm::mat4 transform)
    {
        transform = transform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));

        // add draws for all meshes assigned to this node
        for (int nodeMeshIdx = 0; nodeMeshIdx < (int)node->mNumMeshes; nodeMeshIdx++)
        {
            GLDrawElementsIndirectCommand draw = meshDraws[node->mMeshes[nodeMeshIdx]];
            Scene::NodeDrawCmds.push_back(draw);
            Scene::NodeModelWorldTransforms.push_back(transform);
            Scene::NodeMaterialIDs.push_back(scene->mMeshes[node->mMeshes[nodeMeshIdx]]->mMaterialIndex);
        }

        // traverse all children
        for (int childIdx = 0; childIdx < (int)node->mNumChildren; childIdx++)
        {
            addNode(node->mChildren[childIdx], transform);
        }
    };

    addNode(scene->mRootNode, glm::mat4());

    aiReleaseImport(scene);

    // Build shaders
    const char* vsrc, *fsrc;
    GLint compileStatus, linkStatus;

    std::ifstream vsrcifs("scene.vert");
    if (!vsrcifs)
    {
        fprintf(stderr, "Couldn't open scene.vert\n");
        exit(1);
    }
    std::stringstream vsrcss;
    vsrcss << vsrcifs.rdbuf();
    std::string vsrcs = vsrcss.str();
    vsrc = vsrcs.c_str();

    std::ifstream fsrcifs("scene.frag");
    if (!fsrcifs)
    {
        fprintf(stderr, "Couldn't open scene.frag\n");
        exit(1);
    }
    std::stringstream fsrcss;
    fsrcss << fsrcifs.rdbuf();
    std::string fsrcs = fsrcss.str();
    fsrc = fsrcs.c_str();

    Scene::VS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(Scene::VS, 1, &vsrc, NULL);
    glCompileShader(Scene::VS);
    glGetShaderiv(Scene::VS, GL_COMPILE_STATUS, &compileStatus);
    if (!compileStatus)
    {
        GLint logLength;
        glGetShaderiv(Scene::VS, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(logLength + 1);
        glGetShaderInfoLog(Scene::VS, (GLsizei)log.size(), NULL, log.data());
        fprintf(stderr, "Error compiling vertex shader: %s\n", log.data());
        exit(1);
    }

    Scene::FS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(Scene::FS, 1, &fsrc, NULL);
    glCompileShader(Scene::FS);
    glGetShaderiv(Scene::FS, GL_COMPILE_STATUS, &compileStatus);
    if (!compileStatus)
    {
        GLint logLength;
        glGetShaderiv(Scene::FS, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(logLength + 1);
        glGetShaderInfoLog(Scene::FS, (GLsizei)log.size(), NULL, log.data());
        fprintf(stderr, "Error compiling fragment shader: %s\n", log.data());
        exit(1);
    }

    Scene::SP = glCreateProgram();
    glAttachShader(Scene::SP, Scene::VS);
    glAttachShader(Scene::SP, Scene::FS);
    glLinkProgram(Scene::SP);
    glGetProgramiv(Scene::SP, GL_LINK_STATUS, &linkStatus);
    if (!linkStatus)
    {
        GLint logLength;
        glGetProgramiv(Scene::SP, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<GLchar> log(logLength + 1);
        glGetProgramInfoLog(Scene::SP, (GLsizei)log.size(), NULL, log.data());
        fprintf(stderr, "Error linking program: %s\n", log.data());
        exit(1);
    }

    Scene::ViewLoc = glGetUniformLocation(Scene::SP, "View");
    if (Scene::ViewLoc == -1)
    {
        fprintf(stderr, "Couldn't find View uniform\n");
        exit(1);
    }

    Scene::MVLoc = glGetUniformLocation(Scene::SP, "ModelView");
    if (Scene::MVLoc == -1)
    {
        fprintf(stderr, "Couldn't find ModelView uniform\n");
        exit(1);
    }

    Scene::MVPLoc = glGetUniformLocation(Scene::SP, "ModelViewProjection");
    if (Scene::MVPLoc == -1)
    {
        fprintf(stderr, "Couldn't find ModelViewProjection uniform\n");
        exit(1);
    }

    Scene::Diffuse0Loc = glGetUniformLocation(Scene::SP, "Diffuse0");
    if (Scene::Diffuse0Loc == -1)
    {
        fprintf(stderr, "Couldn't find Diffuse0 uniform\n");
        exit(1);
    }
}

void PaintRenderer(SDL_Window* window, uint32_t dt_ticks)
{
    float dt = dt_ticks * 60 / 1000.0f;

    // Update camera
    glm::mat3 cameraRotation;
    {
        int relativeMouseX, relativeMouseY;
        SDL_GetRelativeMouseState(&relativeMouseX, &relativeMouseY);
        
        const uint8_t* keyboardState = SDL_GetKeyboardState(NULL);

        quatFirstPersonCamera(
            glm::value_ptr(Scene::CameraPosition),
            glm::value_ptr(Scene::CameraQuaternion),
            glm::value_ptr(cameraRotation),
            0.10f,
            1.0f * dt,
            Renderer::GUIFocusEnabled ? 0 : relativeMouseX,
            Renderer::GUIFocusEnabled ? 0 : relativeMouseY,
            Renderer::GUIFocusEnabled ? 0 : keyboardState[SDL_SCANCODE_W],
            Renderer::GUIFocusEnabled ? 0 : keyboardState[SDL_SCANCODE_A],
            Renderer::GUIFocusEnabled ? 0 : keyboardState[SDL_SCANCODE_S],
            Renderer::GUIFocusEnabled ? 0 : keyboardState[SDL_SCANCODE_D],
            Renderer::GUIFocusEnabled ? 0 : keyboardState[SDL_SCANCODE_E],
            Renderer::GUIFocusEnabled ? 0 : keyboardState[SDL_SCANCODE_Q]);
    }

    int drawableWidth, drawableHeight;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    glm::mat4 projection = glm::perspective(70.0f, (float)drawableWidth / drawableHeight, 0.01f, 1000.0f);
    glm::mat4 worldView = glm::translate(glm::mat4(cameraRotation), -Scene::CameraPosition);
    glm::mat4 worldViewProjection = projection * worldView;

    // Scene rendering
    {
        glBindFramebuffer(GL_FRAMEBUFFER, Renderer::BackbufferFBO);

        // Clear color is already SRGB encoded, so don't enable GL_FRAMEBUFFER_SRGB before it.
        glClearColor(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_DEPTH_TEST);

        glBindVertexArray(Scene::VAO);
        glUseProgram(Scene::SP);

        glUniform1i(Scene::Diffuse0Loc, 0);

        for (int drawIdx = 0; drawIdx < (int)Scene::NodeDrawCmds.size(); drawIdx++)
        {
            GLDrawElementsIndirectCommand cmd = Scene::NodeDrawCmds[drawIdx];
            assert(cmd.baseInstance == 0); // no base instance because OS X
            assert(cmd.primCount == 1); // assuming no instancing cuz lack of baseInstance makes it boring

            glm::mat4 mv = worldView * Scene::NodeModelWorldTransforms[drawIdx];
            glm::mat4 mvp = worldViewProjection * Scene::NodeModelWorldTransforms[drawIdx];
            glUniformMatrix4fv(Scene::ViewLoc, 1, GL_FALSE, glm::value_ptr(worldView));
            glUniformMatrix4fv(Scene::MVLoc, 1, GL_FALSE, glm::value_ptr(mv));
            glUniformMatrix4fv(Scene::MVPLoc, 1, GL_FALSE, glm::value_ptr(mvp));

            int materialID = Scene::NodeMaterialIDs[drawIdx];
            int diffuseTexture0Index = Scene::MaterialDiffuse0TextureIndex[materialID];
            glActiveTexture(GL_TEXTURE0);
            if (diffuseTexture0Index == -1)
            {
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, Scene::DiffuseTextures[diffuseTexture0Index]);
            }

            glDrawElementsInstancedBaseVertex(GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT, (GLvoid*)(cmd.firstIndex * sizeof(GLuint)), cmd.primCount, cmd.baseVertex);
        }

        glBindVertexArray(0);
        glUseProgram(0);

        glDisable(GL_DEPTH_TEST);

        glDisable(GL_FRAMEBUFFER_SRGB);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // GUI rendering
    {
        glBindFramebuffer(GL_FRAMEBUFFER, Renderer::BackbufferFBO);
        glEnable(GL_FRAMEBUFFER_SRGB);
        ImGui::Render();
        glDisable(GL_FRAMEBUFFER_SRGB);
        glBindFramebuffer(GL_FRAMEBUFFER, Renderer::BackbufferFBO);
    }

    // Draw to window's framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, Renderer::BackbufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO
    glBlitFramebuffer(
        0, 0, drawableWidth, drawableHeight,
        0, 0, drawableWidth, drawableHeight,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

extern "C"
int main(int argc, char *argv[])
{
    // DPI awareness must be set before any other Window API calls. SDL doesn't do it for some reason?
#ifdef _WIN32
{
    HMODULE ShcoreLib = LoadLibraryW(L"Shcore.dll");
    if (ShcoreLib != NULL)
    {
        typedef HRESULT(WINAPI * PFNSETPROCESSDPIAWARENESSPROC)(PROCESS_DPI_AWARENESS);
        PFNSETPROCESSDPIAWARENESSPROC pfnSetProcessDpiAwareness = (PFNSETPROCESSDPIAWARENESSPROC)GetProcAddress(ShcoreLib, "SetProcessDpiAwareness");
        if (pfnSetProcessDpiAwareness != NULL)
        {
            HRESULT hr = pfnSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
            if (FAILED(hr))
            {
                _com_error err(hr);
                fwprintf(stderr, L"SetProcessDpiAwareness failed: %s\n", err.ErrorMessage());
            }
        }
        FreeLibrary(ShcoreLib);
    }
}
#endif

    if (SDL_Init(SDL_INIT_EVERYTHING))
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    // GL 4.1 for OS X support
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#ifdef _DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // Enable multisampling
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    // Enable SRGB
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

    // Don't need depth, it's done manually through the FBO.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    // Scale window accoridng to DPI zoom
    int windowDpiScaledWidth, windowDpiScaledHeight;
    {
        int windowDpiUnscaledWidth = 1280, windowDpiUnscaledHeight = 720;

        float defaultDpi;
#ifdef __APPLE__
        defaultDpi = 72.0f;
#else
        defaultDpi = 96.0f;
#endif

        float hdpi, vdpi;
        if (SDL_GetDisplayDPI(0, NULL, &hdpi, &vdpi))
        {
            hdpi = defaultDpi;
            vdpi = defaultDpi;
        }

        windowDpiScaledWidth = int(windowDpiUnscaledWidth * hdpi / defaultDpi);
        windowDpiScaledHeight = int(windowDpiUnscaledHeight * vdpi / defaultDpi);
    }

#ifdef __APPLE__
    Uint32 windowFlags = SDL_WINDOW_OPENGL;
#else
    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
#endif

    SDL_Window* window = SDL_CreateWindow(
        "fictional-doodle", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        windowDpiScaledWidth, windowDpiScaledHeight, windowFlags);

    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    if (!glctx)
    {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        exit(1);
    }

    InitGL();
    InitRenderer();

    ImGui_ImplSdlGL3_Init(window);

    // Initial resize to create framebuffers
    {
        int drawableWidth, drawableHeight;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        int numSamples;
        SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &numSamples);

        ResizeRenderer(windowWidth, windowHeight, drawableWidth, drawableHeight, numSamples);
    }

    InitScene();

    auto updateGuiFocus = [&] 
    {
        if (Renderer::GUIFocusEnabled)
        {
            // Warping mouse seems necessary to acquire mouse focus for OS X track pad.
            SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        else
        {
            SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
    };

    updateGuiFocus();

    Uint32 lastTicks = SDL_GetTicks();

    // main loop
    for (;;)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (Renderer::GUIFocusEnabled)
            {
                ImGui_ImplSdlGL3_ProcessEvent(&ev);
            }

            if (ev.type == SDL_QUIT)
            {
                goto endmainloop;
            }
            else if (ev.type == SDL_WINDOWEVENT)
            {
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    int drawableWidth, drawableHeight;
                    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);

                    int windowWidth, windowHeight;
                    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

                    int numSamples;
                    SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &numSamples);

                    ResizeRenderer(windowWidth, windowHeight, drawableWidth, drawableHeight, numSamples);
                }
            }
            else if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.keysym.sym == SDLK_ESCAPE)
                {
                    Renderer::GUIFocusEnabled = !Renderer::GUIFocusEnabled;
                }

                updateGuiFocus();
            }
        }

        ImGui_ImplSdlGL3_NewFrame(Renderer::GUIFocusEnabled);

        // for testing the gui
        ImGui::ShowTestWindow();
        ImGui::ShowMetricsWindow();

        Uint32 currTicks = SDL_GetTicks();
        PaintRenderer(window, currTicks - lastTicks);

        // Bind 0 to the draw framebuffer before swapping the window, because otherwise in Mac OS X nothing will happen.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        SDL_GL_SwapWindow(window);

        lastTicks = currTicks;
    }
    endmainloop:

    ImGui_ImplSdlGL3_Shutdown();
    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
