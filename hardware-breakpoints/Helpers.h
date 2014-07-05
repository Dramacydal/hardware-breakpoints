#ifndef _HELPERS_H
#define _HELPERS_H

#include <Windows.h>

bool SetDebugPrivilege(bool on);
DWORD GetProcessID(wchar_t* szExeName);
DWORD GetProcessThreadID(DWORD dwProcessID);
DWORD GetModuleBaseAddress(DWORD processId, wchar_t* moduleName);

#endif