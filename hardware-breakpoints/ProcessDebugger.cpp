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
    isDebugging = DebugActiveProcess(processId);
    if (!isDebugging)
        return false;

    if (!DebugSetProcessKillOnExit(FALSE))
        return false;

    threadId = GetProcessThreadID(processId);
    if (!threadId)
        return false;

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

bool ProcessDebugger::StartListener(DWORD time)
{
    DEBUG_EVENT DebugEvent;
    for (int i = 0; isDebugging; ++i)
    {
        if (!WaitForDebugEvent(&DebugEvent, INFINITE))
        {
            if (!isDebugging)
                break;
            continue;
        }

        bool okEvent = false;
        switch (DebugEvent.dwDebugEventCode)
        {
            case EXCEPTION_DEBUG_EVENT:
                // exception occured!
                if(DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP)
                {
                    for (auto bp : breakPoints)
                    {
                        if (bp->OnEvent(DebugEvent, this))
                        {
                            okEvent = true;
                            break;
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
            RemoveBreakPoints();
            ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, okEvent ? DBG_CONTINUE : DBG_EXCEPTION_NOT_HANDLED);
            DebugActiveProcessStop(processId);
            return true;
        }

        ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, okEvent ? DBG_CONTINUE : DBG_EXCEPTION_NOT_HANDLED);
    }

    return Detach();
}

void ProcessDebugger::RunnerThread(ProcessDebugger* pd)
{
    if (!pd->FindAndAttach())
    {
        //std::cout << "Can't find process" << std::endl;
        return;
    }

    try
    {
        if (!pd->StartListener())
        {
            //std::cout << "Fail while waiting for debug events" << std::endl;
            return;
        }
    }
    catch (MemoryException& e)
    {
        //printf("Memory Exception: %s\n", e.what());
    }

    try
    {
        pd->Detach();
    }
    catch (std::exception& e)
    {
        //printf("1: %s\n", e.what());
    }
}

std::thread* ProcessDebugger::Run(ProcessDebugger* pd)
{
    return new std::thread(RunnerThread, pd);
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
