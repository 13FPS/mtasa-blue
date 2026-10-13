/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Render native crash context without changing the legacy report ABI
 *****************************************************************************/

#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace NativeContextDiagnostics
{
    // Full-width x64 register lines exceed the old 128-byte emergency buffer.
    // Fixed buffers keep these helpers usable inside the existing SEH readers.
    inline constexpr std::size_t RegisterLineCapacity = 160;

    inline void FormatRegisters(const CONTEXT& context, char* first, std::size_t firstSize, char* second, std::size_t secondSize)
    {
#if defined(_M_X64)
        sprintf_s(first, firstSize, "RAX=0x%016llX RBX=0x%016llX RCX=0x%016llX RDX=0x%016llX", context.Rax, context.Rbx, context.Rcx, context.Rdx);
        sprintf_s(second, secondSize, "RSI=0x%016llX RDI=0x%016llX RBP=0x%016llX RSP=0x%016llX RIP=0x%016llX EFL=0x%08lX", context.Rsi, context.Rdi,
                  context.Rbp, context.Rsp, context.Rip, context.EFlags);
#else
        sprintf_s(first, firstSize, "EAX=0x%08lX EBX=0x%08lX ECX=0x%08lX EDX=0x%08lX", context.Eax, context.Ebx, context.Ecx, context.Edx);
        sprintf_s(second, secondSize, "ESI=0x%08lX EDI=0x%08lX EBP=0x%08lX ESP=0x%08lX EIP=0x%08lX EFL=0x%08lX", context.Esi, context.Edi, context.Ebp,
                  context.Esp, context.Eip, context.EFlags);
#endif
    }

    inline void FormatException(DWORD code, std::uintptr_t address, DWORD thread, char* output, std::size_t size)
    {
#if defined(_M_X64)
        sprintf_s(output, size, "Code=0x%08lX Addr=0x%016llX ThreadId=%lu", code, static_cast<unsigned long long>(address), thread);
#else
        sprintf_s(output, size, "Code=0x%08lX Addr=0x%08lX ThreadId=%lu", code, static_cast<unsigned long>(address), thread);
#endif
    }
}
