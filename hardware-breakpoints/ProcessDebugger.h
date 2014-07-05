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

        DWORD GetModuleAddress(std::wstring moduleName) const;

        template <class T>
        T Read(std::wstring moduleName, DWORD addr);
        template <class T>
        void Write(std::wstring moduleName, DWORD addr, T& value);

    protected:
        std::wstring processName;
        DWORD processId;
        DWORD baseAddress;
        DWORD threadId;
        BreakPointContainer breakPoints;
};
#endif