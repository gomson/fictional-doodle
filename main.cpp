#include <SDL/SDL.h>
#include <GL/glcorearb.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cassert>
#include <type_traits>
#include <vector>
#include <sstream>
#include <string>
#include <fstream>
#include <functional>

PFNGLGETINTEGERVPROC glGetIntegerv;
PFNGLGETSTRINGIPROC glGetStringi;
PFNGLCLEARPROC glClear;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLENABLEPROC glEnable;
PFNGLDISABLEPROC glDisable;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glDrawElementsInstancedBaseVertex;

struct GLDrawElementsIndirectCommand
{
    GLuint count;
    GLuint primCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
};

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

    GLuint VAO;
    GLuint VBO;
    GLuint EBO;
    
    std::vector<GLDrawElementsIndirectCommand> DrawCmds;
    std::vector<glm::mat4> ModelWorldTransforms;

    GLuint SP;
    GLuint VS;
    GLuint FS;
    GLint MVPLoc;
}

void APIENTRY DebugCallbackGL(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    auto DebugSourceToString = [](GLenum source)
    {
        switch (source)
        {
        case GL_DEBUG_SOURCE_API: return "GL_DEBUG_SOURCE_API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "GL_DEBUG_SOURCE_WINDOW_SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "GL_DEBUG_SOURCE_SHADER_COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "GL_DEBUG_SOURCE_THIRD_PARTY";
        case GL_DEBUG_SOURCE_APPLICATION: return "GL_DEBUG_SOURCE_APPLICATION";
        case GL_DEBUG_SOURCE_OTHER: return "GL_DEBUG_SOURCE_OTHER";
        default: return "(unknown)";
        }
    };

    auto DebugTypeToString = [](GLenum type)
    {
        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR: return "GL_DEBUG_TYPE_ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY: return "GL_DEBUG_TYPE_PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE: return "GL_DEBUG_TYPE_PERFORMANCE";
        case GL_DEBUG_TYPE_OTHER: return "GL_DEBUG_TYPE_OTHER";
        default: return "(unknown)";
        }
    };

    auto DebugSeverityToString = [](GLenum severity)
    {
        switch (severity)
        {
        case GL_DEBUG_SEVERITY_HIGH: return "GL_DEBUG_SEVERITY_HIGH";
        case GL_DEBUG_SEVERITY_MEDIUM: return "GL_DEBUG_SEVERITY_MEDIUM";
        case GL_DEBUG_SEVERITY_LOW: return "GL_DEBUG_SEVERITY_LOW";
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "GL_DEBUG_SEVERITY_NOTIFICATION";
        default: return "(unknown)";
        }
    };

    fprintf(stderr,
        "Debug callback: {\n"
        "  source = \"%s\",\n"
        "  type = \"%s\",\n"
        "  id = %d,\n"
        "  severity = \"%s\",\n"
        "  message = \"%s\"\n"
        "}\n",
        DebugSourceToString(source),
        DebugTypeToString(type),
        id,
        DebugSeverityToString(severity),
        message);
}

