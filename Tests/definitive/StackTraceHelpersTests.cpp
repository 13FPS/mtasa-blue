/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check Core crash helpers at actual native-width process addresses
 *****************************************************************************/

#include "../../Client/core/StackTraceHelpers.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    int           checks = 0;
    std::uint64_t moduleMarker = 0x1122334455667788ULL;

    void Check(bool condition, const char* reason)
    {
        ++checks;
        // Real x64 images normally live beyond 4 GiB; optimized tests must
        // detect a helper that silently discards their addresses.
        if (!condition)
            throw std::runtime_error(reason);
    }

    DWORD64 Address(const void* pointer)
    {
        return static_cast<DWORD64>(reinterpret_cast<std::uintptr_t>(pointer));
    }

    struct Page
    {
        void* value = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        ~Page()
        {
            if (value)
                VirtualFree(value, 0, MEM_RELEASE);
        }
    };

    void TestOwnModule()
    {
        const auto marker = Address(&moduleMarker);
        const auto module = Address(GetModuleHandleW(nullptr));
#if defined(_WIN64)
        Check(marker > UINT32_MAX && module > UINT32_MAX, "x64 fixture must exercise real image addresses above 4 GiB");
#else
        Check(marker <= UINT32_MAX && module <= UINT32_MAX, "x86 fixture retains native-width image addresses");
#endif
        Check(StackTraceHelpers::LocalGetModuleBase(GetCurrentProcess(), marker) == module, "own-module resolution must preserve the full image base");
        const auto description = StackTraceHelpers::DescribeModule(marker);
        Check(description.has_value(), "own image address must resolve to a module description");
        Check(description->base == module && !description->name.empty(), "description retains full base and a basename");
        Check(description->name.find_first_of("/\\") == std::string::npos, "description is an ordinary module basename");
        const auto expected = description->name + std::format("+0x{:X}", marker - module);
        Check(StackTraceHelpers::FormatAddressWithModule(marker) == expected, "module-plus-offset formatting must not truncate high addresses");
        Check(StackTraceHelpers::FormatAddressWithModuleAndAbsolute(marker) == expected + std::format(" [0x{:X}]", marker),
              "absolute formatting retains all native address bits");
        std::uint64_t copied = 0;
        DWORD         read = 999;
        Check(StackTraceHelpers::LocalReadProcessMemory(GetCurrentProcess(), marker, &copied, sizeof(copied), &read) == TRUE,
              "stack callback must read this process's actual image data");
        Check(copied == moduleMarker && read == sizeof(copied), "full-width source bytes are copied exactly");
        copied = 0;
        Check(StackTraceHelpers::LocalReadProcessMemory(nullptr, marker, &copied, sizeof(copied), nullptr) == TRUE && copied == moduleMarker,
              "local callback retains optional handle/count behavior");
    }

    void TestOwnMemoryAndFailureBounds()
    {
        Page page;
        Check(page.value != nullptr, "allocate only this test process's memory");
        std::array<std::byte, 32> expected{};
        for (std::size_t index = 0; index < expected.size(); ++index)
            expected[index] = static_cast<std::byte>(index + 1);
        std::memcpy(page.value, expected.data(), expected.size());
        std::array<std::byte, 32> copied{};
        DWORD                     read = 0;
        Check(StackTraceHelpers::LocalReadProcessMemory(nullptr, Address(page.value), copied.data(), static_cast<DWORD>(copied.size()), &read) == TRUE,
              "read ordinary committed local memory");
        Check(copied == expected && read == copied.size(), "local memory bytes/count are exact");
        const auto sentinel = std::uint64_t{0xabcdef0123456789ULL};
        const auto reject = [&](DWORD64 source, DWORD size)
        {
            auto  output = sentinel;
            DWORD bytesRead = 999;
            Check(StackTraceHelpers::LocalReadProcessMemory(nullptr, source, &output, size, &bytesRead) == FALSE, "invalid address/range must reject");
            Check(bytesRead == 0 && output == sentinel, "pre-copy rejection clears bytes-read and preserves the destination");
        };
        const auto maximum = static_cast<DWORD64>((std::numeric_limits<std::uintptr_t>::max)());
        reject(maximum - 1, 4);
        reject(maximum, 1);
        reject(0, sizeof(sentinel));
        reject(Address(&moduleMarker), 0);
#if !defined(_WIN64)
        reject(maximum + 1, sizeof(sentinel));
        Check(StackTraceHelpers::LocalGetModuleBase(nullptr, maximum + 1) == 0, "x86 rejects a module address beyond native width");
        Check(!StackTraceHelpers::DescribeModule(maximum + 1), "x86 descriptions reject wider addresses");
        Check(StackTraceHelpers::FormatAddressWithModule(maximum + 1) == "0x100000000", "rejected foreign-width address still formats without truncation");
#endif
        DWORD protection = 0;
        Check(VirtualProtect(page.value, 4096, PAGE_NOACCESS, &protection) != FALSE, "protect only the owned test page");
        reject(Address(page.value), sizeof(sentinel));
        DWORD bytesRead = 999;
        Check(StackTraceHelpers::LocalReadProcessMemory(nullptr, Address(&moduleMarker), nullptr, sizeof(moduleMarker), &bytesRead) == FALSE && bytesRead == 0,
              "null destination rejects before copying");
        Check(
            StackTraceHelpers::LocalReadProcessMemory(nullptr, Address(&moduleMarker), page.value, sizeof(moduleMarker), &bytesRead) == FALSE && bytesRead == 0,
            "SEH handles an inaccessible local destination");
        Check(StackTraceHelpers::LocalGetModuleBase(nullptr, 0) == 0 && !StackTraceHelpers::DescribeModule(0), "null address has no module");
        Check(StackTraceHelpers::FormatAddressWithModule(0) == "0x0" && StackTraceHelpers::FormatAddressWithModuleAndAbsolute(0) == "0x0 [0x0]",
              "ordinary fallback formatting is unchanged");
    }

    void TestCallbacks()
    {
        const auto fallback = StackTraceHelpers::MakeStackWalkRoutines(false);
        Check(fallback.readMemory == &StackTraceHelpers::LocalReadProcessMemory &&
#if defined(_M_X64)
                  fallback.functionTableAccess == &StackTraceHelpers::LocalFunctionTableAccess &&
#else
                  fallback.functionTableAccess == nullptr &&
#endif
                  fallback.moduleBase == &StackTraceHelpers::LocalGetModuleBase,
              "fallback callbacks remain the actual Core helpers");
        const auto symbols = StackTraceHelpers::MakeStackWalkRoutines(true);
        Check(symbols.readMemory == &StackTraceHelpers::LocalReadProcessMemory && symbols.functionTableAccess == &SymFunctionTableAccess64 &&
                  symbols.moduleBase == &SymGetModuleBase64,
              "symbol callbacks retain existing DbgHelp API contracts");
    }

    void TestNativeFrameInitialization()
    {
        CONTEXT context{};
#if defined(_M_X64)
        context.Rip = 0x123456789abcdef0ULL;
        context.Rbp = 0x23456789abcdef01ULL;
        context.Rsp = 0x3456789abcdef012ULL;
        constexpr DWORD64 instruction = 0x123456789abcdef0ULL;
        constexpr DWORD64 frameAddress = 0x23456789abcdef01ULL;
        constexpr DWORD64 stackAddress = 0x3456789abcdef012ULL;
        Check(StackTraceHelpers::NativeMachineType == IMAGE_FILE_MACHINE_AMD64, "x64 stack walks must select AMD64");
#else
        context.Eip = 0x89abcdef;
        context.Ebp = 0x9abcdef0;
        context.Esp = 0xabcdef01;
        constexpr DWORD64 instruction = 0x89abcdef;
        constexpr DWORD64 frameAddress = 0x9abcdef0;
        constexpr DWORD64 stackAddress = 0xabcdef01;
        Check(StackTraceHelpers::NativeMachineType == IMAGE_FILE_MACHINE_I386, "x86 retains the I386 stack walker");
#endif
        Check(StackTraceHelpers::GetInstructionPointer(context) == instruction, "instruction address retains every native register bit");
        Check(StackTraceHelpers::GetFramePointer(context) == frameAddress, "frame address retains every native register bit");
        Check(StackTraceHelpers::GetStackPointer(context) == stackAddress, "stack address retains every native register bit");
        const auto frame = StackTraceHelpers::InitializeStackFrame(context);
        Check(frame.AddrPC.Offset == instruction && frame.AddrFrame.Offset == frameAddress && frame.AddrStack.Offset == stackAddress,
              "all three stack frame addresses use the native context");
        Check(frame.AddrPC.Mode == AddrModeFlat && frame.AddrFrame.Mode == AddrModeFlat && frame.AddrStack.Mode == AddrModeFlat,
              "native frame keeps the original flat address modes");
        Check(frame.AddrReturn.Offset == 0 && frame.FuncTableEntry == nullptr && frame.Far == FALSE && frame.Virtual == FALSE,
              "frame initialization retains zeroed walker state");
        const auto empty = StackTraceHelpers::InitializeStackFrame(CONTEXT{});
        Check(empty.AddrPC.Offset == 0 && empty.AddrFrame.Offset == 0 && empty.AddrStack.Offset == 0, "null context addresses remain null");
    }

    // Real optimized x64 callers require unwind metadata even without symbols.
    // Walk only our current thread; this fixture never attaches to a game.
    __declspec(noinline) std::size_t CaptureFrames(std::array<DWORD64, 16>& addresses)
    {
        CONTEXT context{};
        RtlCaptureContext(&context);
        auto       frame = StackTraceHelpers::InitializeStackFrame(context);
        const auto routines = StackTraceHelpers::MakeStackWalkRoutines(false);
#if defined(_M_X64)
        Check(StackTraceHelpers::LocalFunctionTableAccess(nullptr, frame.AddrPC.Offset) != nullptr,
              "current non-leaf x64 frame has real runtime unwind metadata");
        Check(StackTraceHelpers::LocalFunctionTableAccess(nullptr, 0) == nullptr, "null address has no runtime function entry");
        Check(StackTraceHelpers::LocalFunctionTableAccess(nullptr, UINT64_MAX) == nullptr, "invalid address has no runtime function entry");
#endif
        std::size_t count = 0;
        while (count < addresses.size() && StackWalk64(StackTraceHelpers::NativeMachineType, GetCurrentProcess(), GetCurrentThread(), &frame, &context,
                                                       routines.readMemory, routines.functionTableAccess, routines.moduleBase, nullptr) != FALSE)
        {
            if (frame.AddrPC.Offset == 0 || (count != 0 && addresses[count - 1] == frame.AddrPC.Offset))
                break;
            addresses[count++] = frame.AddrPC.Offset;
        }
        return count;
    }

    __declspec(noinline) std::size_t CaptureChild(std::array<DWORD64, 16>& addresses)
    {
        const auto    count = CaptureFrames(addresses);
        volatile auto keepCallerFrame = count;
        return keepCallerFrame;
    }

    __declspec(noinline) std::size_t CaptureParent(std::array<DWORD64, 16>& addresses)
    {
        const auto    count = CaptureChild(addresses);
        volatile auto keepCallerFrame = count;
        return keepCallerFrame;
    }

    void TestCurrentThreadWalk()
    {
        std::array<DWORD64, 16> addresses{};
        const auto              count = CaptureParent(addresses);
        Check(count >= 3, "native walker traverses real nested current-thread frames without symbol initialization");
        const auto module = Address(GetModuleHandleW(nullptr));
        for (std::size_t index = 0; index < 3; ++index)
        {
            Check(StackTraceHelpers::LocalGetModuleBase(nullptr, addresses[index]) == module, "nested frame belongs to this actual test image");
#if defined(_M_X64)
            Check(addresses[index] > UINT32_MAX, "real x64 frame address is not truncated to the legacy register width");
#endif
        }
    }
}

int main()
{
    try
    {
        TestOwnModule();
        TestOwnMemoryAndFailureBounds();
        TestCallbacks();
        TestNativeFrameInitialization();
        TestCurrentThreadWalk();
        std::cout << "Passed " << checks << " Core stack trace helper checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Core stack trace helpers failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
