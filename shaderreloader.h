#pragma once

#include "opengl.h"

#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <vector>

struct ReloadableShader
{
    ReloadableShader(const char* filename, GLenum type)
        : Handle(0)
        , Type(type)
        , Filename(filename)
        , Timestamp(0)
    { }

    explicit ReloadableShader(const char* filename)
        : Handle(0)
        , Filename(filename)
        , Timestamp(0)
    {
        const char* exts[] = {
            ".vert", ".frag", ".geom", ".tesc", ".tese", "comp"
        };
        GLenum types[] = {
            GL_VERTEX_SHADER,
            GL_FRAGMENT_SHADER,
            GL_GEOMETRY_SHADER,
            GL_TESS_CONTROL_SHADER,
            GL_TESS_EVALUATION_SHADER,
            GL_COMPUTE_SHADER
        };

        int len = (int)strlen(filename);
        if (len >= 5)
        {
            bool foundType = false;
            for (int i = 0; i < sizeof(exts) / sizeof(*exts); i++)
            {
                if (strcmp(filename + len - 5, exts[i]) == 0)
                {
                    Type = types[i];
                    foundType = true;
                    break;
                }
            }

            if (!foundType)
            {
                fprintf(stderr, "Couldn't identify shader type from extension\n");
                exit(1);
            }
        }
    }

    GLuint Handle;
    GLenum Type;
    const char* Filename;
    uint64_t Timestamp;
};

struct ReloadableProgram
{
    ReloadableProgram(
        ReloadableShader* vs,
        ReloadableShader* fs,
        ReloadableShader* gs = NULL,
        ReloadableShader* tcs = NULL,
        ReloadableShader* tes = NULL)
        : Handle(0), VS(vs), FS(fs), GS(gs), TCS(tcs), TES(tes), CS(NULL), Varyings()
    { }

    explicit ReloadableProgram(
        ReloadableShader* onestage, const std::vector<const char*>& varyings)
        : Handle(0), VS(NULL), FS(NULL), GS(NULL), TCS(NULL), TES(NULL), CS(NULL), Varyings(varyings)
    {
        switch (onestage->Type)
        {
        case GL_VERTEX_SHADER: VS = onestage; break;
        case GL_FRAGMENT_SHADER: FS = onestage; break;
        case GL_GEOMETRY_SHADER: GS = onestage; break;
        case GL_TESS_CONTROL_SHADER: TCS = onestage; break;
        case GL_TESS_EVALUATION_SHADER: TES = onestage; break;
        case GL_COMPUTE_SHADER: CS = onestage; break;
        default: 
            fprintf(stderr, "Unknown shader type\n");
            exit(1);
        }
    }

    GLuint Handle;
    ReloadableShader* VS;
    ReloadableShader* FS;
    ReloadableShader* GS;
    ReloadableShader* TCS;
    ReloadableShader* TES;
    ReloadableShader* CS;

    // Transform feedback outputs to capture.
    std::vector<const char*> Varyings;
};

bool ReloadProgram(ReloadableProgram* program);
