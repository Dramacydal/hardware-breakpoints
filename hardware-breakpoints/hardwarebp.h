// Copyright (c) 2000 Mike Morearty <mike@morearty.com>
// Original source and docs: http://www.morearty.com/code/breakpoint

#ifndef _HARDWAREBP_H_
#define _HARDWAREBP_H_

#ifdef _DEBUG

#define MAX_BREAKPOINTS 4

#include <exception>
#include <string>

class BreakPointException : public std::exception
{
    public:
        explicit BreakPointException(std::string message) : std::exception(message.c_str()) { }
};

class HardwareBreakpoint
{
public:
    // The enum values correspond to the values used by the Intel Pentium,
    // so don't change them!
    enum Condition
    {
        Code = 0,
        Write = 1,
        Read = 3,   // or write
    };
    
    HardwareBreakpoint(int _address, int _len, Condition _condition)
        : threadId(0), address(_address), len(_len), condition(_condition) { m_index = -1; }
    ~HardwareBreakpoint() { UnSet(); }

    int GetIndex() const { return m_index; }
    DWORD GetAddress() const { return address; }
    virtual bool HandleException(CONTEXT& ctx) { return false; }

    void Set(DWORD threadId);
    void UnSet();
    bool OnEvent(DEBUG_EVENT& DebugEvent);
    inline void Shift(DWORD offset, bool set = false) { if (set) address = offset; else address += offset; }
    //virtual void HandleException() = 0;

protected:

    inline void SetBits(unsigned long& dw, int lowBit, int bits, int newValue)
    {
        int mask = (1 << bits) - 1; // e.g. 1 becomes 0001, 2 becomes 0011, 3 becomes 0111

        dw = (dw & ~(mask << lowBit)) | (newValue << lowBit);
    }

    DWORD threadId;
    DWORD address;
    int len;
    Condition condition;
    int m_index; // -1 means not set; 0-3 means we've set that hardware bp
};

#endif // _DEBUG

#endif // _HARDWAREBP_H_
