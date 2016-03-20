#include "runtimecpp.h"

#ifdef _MSC_VER
bool PollDLLs(RuntimeCpp* rcpp)
{
    bool reloaded = false;

    HANDLE hFile = CreateFileW(rcpp->DLLName.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        FILETIME lastWriteTime;
        if (GetFileTime(hFile, NULL, NULL, &lastWriteTime))
        {
            LARGE_INTEGER largeWriteTime;
            largeWriteTime.HighPart = lastWriteTime.dwHighDateTime;
            largeWriteTime.LowPart = lastWriteTime.dwLowDateTime;

            if (rcpp->LastWrite < largeWriteTime.QuadPart)
            {
                if (rcpp->Module)
                {
                    FreeLibrary(rcpp->Module);
                    for (auto& p : rcpp->Procs)
                    {
                        p.second = NULL;
                    }
                }

                std::wstring tmpname = L"rcpp.";
                tmpname += rcpp->DLLName;

                if (CopyFileW(rcpp->DLLName.c_str(), tmpname.c_str(), FALSE))
                {
                    rcpp->Module = LoadLibraryW(tmpname.c_str());
                    if (rcpp->Module)
                    {
                        for (auto& p : rcpp->Procs)
                        {
                            p.second = GetProcAddress(rcpp->Module, p.first.c_str());
                        }
                        rcpp->LastWrite = largeWriteTime.QuadPart;
                        reloaded = true;
                    }
                }
            }
        }
        CloseHandle(hFile);
    }

    return reloaded;
}
#endif // _MSC_VER