#include "TorPlusPlus/torplusplus.hpp"
#include "torExeRes.h"

#include <Windows.h>
#include <string>
#include <fstream>

HMODULE GetCurrentModule() {
    HMODULE hModule = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)GetCurrentModule, &hModule);
    return hModule;
}

void dropTor() {
    // Get a pointer to tor.zip
    HMODULE hCurrentModule = GetCurrentModule();
    HRSRC hTorZipRes = FindResource(hCurrentModule, MAKEINTRESOURCE(TOREXE), MAKEINTRESOURCE(EXEFILE));
    HGLOBAL hTorZipData = LoadResource(hCurrentModule, hTorZipRes);
    DWORD hTorZipSize = SizeofResource(hCurrentModule, hTorZipRes);
    char* hTorZipFinal = (char*)LockResource(hTorZipData);
    // Write hTorZipFinal to disk
    std::ofstream torZipFile("tor.exe", std::ios::binary);
    torZipFile.write(hTorZipFinal, hTorZipSize);
    torZipFile.close();
}

#define HOST "cryptbbtg65gibadeeo2awe3j7s6evg7eklserehqr4w4e2bis5tebid.onion"

int main() {
    if (GetFileAttributes(L"tor.exe") == INVALID_FILE_ATTRIBUTES) {
        dropTor();
    }

    return 0;
}