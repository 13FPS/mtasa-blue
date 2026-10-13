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
        Check(fallback.readMemory == &StackTraceHelpers::LocalReadProcessMemory && fallback.functionTableAccess == nullptr &&
                  fallback.moduleBase == &StackTraceHelpers::LocalGetModuleBase,
              "fallback callbacks remain the actual Core helpers");
        const auto symbols = StackTraceHelpers::MakeStackWalkRoutines(true);
        Check(symbols.readMemory == &StackTraceHelpers::LocalReadProcessMemory && symbols.functionTableAccess == &SymFunctionTableAccess64 &&
                  symbols.moduleBase == &SymGetModuleBase64,
              "symbol callbacks retain existing DbgHelp API contracts");
    }
}

int main()
{
    try
    {
        TestOwnModule();
        TestOwnMemoryAndFailureBounds();
        TestCallbacks();
        std::cout << "Passed " << checks << " Core stack trace helper checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Core stack trace helpers failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
