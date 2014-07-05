#ifndef _PROCESSDEBUGGER_H
#define _PROCESSDEBUGGER_H

#include <set>
#include <Windows.h>

class HardwareBreakpoint;

class ProcessDebugger
{
    public:
        typedef std::set<HardwareBreakpoint*> BreakPointContainer;
        ProcessDebugger(wchar_t* _processName);
        bool FindAndAttach();
        bool AddBreakPoint(HardwareBreakpoint* bp);
        bool StopDebugging();
        bool RemoveBreakPoint(DWORD address);

        bool WaitForDebugEvents(DWORD time = INFINITE);

    protected:
        wchar_t* processName;
        DWORD processId;
        DWORD baseAddress;
        DWORD threadId;
        BreakPointContainer breakPoints;
};
#endif