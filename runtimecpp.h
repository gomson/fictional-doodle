#pragma once

#include <string>
#include <unordered_map>
#include <map>

#ifdef _MSC_VER

#define UNICODE 1
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

struct RuntimeCpp
{
    RuntimeCpp(
        std::wstring dllname,
        const std::initializer_list<std::string>& procnames)
        : DLLName(std::move(dllname))
    {
        for (auto procname : procnames)
        {
            Procs[procname] = NULL;
        }
    }

    template<class ProcT>
    void GetProc(ProcT& proc, const char* name)
    {
        proc = reinterpret_cast<ProcT>(Procs[name]);
    }

    std::wstring DLLName;
    std::map<std::string, void*> Procs;

    HMODULE Module = NULL;
    LONGLONG LastWrite = 0;
};

bool PollDLLs(RuntimeCpp* rcpp);

#endif // _MSC_VER