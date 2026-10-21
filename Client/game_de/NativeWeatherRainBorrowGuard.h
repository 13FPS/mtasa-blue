/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Refuse Rain publication/retirement across a live serializer borrow
 *****************************************************************************/
#pragma once
#include "NativeWeatherRainProfile.h"
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativeWeatherRainBorrow
{
    enum class Result : unsigned char
    {
        Clear,
        Borrowed,
        Unsupported,
        ReadFailure,
        Changed
    };
    struct Range final
    {
        std::uintptr_t begin = 0, end = 0;
        bool           Contains(std::uintptr_t value) const noexcept { return value >= begin && value < end; }
    };
    struct Exclusions final
    {
        std::uintptr_t image = 0;
        Range          leaf{}, bodyModule{};
    };
    struct Stack final
    {
        std::uintptr_t limit = 0, base = 0, rsp = 0, rip = 0;
        bool           current = false;
    };
    using ReadMemory = bool (*)(std::uintptr_t, void*, std::size_t) noexcept;
    using CheckPages = bool (*)(std::uintptr_t, std::size_t) noexcept;
    inline constexpr std::size_t MaximumActiveBytes = 8 * 1024 * 1024;
    using Buffer = std::array<std::uintptr_t, 512>;

    // This examines an independently qualified active stack, not an arbitrary
    // private-memory extent. Its result alone grants no thread or patch scope.
    // A full host must park every prepared peer and also inspect its own stack.
    Result Inspect(const Stack&, const Exclusions&, ReadMemory, CheckPages, Buffer&) noexcept;

    class Guard final
    {
    public:
        struct Peer final
        {
            HANDLE         handle = nullptr;  // Borrowed; the existing peer holder owns it.
            DWORD          threadId = 0;
            std::uintptr_t teb = 0;
        };
        // Resolve/calibrate APIs and prepare each peer BEFORE parking. These
        // operations deliberately never suspend a thread or acquire an owner.
        bool Prepare() noexcept;
        bool PreparePeer(HANDLE, Peer&) const noexcept;
        // The caller must already own the complete parked-thread set. Bounds
        // come from its prepared TEB, reread after suspension; VirtualQuery
        // only validates pages inside those bounds and never invents a top.
        Result InspectParked(const Peer&, const Exclusions&) noexcept;
        Result InspectCurrent(const Exclusions&) noexcept;

    private:
        using QueryThread = LONG(NTAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
        using StackLimits = void(WINAPI*)(ULONG_PTR*, ULONG_PTR*);
        using IsFiber = BOOL(WINAPI*)();
        QueryThread    queryThread = nullptr;
        StackLimits    stackLimits = nullptr;
        IsFiber        isFiber = nullptr;
        std::uintptr_t nonFiberToken = 0;
        DWORD          processId = 0;
        bool           prepared = false;
        Buffer         buffer{};
        bool           QueryTeb(HANDLE, DWORD, std::uintptr_t&) const noexcept;
        bool           ReadTib(std::uintptr_t, NT_TIB&) const noexcept;
    };
}
