#define OPENGL_IMPLEMENTATION
#include "opengl.h"

#include <SDL.h>
#include <GL/glcorearb.h>

#include <vector>
#include <cstdio>

#ifdef _MSC_VER
#define BREAKPOINT __debugbreak()
#else
#define BREAKPOINT asm("int $3")
#endif

void GetProcGL(void** proc, const char* name)
{
    *proc = SDL_GL_GetProcAddress(name);
    if (!*proc)
    {
        fprintf(stderr, "SDL_GL_GetProcAddress(%s): %s\n", name, SDL_GetError());
        exit(1);
    }
}

void CheckErrorGL(const char* description)
{
    static PFNGLGETERRORPROC pfnglGetError = (PFNGLGETERRORPROC)SDL_GL_GetProcAddress("glGetError");
    GLenum err = pfnglGetError();
    const char* errmsg = NULL;
    switch (err)
    {
    case GL_INVALID_ENUM: errmsg = "GL_INVALID_ENUM"; break;
    case GL_INVALID_VALUE: errmsg = "GL_INVALID_VALUE"; break;
    case GL_INVALID_OPERATION: errmsg = "GL_INVALID_OPERATION"; break;
    case GL_STACK_OVERFLOW: errmsg = "GL_STACK_OVERFLOW"; break;
    case GL_STACK_UNDERFLOW: errmsg = "GL_STACK_UNDERFLOW"; break;
    case GL_OUT_OF_MEMORY: errmsg = "GL_OUT_OF_MEMORY"; break;
    case GL_INVALID_FRAMEBUFFER_OPERATION: errmsg = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
    case GL_CONTEXT_LOST: errmsg = "GL_CONTEXT_LOST"; break;
    default: break;
    }
    if (errmsg != NULL)
    {
        fprintf(stderr, "OpenGL error (%s): %s\n", description, errmsg);
        BREAKPOINT;
    }
}

const char* FramebufferStatusToStringGL(GLenum err)
{
    const char* errmsg = NULL;
    switch (err)
    {
    case GL_FRAMEBUFFER_COMPLETE: errmsg = "GL_FRAMEBUFFER_COMPLETE"; break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"; break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"; break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"; break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"; break;
    case GL_FRAMEBUFFER_UNSUPPORTED: errmsg = "GL_FRAMEBUFFER_UNSUPPORTED"; break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"; break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: errmsg = "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS"; break;
    default: break;
    }
    if (errmsg != NULL)
    {
        fprintf(stderr, "OpenGL error: %s\n", errmsg);
        BREAKPOINT;
    }
    return errmsg;
}

const char* DebugSourceToStringGL(GLenum source)
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
}

const char* DebugTypeToStringGL(GLenum type)
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
}

const char* DebugSeverityToStringGL(GLenum severity)
{
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH: return "GL_DEBUG_SEVERITY_HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM: return "GL_DEBUG_SEVERITY_MEDIUM";
    case GL_DEBUG_SEVERITY_LOW: return "GL_DEBUG_SEVERITY_LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "GL_DEBUG_SEVERITY_NOTIFICATION";
    default: return "(unknown)";
    }
}

void APIENTRY DebugCallbackGL(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        fprintf(stderr,
            "Debug callback: {\n"
            "  source = \"%s\",\n"
            "  type = \"%s\",\n"
            "  id = %d,\n"
            "  severity = \"%s\",\n"
            "  message = \"%s\"\n"
            "}\n",
            DebugSourceToStringGL(source),
            DebugTypeToStringGL(type),
            id,
            DebugSeverityToStringGL(severity),
            message);
    }

    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION &&
        source != GL_DEBUG_SOURCE_SHADER_COMPILER)
    {
        BREAKPOINT;
    }
}

