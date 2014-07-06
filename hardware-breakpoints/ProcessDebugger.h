#ifndef _PROCESSDEBUGGER_H
#define _PROCESSDEBUGGER_H

#include <atomic>
#include <exception>
#include <set>
#include <Windows.h>
#include <thread>

class HardwareBreakpoint;

class MemoryException : public std::exception
{
    public:
        MemoryException(std::string message) : std::exception(message.c_str()) { }
};

class ProcessDebugger
{
    public:
        typedef std::set<HardwareBreakpoint*> BreakPointContainer;

        ProcessDebugger(std::wstring _processName);
        bool FindAndAttach();
        bool AddBreakPoint(std::wstring moduleName, HardwareBreakpoint* bp);
        bool StopDebugging();
        bool Detach();
        bool RemoveBreakPoint(DWORD address);
        bool RemoveBreakPoints();

        bool StartListener(DWORD time = INFINITE);

        DWORD GetModuleAddress(std::wstring moduleName) const;

        template <class T>
        T Read(std::wstring moduleName, DWORD addr);
        template <class T>
        void Write(std::wstring moduleName, DWORD addr, T& value);

        BreakPointContainer const& GetBreakPoints() const { return breakPoints; }
        bool IsDebugging() const { return isDebugging; }

        static std::thread* Run(ProcessDebugger* pd);

    protected:

        static void RunnerThread(ProcessDebugger* pd);
        std::atomic_bool isDebugging;
        std::atomic_bool detached;

        std::wstring processName;
        DWORD processId;
        DWORD baseAddress;
        DWORD threadId;
        BreakPointContainer breakPoints;
};
#endif