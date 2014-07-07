#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <hardwarebp.h>
#include <assert.h>
#include <signal.h>
#include "Helpers.h"
#include "ProcessDebugger.h"

class IncBreakPoint : public HardwareBreakpoint
{
    public:
        IncBreakPoint(int _address, int _len, Condition _condition) :
            HardwareBreakpoint(_address, _len, _condition) { }

        virtual bool HandleException(CONTEXT& ctx, ProcessDebugger* pd) override
        {
            // skip 'inc a'
            ctx.Eip += 6;

            // inc 'a' artificially
            DWORD addr = 0x404430 - 0x400000;
            int a = pd->Read<int>(L"process.exe", addr) + 10;
            pd->Write<int>(L"process.exe", addr, a);

            std::cout << "Triggered " << std::endl;
            return true;
        }
};

ProcessDebugger* pd;

void SignalHandler(int s)
{
    switch (s)
    {
        case SIGINT:
            printf("Caught SIGINT\n");
            if (pd)
                pd->StopDebugging();
            break;
        default:
            break;
    }
}

void main()
{
    signal(SIGINT, SignalHandler);
    if (!SetDebugPrivilege(true))
    {
        std::cout << "Failed to set debug privileges" << std::endl;
        return;
    }

    pd = new ProcessDebugger(L"program.exe");
    std::thread* th = ProcessDebugger::Run(pd);


    if (!pd->WaitForComeUp(500))
    {
        std::cout << "Failed to start thread" << std::endl;
        return;
    }

    IncBreakPoint* bp = new IncBreakPoint(0x4012B0 - 0x400000, 1, HardwareBreakpoint::Condition::Code);
    if (!pd->AddBreakPoint(L"program.exe", bp))
    {
        std::cout << "Failed to add breakpoint" << std::endl;
        return;
    }

    th->join();
}
