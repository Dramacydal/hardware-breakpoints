#include <Windows.h>
#include <TlHelp32.h>
#include "Helpers.h"

bool SetDebugPrivilege(bool on)
{
    HANDLE hToken;
    TOKEN_PRIVILEGES token_privileges;
    DWORD dwSize;

    ZeroMemory(&token_privileges, sizeof(token_privileges));
    token_privileges.PrivilegeCount = 1;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
        return FALSE;

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &token_privileges.Privileges[0].Luid))
    {
        CloseHandle(hToken);
        return FALSE;
    }

    if (on)
        token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;

    if(!AdjustTokenPrivileges ( hToken, FALSE, &token_privileges, 0, NULL, &dwSize))
    {
        CloseHandle(hToken);
        return false;
    }

    return (bool)CloseHandle(hToken);
}

// gets process id of given image name
DWORD GetProcessID(wchar_t* szExeName)
{
    PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(Process32First(hSnapshot, &pe))
        while(Process32Next(hSnapshot, &pe))
            if(!wcscmp(pe.szExeFile, szExeName))
                return pe.th32ProcessID;

    return 0;
}

DWORD GetProcessThreadID(DWORD dwProcessID)
{
    THREADENTRY32 te = { sizeof(THREADENTRY32) };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

    if(Thread32First(hSnapshot, &te))
        while(Thread32Next(hSnapshot, &te))
            if(te.th32OwnerProcessID == dwProcessID)
                return te.th32ThreadID;

    return 0;
}

DWORD GetModuleBaseAddress(DWORD processId, wchar_t* moduleName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    MODULEENTRY32 module;
    module.dwSize = sizeof(MODULEENTRY32);
    if (!Module32First(snapshot, &module))
        return 0;

    do
    {
        if (lstrcmpi(moduleName, module.szModule) == 0)
            return (DWORD)module.modBaseAddr;
    }
    while( Module32Next(snapshot, &module) );
    CloseHandle(snapshot);
    return 0;
}