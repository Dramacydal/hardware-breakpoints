#include "ProcessDebugger.h"
#include "Helpers.h"
#include "hardwarebp.h"
#include<iostream>

ProcessDebugger::ProcessDebugger(std::wstring _processName) : processName(_processName)
{
    isDebugging = false;
    detached = false;
}

bool ProcessDebugger::FindAndAttach()
{
    processId = GetProcessID(processName.c_str());
    if (!processId)
        return false;

    baseAddress = GetModuleBaseAddress(processId, processName.c_str());
    if (!baseAddress)
        return false;

    // start debugging process so we get access
    if (!DebugActiveProcess(processId))
        return false;

    if (!DebugSetProcessKillOnExit(FALSE))
        return false;

    threadId = GetProcessThreadID(processId);
    if (!threadId)
        return false;

    isDebugging = true;
    return true;
}

DWORD ProcessDebugger::GetModuleAddress(std::wstring moduleName) const
{
    HMODULE hMod = 0;
    if (moduleName.find(L".exe") == std::wstring::npos)
    {
        hMod = GetModuleHandleW(moduleName.c_str());
        if (!hMod)
            hMod = LoadLibraryW(moduleName.c_str());
    }
    else
        hMod = (HMODULE)baseAddress;

    return (DWORD)hMod;
}

bool ProcessDebugger::AddBreakPoint(std::wstring moduleName, HardwareBreakpoint* bp)
{
    HMODULE hMod = (HMODULE)GetModuleAddress(moduleName);
    if (!hMod)
        return false;

    int offs = (int)bp->GetAddress();
    if (offs < 0)
        bp->Shift((DWORD)GetProcAddress(hMod, (LPCSTR)(-offs)), true);
    else
        bp->Shift((DWORD)hMod);

    try
    {
        bp->Set(threadId);
    }
    catch (BreakPointException&)
    {
        return false;
    }

    breakPoints.insert(bp);

    return true;
}

bool ProcessDebugger::Detach()
{
    if (detached)
        return true;
    detached = true;

    if (!RemoveBreakPoints())
        return false;

    return DebugActiveProcessStop(processId) ? true : false;
}

bool ProcessDebugger::StopDebugging()
{
    if (!isDebugging)
        return true;
    isDebugging = false;

    return true;
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
    catch (BreakPointException&)
    {
        return false;
    }

    return true;
}

bool ProcessDebugger::RemoveBreakPoints()
{
    try
    {
        for (auto bp : breakPoints)
            delete bp;
        breakPoints.clear();
    }
    catch (BreakPointException&)
    {
        return false;
    }

    return true;
}

RunnerError ProcessDebugger::StartListener(DWORD time)
{
    DEBUG_EVENT DebugEvent;
    for (int i = 0; isDebugging; ++i)
    {
        if (!WaitForDebugEvent(&DebugEvent, time))
        {
            if (!isDebugging)
                break;
            continue;
        }

        //std::cout << "Event: " << DebugEvent.dwDebugEventCode << std::endl;

        bool okEvent = false;
        switch (DebugEvent.dwDebugEventCode)
        {
            case RIP_EVENT:
            case EXIT_PROCESS_DEBUG_EVENT:
                //std::cout << "Rip" << std::endl;
                detached = true;
                isDebugging = false;
                return ERR_OK;
            case EXCEPTION_DEBUG_EVENT:
                // exception occured!
                if(DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
                {
                    for (auto bp : breakPoints)
                    {
                        try
                        {
                            if (bp->OnEvent(DebugEvent, this))
                            {
                                okEvent = true;
                                break;
                            }
                        }
                        catch (BreakPointException&)
                        {
                            return ERR_BREAKPOINT_FAIL;
                        }
                        catch (MemoryException&)
                        {
                            return ERR_MEMORY_ACCESS;
                        }
                    }
                }
                else
                {
                    //std::cout << "Unknown debug event occured! Process Id: " << DebugEvent.dwProcessId << " Thread Id: " << DebugEvent.dwThreadId << std::endl;
                }
                break;
            default:
                break;
        }

        //if (i > 15)
        //    StopDebugging();

        if (!isDebugging)
        {
            detached = true;
            if (!RemoveBreakPoints())
                return ERR_BREAKPOINT_FAIL;
            if (!ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, okEvent ? DBG_CONTINUE : DBG_EXCEPTION_NOT_HANDLED))
                return ERR_DETACH_FAIL;
            if (!DebugActiveProcessStop(processId))
                return ERR_DETACH_FAIL;
            return ERR_OK;
        }

        ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, okEvent ? DBG_CONTINUE : DBG_EXCEPTION_NOT_HANDLED);
    }

    return Detach() ? ERR_OK : ERR_DETACH_FAIL;
}

void ProcessDebugger::RunnerThread(ProcessDebugger* pd)
{
    if (!pd->FindAndAttach())
    {
        std::cout << "Error: Can't find or attach process" << std::endl;
        return;
    }

    try
    {
        RunnerError err = pd->StartListener(INFINITE);
        if (err != ERR_OK)
        {
            std::cout << "Listener Error: " << err << std::endl;
            return;
        }
    }
    catch (BreakPointException& e)
    {
        std::cout << "Breakpoint error: " << e.what() << std::endl;
    }
    catch (MemoryException& e)
    {
        std::cout << "Memory error: " << e.what() << std::endl;
    }

    try
    {
        pd->Detach();
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

std::thread* ProcessDebugger::Run(ProcessDebugger* pd)
{
    return new std::thread(RunnerThread, pd);
}

bool ProcessDebugger::WaitForComeUp(DWORD msec)
{
    if (isDebugging)
        return true;

    std::cout << "Waiting for thread to come up..." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(msec));
    return isDebugging;
}

template <class T>
T ProcessDebugger::Read(std::wstring moduleName, DWORD addr)
{
    DWORD moduleAddress = GetModuleAddress(moduleName);
    if (!moduleAddress)
        throw MemoryException("Failed to get module address");

    HANDLE phandle = OpenProcess(PROCESS_VM_READ, 0, processId);
    if (!phandle)
        throw MemoryException("Failed to process handle\n");

    T value;
    if (!ReadProcessMemory(phandle, (void*)(addr + moduleAddress), &value, sizeof(value), 0))
        throw MemoryException("Failed to read memory");

    if (!CloseHandle(phandle))
        throw MemoryException("Failed to close process handle");

    return value;
}

template <class T>
void ProcessDebugger::Write(std::wstring moduleName, DWORD addr, T& value)
{
    DWORD moduleAddress = GetModuleAddress(moduleName);
    if (!moduleAddress)
        throw MemoryException("Failed to get module address");

    HANDLE phandle = OpenProcess(PROCESS_ALL_ACCESS, 0, processId);
    if (!phandle)
        throw MemoryException("Failed to process handle");

    if (!WriteProcessMemory(phandle, (void*)(addr + moduleAddress), &value, sizeof(value), 0))
        throw MemoryException("Failed to write memory");

    if (!CloseHandle(phandle))
        throw MemoryException("Failed to close process handle");
}

template int ProcessDebugger::Read(std::wstring moduleName, DWORD addr);
template void ProcessDebugger::Write(std::wstring moduleName, DWORD addr, int& value);