void InitGL()
{
    // Get GL proc in a type safe way and assert its existence
    auto GetProc = [](auto& proc, const char* name)
    {
        proc = static_cast<std::remove_reference_t<decltype(proc)>>(SDL_GL_GetProcAddress(name));
        if (!proc)
        {
            fprintf(stderr, "SDL_GL_GetProcAddress(%s): %s\n", name, SDL_GetError());
            exit(1);
        }
    };

    GetProc(glGetIntegerv, "glGetIntegerv");
    GetProc(glGetStringi, "glGetStringi");
    GetProc(glClear, "glClear");
    GetProc(glClearColor, "glClearColor");
    GetProc(glEnable, "glEnable");
    GetProc(glDisable, "glDisable");
    GetProc(glGenBuffers, "glGenBuffers");
    GetProc(glBindBuffer, "glBindBuffer");
    GetProc(glBufferData, "glBufferData");
    GetProc(glGenVertexArrays, "glGenVertexArrays");
    GetProc(glBindVertexArray, "glBindVertexArray");
    GetProc(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    GetProc(glVertexAttribPointer, "glVertexAttribPointer");
    GetProc(glCreateShader, "glCreateShader");
    GetProc(glShaderSource, "glShaderSource");
    GetProc(glCompileShader, "glCompileShader");
    GetProc(glGetShaderiv, "glGetShaderiv");
    GetProc(glGetShaderInfoLog, "glGetShaderInfoLog");
    GetProc(glCreateProgram, "glCreateProgram");
    GetProc(glLinkProgram, "glLinkProgram");
    GetProc(glAttachShader, "glAttachShader");
    GetProc(glGetProgramiv, "glGetProgramiv");
    GetProc(glGetProgramInfoLog, "glGetProgramInfoLog");
    GetProc(glUseProgram, "glUseProgram");
    GetProc(glGetUniformLocation, "glGetUniformLocation");
    GetProc(glUniformMatrix4fv, "glUniformMatrix4fv");
    GetProc(glBindFramebuffer, "glBindFramebuffer");
    GetProc(glDrawElementsInstancedBaseVertex, "glDrawElementsInstancedBaseVertex");

    GLint majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

    int contextFlags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);

    GLint numExtensions;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);

    // set up debugging if this is a debug context
    if (contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        if (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 3))
        {
            PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
            GetProc(glDebugMessageCallback, "glDebugMessageCallback");
            glDebugMessageCallback(DebugCallbackGL, NULL);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            goto debug_enabled;
        }

        for (int i = 0; i < numExtensions; i++)
        {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(ext, "GL_ARB_debug_output") == 0)
            {
                PFNGLDEBUGMESSAGECALLBACKARBPROC glDebugMessageCallbackARB;
                GetProc(glDebugMessageCallbackARB, "glDebugMessageCallbackARB");
                glDebugMessageCallbackARB(DebugCallbackGL, NULL);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
                goto debug_enabled;
            }
            else if (strcmp(ext, "GL_KHR_debug") == 0)
            {
                PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;
                GetProc(glDebugMessageCallback, "glDebugMessageCallback");
                glDebugMessageCallback(DebugCallbackGL, NULL);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                goto debug_enabled;
            }
        }

        fprintf(stdout, "Failed to init debug output\n");
    debug_enabled:;
    }
}

void InitScene()
{
    // load scene
    const aiScene* scene = aiImportFile("assets/teapot/teapot.obj", aiProcessPreset_TargetRealtime_MaxQuality);
    if (!scene)
    {
        fprintf(stderr, "aiImportFile: %s\n", aiGetErrorString());
        exit(1);
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
    addNode = [&addNode, &meshDraws](const aiNode* node, glm::mat4 transform)
    {
        transform = transform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));

        // add draws for all meshes assigned to this node
        for (int nodeMeshIdx = 0; nodeMeshIdx < (int)node->mNumMeshes; nodeMeshIdx++)
        {
            GLDrawElementsIndirectCommand draw = meshDraws[node->mMeshes[nodeMeshIdx]];
            Scene::DrawCmds.push_back(draw);
            Scene::ModelWorldTransforms.push_back(transform);
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

    Scene::MVPLoc = glGetUniformLocation(Scene::SP, "ModelViewProjection");
    if (Scene::MVPLoc == -1)
    {
        fprintf(stderr, "Couldn't find ModelViewProjection uniform\n");
        exit(1);
    }
}

void PaintGL(SDL_Window* window)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
        {
            exit(0);
        }
    }

    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    glm::mat4 projection = glm::perspective(70.0f, (float)windowWidth / windowHeight, 0.01f, 1000.0f);
    glm::mat4 worldView = glm::lookAt(glm::vec3(100), glm::vec3(0,20,0), glm::vec3(0, 1, 0));
    glm::mat4 worldViewProjection = projection * worldView;

    glClearColor(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(Scene::VAO);
    glUseProgram(Scene::SP);

    for (int drawIdx = 0; drawIdx < (int)Scene::DrawCmds.size(); drawIdx++)
    {
        GLDrawElementsIndirectCommand cmd = Scene::DrawCmds[drawIdx];
        assert(cmd.baseInstance == 0); // no base instance because OS X
        assert(cmd.primCount == 1); // assuming no instancing cuz lack of baseInstance makes it boring

        glm::mat4 mvp = worldViewProjection * Scene::ModelWorldTransforms[drawIdx];
        glUniformMatrix4fv(Scene::MVPLoc, 1, GL_FALSE, glm::value_ptr(mvp));
       
        glDrawElementsInstancedBaseVertex(GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT, (GLvoid*)(cmd.firstIndex * sizeof(GLuint)), cmd.primCount, cmd.baseVertex);
    }

    glBindVertexArray(0);
    glUseProgram(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
}

extern "C"
int main(int argc, char *argv[])
{
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

    SDL_Window* window = SDL_CreateWindow("fictional-doodle", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
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
    InitScene();

    // main loop
    for (;;)
    {
        PaintGL(window);

        // Bind 0 to the draw framebuffer before swapping the window, because otherwise in Mac OS X nothing will happen.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}