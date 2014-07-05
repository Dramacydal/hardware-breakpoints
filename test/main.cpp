#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <hardwarebp.h>
#include <assert.h>
#include "Helpers.h"
#include "ProcessDebugger.h"

class IncBreakPoint : public HardwareBreakpoint
{
    public:
        IncBreakPoint(DWORD _address, int _len, Condition _condition) :
            HardwareBreakpoint(_address, _len, _condition) { }

        virtual bool HandleException(CONTEXT& ctx) override
        {
            // skip 'inc a'
            ctx.Eip += 6;
            std::cout << "Triggered" << std::endl;
            return true;
        }
};

void main()
{
    if (!SetDebugPrivilege(true))
    {
        std::cout << "Failed to set debug privileges" << std::endl;
        return;
    }

    ProcessDebugger pd(L"program.exe");
    if (!pd.FindAndAttach())
    {
        std::cout << "Can't find process" << std::endl;
        return;
    }

    IncBreakPoint* bp = new IncBreakPoint(0x4012B0 - 0x400000, 1, HardwareBreakpoint::Condition::Code);
    if (!pd.AddBreakPoint(bp))
    {
        std::cout << "Failed to add breakpoint" << std::endl;
        return;
    }

    if (!pd.WaitForDebugEvents())
    {
        std::cout << "Fail while waiting for debug events" << std::endl;
        return;
    }

    pd.StopDebugging();
}