void InitGL()
{
    GetProcGL(glGetFloatv, "glGetFloatv");
    GetProcGL(glGetIntegerv, "glGetIntegerv");
    GetProcGL(glGetStringi, "glGetStringi");
    GetProcGL(glGetString, "glGetString");
    GetProcGL(glClear, "glClear");
    GetProcGL(glClearColor, "glClearColor");
    GetProcGL(glEnable, "glEnable");
    GetProcGL(glDisable, "glDisable");
    GetProcGL(glIsEnabled, "glIsEnabled");
    GetProcGL(glDepthFunc, "glDepthFunc");
    GetProcGL(glDepthMask, "glDepthMask");
    GetProcGL(glViewport, "glViewport");
    GetProcGL(glScissor, "glScissor");
    GetProcGL(glBlendEquation, "glBlendEquation");
    GetProcGL(glBlendFunc, "glBlendFunc");
    GetProcGL(glBlendFuncSeparate, "glBlendFuncSeparate");
    GetProcGL(glBlendEquationSeparate, "glBlendEquationSeparate");
    GetProcGL(glPolygonOffset, "glPolygonOffset");
    GetProcGL(glGenBuffers, "glGenBuffers");
    GetProcGL(glDeleteBuffers, "glDeleteBuffers");
    GetProcGL(glBindBuffer, "glBindBuffer");
    GetProcGL(glBindBufferBase, "glBindBufferBase");
    GetProcGL(glBufferData, "glBufferData");
    GetProcGL(glBufferSubData, "glBufferSubData");
    GetProcGL(glMapBuffer, "glMapBuffer");
    GetProcGL(glUnmapBuffer, "glUnmapBuffer");
    GetProcGL(glGenVertexArrays, "glGenVertexArrays");
    GetProcGL(glDeleteVertexArrays, "glDeleteVertexArrays");
    GetProcGL(glBindVertexArray, "glBindVertexArray");
    GetProcGL(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    GetProcGL(glVertexAttribPointer, "glVertexAttribPointer");
    GetProcGL(glVertexAttribIPointer, "glVertexAttribIPointer");
    GetProcGL(glCreateShader, "glCreateShader");
    GetProcGL(glDeleteShader, "glDeleteShader");
    GetProcGL(glShaderSource, "glShaderSource");
    GetProcGL(glCompileShader, "glCompileShader");
    GetProcGL(glGetShaderiv, "glGetShaderiv");
    GetProcGL(glGetShaderInfoLog, "glGetShaderInfoLog");
    GetProcGL(glCreateProgram, "glCreateProgram");
    GetProcGL(glDeleteProgram, "glDeleteProgram");
    GetProcGL(glLinkProgram, "glLinkProgram");
    GetProcGL(glAttachShader, "glAttachShader");
    GetProcGL(glDetachShader, "glDetachShader");
    GetProcGL(glGetProgramiv, "glGetProgramiv");
    GetProcGL(glGetProgramInfoLog, "glGetProgramInfoLog");
    GetProcGL(glUseProgram, "glUseProgram");
    GetProcGL(glGetAttribLocation, "glGetAttribLocation");
    GetProcGL(glGetUniformLocation, "glGetUniformLocation");
    GetProcGL(glUniform1i, "glUniform1i");
    GetProcGL(glUniform2f, "glUniform2f");
    GetProcGL(glUniform3fv, "glUniform3fv");
    GetProcGL(glUniformMatrix4fv, "glUniformMatrix4fv");
    GetProcGL(glGenTextures, "glGenTextures");
    GetProcGL(glDeleteTextures, "glDeleteTextures");
    GetProcGL(glBindTexture, "glBindTexture");
    GetProcGL(glActiveTexture, "glActiveTexture");
    GetProcGL(glTexBuffer, "glTexBuffer");
    GetProcGL(glTexImage2D, "glTexImage2D");
    GetProcGL(glTexImage2DMultisample, "glTexImage2DMultisample");
    GetProcGL(glTexParameterf, "glTexParameterf");
    GetProcGL(glTexParameteri, "glTexParameteri");
    GetProcGL(glTexParameteriv, "glTexParameteriv");
    GetProcGL(glTexParameterfv, "glTexParameterfv");
    GetProcGL(glGenerateMipmap, "glGenerateMipmap");
    GetProcGL(glDrawElements, "glDrawElements");
    GetProcGL(glDrawArrays, "glDrawArrays");
    GetProcGL(glDrawElementsInstancedBaseVertex, "glDrawElementsInstancedBaseVertex");
    GetProcGL(glGenFramebuffers, "glGenFramebuffers");
    GetProcGL(glDeleteFramebuffers, "glDeleteFramebuffers");
    GetProcGL(glBindFramebuffer, "glBindFramebuffer");
    GetProcGL(glFramebufferTexture, "glFramebufferTexture");
    GetProcGL(glBlitFramebuffer, "glBlitFramebuffer");
    GetProcGL(glCheckFramebufferStatus, "glCheckFramebufferStatus");
    GetProcGL(glDrawBuffers, "glDrawBuffers");
    GetProcGL(glReadBuffer, "glReadBuffer");
    GetProcGL(glTransformFeedbackVaryings, "glTransformFeedbackVaryings");
    GetProcGL(glGenTransformFeedbacks, "glGenTransformFeedbacks");
    GetProcGL(glBindTransformFeedback, "glBindTransformFeedback");
    GetProcGL(glBeginTransformFeedback, "glBeginTransformFeedback");
    GetProcGL(glEndTransformFeedback, "glEndTransformFeedback");
    GetProcGL(glPointSize, "glPointSize");
    GetProcGL(glFinish, "glFinish");

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
            ProcGL<PFNGLDEBUGMESSAGECALLBACKPROC> glDebugMessageCallback;
            GetProcGL(glDebugMessageCallback, "glDebugMessageCallback");
            glDebugMessageCallback(DebugCallbackGL, NULL);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            goto debug_enabled;
        }

        for (int i = 0; i < numExtensions; i++)
        {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            if (strcmp(ext, "GL_ARB_debug_output") == 0)
            {
                ProcGL<PFNGLDEBUGMESSAGECALLBACKARBPROC> glDebugMessageCallbackARB;
                GetProcGL(glDebugMessageCallbackARB, "glDebugMessageCallbackARB");
                glDebugMessageCallbackARB(DebugCallbackGL, NULL);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
                goto debug_enabled;
            }
            else if (strcmp(ext, "GL_KHR_debug") == 0)
            {
                ProcGL<PFNGLDEBUGMESSAGECALLBACKPROC> glDebugMessageCallback;
                GetProcGL(glDebugMessageCallback, "glDebugMessageCallback");
                glDebugMessageCallback(DebugCallbackGL, NULL);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                goto debug_enabled;
            }
        }

        fprintf(stdout, "Failed to init debug output\n");
    debug_enabled:;
    }
}
