#include "ProcessDebugger.h"
#include "Helpers.h"
#include "hardwarebp.h"

ProcessDebugger::ProcessDebugger(wchar_t* _processName) : processName(_processName) { }

bool ProcessDebugger::FindAndAttach()
{
    processId = GetProcessID(processName);
    if (!processId)
        return false;

    baseAddress = GetModuleBaseAddress(processId, processName);
    if (!baseAddress)
        return false;

    // start debugging process so we get access
    if (!DebugActiveProcess(processId))
        return false;

    threadId = GetProcessThreadID(processId);
    if (!threadId)
        return false;

    return true;
}

bool ProcessDebugger::AddBreakPoint(HardwareBreakpoint* bp)
{
    //HardwareBreakpoint* bp = new HardwareBreakpoint(threadId, baseAddress + offset, len, HardwareBreakpoint::Condition(condition));
    try
    {
        bp->Shift(baseAddress);
        bp->Set(threadId);
        breakPoints.insert(bp);
    }
    catch (BreakPointException& e)
    {
        delete bp;
        return false;
    }

    return true;
}

bool ProcessDebugger::StopDebugging()
{
    for (auto bp : breakPoints)
        delete bp;

    return DebugActiveProcessStop(processId);
}

bool ProcessDebugger::RemoveBreakPoint(DWORD address)
{
    try
    {
        for (auto& bp : breakPoints)
        {
            if (bp->GetAddress() == address)
            {
                delete bp;
                breakPoints.erase(bp);
                break;
            }
        }
    }
    catch (BreakPointException& e)
    {
        return false;
    }

    return true;
}

bool ProcessDebugger::WaitForDebugEvents(DWORD time)
{
    DEBUG_EVENT DebugEvent;
    for (;;)
    {
        if (!WaitForDebugEvent(&DebugEvent, time))
            return false;

        bool okEvent = false;
        switch(DebugEvent.dwDebugEventCode)
        {
            case EXCEPTION_DEBUG_EVENT:
                // exception occured!
                if(DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
                {
                    //std::cout << "Debug event occured! Process Id: " << DebugEvent.dwProcessId << " Thread Id: " << DebugEvent.dwThreadId << " Addr: " << std::hex << DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress << std::dec << std::endl;
                    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, DebugEvent.dwThreadId);
                    //assert(hThread);


                    CONTEXT Context;
                    Context.ContextFlags = CONTEXT_FULL;
                    GetThreadContext(hThread, &Context);

                    for (auto bp : breakPoints)
                        if (bp->GetAddress() == Context.Eip)
                            bp->HandleException(Context);

                    //std::cout << std::hex << Context.Eip << std::dec << std::endl;
                    //Context.Eip += 6;
                    SetThreadContext(hThread, &Context);

                    okEvent = true;
                }
                else
                {
                    //std::cout << "Unknown debug event occured! Process Id: " << DebugEvent.dwProcessId << " Thread Id: " << DebugEvent.dwThreadId << std::endl;
                }
                break;
            default:
                break;
        }

        if (!okEvent)
        {
            //std::cout << "Unknown debug event. Code: " << DebugEvent.dwDebugEventCode << " Exc Code: " << DebugEvent.u.Exception.ExceptionRecord.ExceptionCode << std::endl;
            if (!ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED))
            {
                //std::cout << "Failed to continue debug event #1" << std::endl;
                return false;
            }
        }
        else
        {
            if (!ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE))
            {
                //std::cout << "Failed to continue debug event #1" << std::endl;
                return false;
            }
        }
    }
}
