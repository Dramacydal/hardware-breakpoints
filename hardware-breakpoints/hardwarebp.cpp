// Copyright (c) 2000 Mike Morearty <mike@morearty.com>
// Original source and docs: http://www.morearty.com/code/breakpoint

#include <windows.h>
#include "hardwarebp.h"

#ifdef _DEBUG

void HardwareBreakpoint::Set(DWORD _threadId)
{
    // make sure this breakpoint isn't already set
    if (m_index != -1)
        throw BreakPointException("Breakpoint is already set!");

    threadId = _threadId;

    CONTEXT cxt;

    switch (len)
    {
        case 1: len = 0; break;
        case 2: len = 1; break;
        case 4: len = 3; break;
        case 8: len = 2; break;
        default: throw BreakPointException("Invalid length!");
    }

    // The only registers we care about are the debug registers
    cxt.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    HANDLE handle = OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);

    if (!SuspendThread(handle))
        throw BreakPointException("Failed to suspend thread");

    // Read the register values
    if (!GetThreadContext(handle, &cxt))
        throw BreakPointException("Failed to get thread context");

    // Find an available hardware register
    for (m_index = 0; m_index < MAX_BREAKPOINTS; ++m_index)
    {
        if ((cxt.Dr7 & (1 << (m_index*2))) == 0)
            break;
    }

    if (m_index == MAX_BREAKPOINTS)
        throw BreakPointException("All hardware breakpoint registers are already being used");

    switch (m_index)
    {
        case 0: cxt.Dr0 = (DWORD) address; break;
        case 1: cxt.Dr1 = (DWORD) address; break;
        case 2: cxt.Dr2 = (DWORD) address; break;
        case 3: cxt.Dr3 = (DWORD) address; break;
        default: throw BreakPointException("m_index has bogus value!");
    }

    SetBits(cxt.Dr7, 16 + (m_index*4), 2, condition);
    SetBits(cxt.Dr7, 18 + (m_index*4), 2, len);
    SetBits(cxt.Dr7, m_index*2,        1, 1);

    // Write out the new debug registers
    if (!SetThreadContext(handle, &cxt))
        throw BreakPointException("Failed to set thread context");

    if (!ResumeThread(handle))
        throw BreakPointException("Failed to resume thread");
}

void HardwareBreakpoint::UnSet()
{
    if (m_index == -1 || !threadId)
        return;

    CONTEXT cxt;

    // The only registers we care about are the debug registers
    cxt.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    HANDLE handle = OpenThread(THREAD_ALL_ACCESS, FALSE, threadId);
    if (!SuspendThread(handle))
        throw BreakPointException("Failed to suspend thread");

    // Read the register values
    if (!GetThreadContext(handle, &cxt))
        throw BreakPointException("Failed to get thread context");

    // Zero out the debug register settings for this breakpoint
    if (m_index < 0 || m_index >= MAX_BREAKPOINTS)
        throw BreakPointException("Failed to set suspend thread");

    SetBits(cxt.Dr7, m_index*2, 1, 0);

    // Write out the new debug registers
    if (!SetThreadContext(handle, &cxt))
        throw BreakPointException("Failed to set thread context");

    if (!ResumeThread(handle))
        throw BreakPointException("Failed to resume thread");

    m_index = -1;
}

bool HardwareBreakpoint::OnEvent(DEBUG_EVENT& DebugEvent)
{
    if (DebugEvent.dwThreadId != threadId)
        return false;

    //std::cout << "Debug event occured! Process Id: " << DebugEvent.dwProcessId << " Thread Id: " << DebugEvent.dwThreadId << " Addr: " << std::hex << DebugEvent.u.Exception.ExceptionRecord.ExceptionAddress << std::dec << std::endl;
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, false, DebugEvent.dwThreadId);
    if (!hThread)
        throw BreakPointException("Failed to open thread");

    //assert(hThread);

    CONTEXT Context;
    Context.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &Context))
        throw BreakPointException("Failed to get thread context");

    if (Context.Eip != GetAddress())
        return false;

    if (HandleException(Context))
        if (!SetThreadContext(hThread, &Context))
            throw BreakPointException("Failed to set thread context");

    //std::cout << std::hex << Context.Eip << std::dec << std::endl;
    //Context.Eip += 6;

    return true;
}

#endif // _DEBUG
