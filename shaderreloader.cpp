#include "shaderreloader.h"

#include <string>
#include <fstream>
#include <cassert>

#if defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#endif

static uint64_t GetShaderFileTimestamp(const char* filename)
{
    uint64_t timestamp = 0;

#ifdef _WIN32
    int filenameBufferSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, NULL, 0);
    if (filenameBufferSize == 0)
    {
        return 0;
    }
    
    WCHAR* wfilename = new WCHAR[filenameBufferSize];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, wfilename, filenameBufferSize))
    {
        HANDLE hFile = CreateFileW(wfilename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            FILETIME lastWriteTime;
            if (GetFileTime(hFile, NULL, NULL, &lastWriteTime))
            {
                LARGE_INTEGER largeWriteTime;
                largeWriteTime.HighPart = lastWriteTime.dwHighDateTime;
                largeWriteTime.LowPart = lastWriteTime.dwLowDateTime;
                timestamp = largeWriteTime.QuadPart;
            }
            CloseHandle(hFile);
        }
    }
    delete[] wfilename;
#elif defined(__APPLE__)
    struct stat buf;

    if (stat(filename, &buf) == -1)
    {
        perror(filename);
        return 0;
    }

    timestamp = buf.st_mtimespec.tv_sec;
#else
    static_assert(false, "no GetShaderFileTimestamp for this platform");
#endif

    return timestamp;
}

static std::string ShaderStringFromFile(const char* filename)
{
    std::ifstream fs(filename);
    if (!fs)
    {
        return "";
    }

    std::string s(
        std::istreambuf_iterator<char>{fs},
        std::istreambuf_iterator<char>{});

    return std::move(s);
}

bool ReloadProgram(ReloadableProgram* program)
{
    ReloadableShader* shaders[] = {
        program->VS,
        program->FS,
        program->GS,
        program->TCS,
        program->TES,
        program->CS
    };

    const char* shaderStageNames[] = {
        "vertex",
        "fragment",
        "geometry",
        "tessellation control",
        "tessellation evaluation",
        "compute"
    };

    bool anyChanged = false;
    bool anyErrors = false;

    for (int i = 0; i < sizeof(shaders) / sizeof(*shaders); i++)
    {
        if (!shaders[i])
        {
            continue;
        }

        uint64_t timestamp = GetShaderFileTimestamp(shaders[i]->Filename);
        if (shaders[i]->Timestamp < timestamp)
        {
            anyChanged = true;
            shaders[i]->Timestamp = timestamp;

            glDeleteShader(shaders[i]->Handle);
            shaders[i]->Handle = glCreateShader(shaders[i]->Type);

            std::string src = ShaderStringFromFile(shaders[i]->Filename);
            const char* csrc = src.c_str();
            glShaderSource(shaders[i]->Handle, 1, &csrc, NULL);
            glCompileShader(shaders[i]->Handle);

            GLint status;
            glGetShaderiv(shaders[i]->Handle, GL_COMPILE_STATUS, &status);
            if (!status)
            {
                GLint logLength;
                glGetShaderiv(shaders[i]->Handle, GL_INFO_LOG_LENGTH, &logLength);
                std::vector<GLchar> log(logLength + 1);
                glGetShaderInfoLog(shaders[i]->Handle, (GLsizei)log.size(), NULL, log.data());
                fprintf(stderr, "Error compiling %s shader %s: %s\n", shaderStageNames[i], shaders[i]->Filename, log.data());

                glDeleteShader(shaders[i]->Handle);
                shaders[i]->Handle = 0;
                anyErrors = true;
            }
        }
    }

    if (anyChanged && !anyErrors)
    {
        GLuint newProgram;
        newProgram = glCreateProgram();

        for (int i = 0; i < sizeof(shaders) / sizeof(*shaders); i++)
        {
            if (!shaders[i])
            {
                continue;
            }

            glAttachShader(newProgram, shaders[i]->Handle);
        }

        if (size(program->Varyings))
        {
            glTransformFeedbackVaryings(newProgram, (GLsizei)size(program->Varyings), data(program->Varyings), GL_INTERLEAVED_ATTRIBS);
        }

        glLinkProgram(newProgram);

        GLint status;
        glGetProgramiv(newProgram, GL_LINK_STATUS, &status);
        if (!status)
        {
            GLint logLength;
            glGetProgramiv(newProgram, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> log(logLength + 1);
            glGetProgramInfoLog(newProgram, (GLsizei)log.size(), NULL, log.data());
            fprintf(stderr, "Error linking program (");
            bool first = false;
            for (int i = 0; i < sizeof(shaders) / sizeof(*shaders); i++)
            {
                if (!shaders[i])
                {
                    continue;
                }

                if (!first)
                {
                    fprintf(stderr, ", ");
                }
                else
                {
                    first = true;
                }

                fprintf(stderr, "%s", shaders[i]->Filename);
            }
            fprintf(stderr, "): %s\n", log.data());
            glDeleteProgram(newProgram);
        }
        else
        {
            glDeleteProgram(program->Handle);
            program->Handle = newProgram;
            return true;
        }
    }

    return false;
}
