#ifndef _PROCESSDEBUGGER_H
#define _PROCESSDEBUGGER_H

#include <set>
#include <Windows.h>

class HardwareBreakpoint;

class ProcessDebugger
{
    public:
        typedef std::set<HardwareBreakpoint*> BreakPointContainer;
        ProcessDebugger(std::wstring _processName);
        bool FindAndAttach();
        bool AddBreakPoint(std::wstring moduleName, HardwareBreakpoint* bp);
        bool StopDebugging();
        bool RemoveBreakPoint(DWORD address);

        bool WaitForDebugEvents(DWORD time = INFINITE);

    protected:
        std::wstring processName;
        DWORD processId;
        DWORD baseAddress;
        DWORD threadId;
        BreakPointContainer breakPoints;
};
#endif