/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        core/StackTraceHelpers.h
 *  PURPOSE:     Crash handler component
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include <windows.h>
#include <DbgHelp.h>

namespace StackTraceHelpers
{
    // Core's native stack walker must use the process architecture rather than
    // the legacy x86 register/report fields, which cannot hold x64 addresses.
#if defined(_M_X64)
    inline constexpr DWORD NativeMachineType = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86)
    inline constexpr DWORD NativeMachineType = IMAGE_FILE_MACHINE_I386;
#else
    #error StackTraceHelpers requires an x86 or x64 Windows target
#endif

    [[nodiscard]] inline DWORD64 GetInstructionPointer(const CONTEXT& context) noexcept
    {
#if defined(_M_X64)
        return context.Rip;
#else
        return context.Eip;
#endif
    }

    [[nodiscard]] inline DWORD64 GetFramePointer(const CONTEXT& context) noexcept
    {
#if defined(_M_X64)
        return context.Rbp;
#else
        return context.Ebp;
#endif
    }

    [[nodiscard]] inline DWORD64 GetStackPointer(const CONTEXT& context) noexcept
    {
#if defined(_M_X64)
        return context.Rsp;
#else
        return context.Esp;
#endif
    }

    [[nodiscard]] inline STACKFRAME64 InitializeStackFrame(const CONTEXT& context) noexcept
    {
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = GetInstructionPointer(context);
        frame.AddrFrame.Offset = GetFramePointer(context);
        frame.AddrStack.Offset = GetStackPointer(context);
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;
        return frame;
    }

    struct StackWalkRoutines
    {
        PREAD_PROCESS_MEMORY_ROUTINE64   readMemory = nullptr;
        PFUNCTION_TABLE_ACCESS_ROUTINE64 functionTableAccess = nullptr;
        PGET_MODULE_BASE_ROUTINE64       moduleBase = nullptr;

        bool operator==(const StackWalkRoutines&) const noexcept = default;
    };

    inline BOOL __stdcall LocalReadProcessMemory(HANDLE /*process*/, DWORD64 baseAddress, PVOID buffer, DWORD size, LPDWORD bytesRead) noexcept
    {
        if (buffer == nullptr || size == 0) [[unlikely]]
        {
            if (bytesRead)
                *bytesRead = 0;
            return FALSE;
        }

        // x64 Core images can live above 4 GiB. Validate against this process's
        // pointer width, including the entire read range, before any cast/copy.
        constexpr DWORD64 maxNativeAddress = static_cast<DWORD64>((std::numeric_limits<std::uintptr_t>::max)());
        if (baseAddress > maxNativeAddress || static_cast<DWORD64>(size) > maxNativeAddress - baseAddress) [[unlikely]]
        {
            if (bytesRead)
                *bytesRead = 0;
            return FALSE;
        }

        __try
        {
            const auto srcAddr = static_cast<std::uintptr_t>(baseAddress);
            const auto srcPtr = reinterpret_cast<const std::byte*>(srcAddr);
            const auto dest = std::span{reinterpret_cast<std::byte*>(buffer), size};
            const auto srcSpan = std::span{srcPtr, size};
            std::ranges::copy(srcSpan, dest.begin());
            if (bytesRead)
                *bytesRead = size;
            return TRUE;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (bytesRead)
                *bytesRead = 0;
            return FALSE;
        }
    }

    inline DWORD64 __stdcall LocalGetModuleBase(HANDLE /*process*/, DWORD64 address) noexcept
    {
        constexpr DWORD64 maxNativeAddress = static_cast<DWORD64>((std::numeric_limits<std::uintptr_t>::max)());
        if (address > maxNativeAddress) [[unlikely]]
            return 0;

        MEMORY_BASIC_INFORMATION mbi{};
        const auto               nativeAddress = static_cast<std::uintptr_t>(address);
        if (VirtualQuery(reinterpret_cast<LPCVOID>(nativeAddress), &mbi, sizeof(mbi)) == 0) [[unlikely]]
            return 0;

        if (mbi.AllocationBase == nullptr) [[unlikely]]
            return 0;

        return static_cast<DWORD64>(reinterpret_cast<std::uintptr_t>(mbi.AllocationBase));
    }

