#pragma once

#include <GL/glcorearb.h>

void InitGL();
void GetProcGL(void** proc, const char* name);
void CheckErrorGL(const char* description);

const char* FramebufferStatusToStringGL(GLenum err);
const char* DebugSourceToStringGL(GLenum source);
const char* DebugTypeToStringGL(GLenum type);
const char* DebugSeverityToStringGL(GLenum severity);

template<class F> struct ProcGL;

template<class F, class... Args>
struct ProcGL<F(*)(Args...)>
{
public:
    F(*fptr)(Args...);
    const char* fname;

    F operator()(Args... args)
    {
        F f = fptr(args...);
#ifdef _DEBUG
        CheckErrorGL(fname);
#endif
        return f;
    }
};

template<class... Args>
struct ProcGL<void(*)(Args...)>
{
public:
    void(*fptr)(Args...);
    const char* fname;

    void operator()(Args... args)
    {
        fptr(args...);
#ifdef _DEBUG
        CheckErrorGL(fname);
#endif
    }
};

#ifdef OPENGL_IMPLEMENTATION
#define PROCGL(type,name) ProcGL<type> name
#else
#define PROCGL(type,name) extern ProcGL<type> name
#endif

PROCGL(PFNGLGETINTEGERVPROC, glGetIntegerv);
PROCGL(PFNGLGETSTRINGIPROC, glGetStringi);
PROCGL(PFNGLGETSTRINGPROC, glGetString);
PROCGL(PFNGLCLEARPROC, glClear);
PROCGL(PFNGLCLEARCOLORPROC, glClearColor);
PROCGL(PFNGLENABLEPROC, glEnable);
PROCGL(PFNGLDISABLEPROC, glDisable);
PROCGL(PFNGLVIEWPORTPROC, glViewport);
PROCGL(PFNGLSCISSORPROC, glScissor);
PROCGL(PFNGLISENABLEDPROC, glIsEnabled);
PROCGL(PFNGLBLENDEQUATIONPROC, glBlendEquation);
PROCGL(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate);
PROCGL(PFNGLBLENDFUNCPROC, glBlendFunc);
PROCGL(PFNGLGENBUFFERSPROC, glGenBuffers);
PROCGL(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
PROCGL(PFNGLBINDBUFFERPROC, glBindBuffer);
PROCGL(PFNGLBINDBUFFERBASEPROC, glBindBufferBase);
PROCGL(PFNGLBUFFERDATAPROC, glBufferData);
PROCGL(PFNGLBUFFERSUBDATAPROC, glBufferSubData);
PROCGL(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
PROCGL(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
PROCGL(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
PROCGL(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
PROCGL(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
PROCGL(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer);
PROCGL(PFNGLCREATESHADERPROC, glCreateShader);
PROCGL(PFNGLDELETESHADERPROC, glDeleteShader);
PROCGL(PFNGLSHADERSOURCEPROC, glShaderSource);
PROCGL(PFNGLCOMPILESHADERPROC, glCompileShader);
PROCGL(PFNGLGETSHADERIVPROC, glGetShaderiv);
PROCGL(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
PROCGL(PFNGLCREATEPROGRAMPROC, glCreateProgram);
PROCGL(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
PROCGL(PFNGLLINKPROGRAMPROC, glLinkProgram);
PROCGL(PFNGLATTACHSHADERPROC, glAttachShader);
PROCGL(PFNGLDETACHSHADERPROC, glDetachShader);
PROCGL(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
PROCGL(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
PROCGL(PFNGLUSEPROGRAMPROC, glUseProgram);
PROCGL(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation);
PROCGL(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
PROCGL(PFNGLUNIFORM1IPROC, glUniform1i);
PROCGL(PFNGLUNIFORM2FPROC, glUniform2f);
PROCGL(PFNGLUNIFORM3FVPROC, glUniform3fv);
PROCGL(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv);
PROCGL(PFNGLGENTEXTURESPROC, glGenTextures);
PROCGL(PFNGLDELETETEXTURESPROC, glDeleteTextures);
PROCGL(PFNGLBINDTEXTUREPROC, glBindTexture);
PROCGL(PFNGLACTIVETEXTUREPROC, glActiveTexture);
PROCGL(PFNGLTEXBUFFERPROC, glTexBuffer);
PROCGL(PFNGLTEXIMAGE2DPROC, glTexImage2D);
PROCGL(PFNGLTEXIMAGE2DMULTISAMPLEPROC, glTexImage2DMultisample);
PROCGL(PFNGLTEXPARAMETERIPROC, glTexParameteri);
PROCGL(PFNGLTEXPARAMETERIVPROC, glTexParameteriv);
PROCGL(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap);
PROCGL(PFNGLDRAWELEMENTSPROC, glDrawElements);
PROCGL(PFNGLDRAWARRAYSPROC, glDrawArrays);
PROCGL(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC, glDrawElementsInstancedBaseVertex);
PROCGL(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);
PROCGL(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);
PROCGL(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
PROCGL(PFNGLFRAMEBUFFERTEXTUREPROC, glFramebufferTexture);
PROCGL(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer);
PROCGL(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus);
PROCGL(PFNGLDRAWBUFFERSPROC, glDrawBuffers);
PROCGL(PFNGLREADBUFFERPROC, glReadBuffer);
PROCGL(PFNGLTRANSFORMFEEDBACKVARYINGSPROC, glTransformFeedbackVaryings);
PROCGL(PFNGLGENTRANSFORMFEEDBACKSPROC, glGenTransformFeedbacks);
PROCGL(PFNGLBINDTRANSFORMFEEDBACKPROC, glBindTransformFeedback);
PROCGL(PFNGLBEGINTRANSFORMFEEDBACKPROC, glBeginTransformFeedback);
PROCGL(PFNGLENDTRANSFORMFEEDBACKPROC, glEndTransformFeedback);

template<class ProcT>
void GetProcGL(ProcGL<ProcT>& proc, const char* name)
{
    GetProcGL((void**)&proc.fptr, name);
    proc.fname = name;
}

struct GLDrawElementsIndirectCommand
{
    GLuint count;
    GLuint primCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
};

struct DrawArraysIndirectCommand
{
    GLuint count;
    GLuint primCount;
    GLuint first;
    GLuint baseInstance;
};
