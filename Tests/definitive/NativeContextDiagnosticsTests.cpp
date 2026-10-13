/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native diagnostic address bits and existing x86 text
 *****************************************************************************/

#include "../../Client/core/NativeContextDiagnostics.h"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

int main()
{
    int        checks = 0;
    const auto check = [&](bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    };
    try
    {
        CONTEXT context{};
#if defined(_M_X64)
        context.Rax = 0x1234567887654321ULL;
        context.Rbx = 0x2345678998765432ULL;
        context.Rcx = 0x3456789aa9876543ULL;
        context.Rdx = 0x456789abba987654ULL;
        context.Rsi = 0x56789abccbba9876ULL;
        context.Rdi = 0x6789abcddccbba98ULL;
        context.Rbp = 0x789abcdeeddccbbaULL;
        context.Rsp = 0x89abcdeffeedccbbULL;
        context.Rip = 0xffffffffffffffffULL;
        constexpr auto expectedFirst = "RAX=0x1234567887654321 RBX=0x2345678998765432 RCX=0x3456789AA9876543 RDX=0x456789ABBA987654";
        constexpr auto expectedSecond =
            "RSI=0x56789ABCCBBA9876 RDI=0x6789ABCDDCCBBA98 RBP=0x789ABCDEEDDCCBBA RSP=0x89ABCDEFFEEDCCBB RIP=0xFFFFFFFFFFFFFFFF EFL=0x89ABCDEF";
        constexpr auto           expectedException = "Code=0xC0000005 Addr=0x1234567887654321 ThreadId=42";
        constexpr std::uintptr_t exceptionAddress = 0x1234567887654321ULL;
#else
        context.Eax = 0x87654321;
        context.Ebx = 0x98765432;
        context.Ecx = 0xa9876543;
        context.Edx = 0xba987654;
        context.Esi = 0xcbba9876;
        context.Edi = 0xdccbba98;
        context.Ebp = 0xeddccbba;
        context.Esp = 0xfeedccbb;
        context.Eip = 0xffffffff;
        // These literal lines are the pre-port EmergencyCrashLogging format,
        // independently fixing its names, padding, ordering and punctuation.
        constexpr auto           expectedFirst = "EAX=0x87654321 EBX=0x98765432 ECX=0xA9876543 EDX=0xBA987654";
        constexpr auto           expectedSecond = "ESI=0xCBBA9876 EDI=0xDCCBBA98 EBP=0xEDDCCBBA ESP=0xFEEDCCBB EIP=0xFFFFFFFF EFL=0x89ABCDEF";
        constexpr auto           expectedException = "Code=0xC0000005 Addr=0x87654321 ThreadId=42";
        constexpr std::uintptr_t exceptionAddress = 0x87654321;
#endif
        context.EFlags = 0x89abcdef;
        struct GuardedLine
        {
            std::array<char, NativeContextDiagnostics::RegisterLineCapacity> text{};
            std::array<unsigned char, 8>                                     guard{1, 2, 3, 4, 5, 6, 7, 8};
        };
        GuardedLine first, second;
        NativeContextDiagnostics::FormatRegisters(context, first.text.data(), first.text.size(), second.text.data(), second.text.size());
        check(std::string(first.text.data()) == expectedFirst, "native general register text must retain all bits and original ordering");
        check(std::string(second.text.data()) == expectedSecond, "native stack/instruction text must retain all bits and flags");
        check(first.guard == std::array<unsigned char, 8>{1, 2, 3, 4, 5, 6, 7, 8} && second.guard == first.guard,
              "register lines stay within fixed crash buffers");
        check(std::strlen(first.text.data()) < first.text.size() && std::strlen(second.text.data()) < second.text.size(),
              "register lines fit without truncation");
#if defined(_M_X64)
        check(std::strlen(second.text.data()) >= 128, "x64 fixture exercises the old emergency buffer overflow limit");
#endif
        std::array<char, 128> exception{};
        NativeContextDiagnostics::FormatException(0xc0000005, exceptionAddress, 42, exception.data(), exception.size());
        check(std::string(exception.data()) == expectedException, "exception address uses native width without changing x86 output");
        NativeContextDiagnostics::FormatRegisters(CONTEXT{}, first.text.data(), first.text.size(), second.text.data(), second.text.size());
#if defined(_M_X64)
        check(std::string(first.text.data()) == "RAX=0x0000000000000000 RBX=0x0000000000000000 RCX=0x0000000000000000 RDX=0x0000000000000000",
              "native zero values keep full-width padding");
        check(std::string(second.text.data()).find("RIP=0x0000000000000000") != std::string::npos, "actual zero RIP is represented directly");
#else
        check(std::string(first.text.data()) == "EAX=0x00000000 EBX=0x00000000 ECX=0x00000000 EDX=0x00000000", "classic zero values retain old padding");
        check(std::string(second.text.data()).find("EIP=0x00000000") != std::string::npos, "classic zero EIP is represented directly");
#endif
        std::cout << "Passed " << checks << " native context diagnostic checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