#if defined(_M_X64)
    inline PVOID __stdcall LocalFunctionTableAccess(HANDLE /*process*/, DWORD64 address) noexcept
    {
        // x64 unwinding needs image unwind metadata even when no PDB/symbol
        // handler is available. Query only this process, like the local read
        // and module-base callbacks used by the same fallback walker.
        DWORD64 imageBase = 0;
        return RtlLookupFunctionEntry(address, &imageBase, nullptr);
    }
#endif

    [[nodiscard]] inline StackWalkRoutines MakeStackWalkRoutines(bool useDbgHelp) noexcept
    {
        if (useDbgHelp)
        {
            return StackWalkRoutines{.readMemory = &LocalReadProcessMemory, .functionTableAccess = SymFunctionTableAccess64, .moduleBase = SymGetModuleBase64};
        }

        return StackWalkRoutines{.readMemory = &LocalReadProcessMemory,
#if defined(_M_X64)
                                 .functionTableAccess = &LocalFunctionTableAccess,
#else
                                 .functionTableAccess = nullptr,
#endif
                                 .moduleBase = &LocalGetModuleBase};
    }

    struct ModuleAddressInfo
    {
        std::string name;
        DWORD64     base;

        bool operator==(const ModuleAddressInfo&) const noexcept = default;
    };

    [[nodiscard]] inline std::optional<ModuleAddressInfo> DescribeModule(DWORD64 address)
    {
        if (address == 0) [[unlikely]]
            return std::nullopt;

        constexpr DWORD64 maxNativeAddress = static_cast<DWORD64>((std::numeric_limits<std::uintptr_t>::max)());
        if (address > maxNativeAddress) [[unlikely]]
            return std::nullopt;

        MEMORY_BASIC_INFORMATION mbi{};
        if (const auto nativeAddress = static_cast<std::uintptr_t>(address); VirtualQuery(reinterpret_cast<LPCVOID>(nativeAddress), &mbi, sizeof(mbi)) == 0)
            [[unlikely]]
            return std::nullopt;

        if (mbi.AllocationBase == nullptr) [[unlikely]]
            return std::nullopt;

        std::array<char, MAX_PATH> modulePath{};
        const DWORD pathLen = GetModuleFileNameA(reinterpret_cast<HMODULE>(mbi.AllocationBase), modulePath.data(), static_cast<DWORD>(modulePath.size()));

        if (pathLen == 0) [[unlikely]]
            return std::nullopt;

        if (pathLen >= modulePath.size()) [[unlikely]]
        {
            modulePath.back() = '\0';
            return std::nullopt;
        }

        const std::string_view modulePathView{modulePath.data(), pathLen};
        const auto             lastSlash = modulePathView.find_last_of("\\/");
        const std::string_view moduleName = (lastSlash != std::string_view::npos) ? modulePathView.substr(lastSlash + 1) : modulePathView;

        ModuleAddressInfo info{.name = std::string{moduleName}, .base = static_cast<DWORD64>(reinterpret_cast<std::uintptr_t>(mbi.AllocationBase))};
        return info;
    }

    [[nodiscard]] inline std::string FormatAddressWithModule(DWORD64 address)
    {
        try
        {
            if (const auto moduleInfo = DescribeModule(address)) [[likely]]
            {
                const auto& [name, base] = *moduleInfo;

                if (address < base) [[unlikely]]
                    return std::format("0x{:X}", address);

                const DWORD64 offset = address - base;
                return std::format("{}+0x{:X}", name, offset);
            }

            return std::format("0x{:X}", address);
        }
        catch (const std::format_error&)
        {
            return "<format_error>";
        }
    }

    [[nodiscard]] inline std::string FormatAddressWithModuleAndAbsolute(DWORD64 address)
    {
        try
        {
            return std::format("{} [0x{:X}]", FormatAddressWithModule(address), address);
        }
        catch (const std::format_error&)
        {
            return "<format_error>";
        }
    }
}  // namespace StackTraceHelpers
