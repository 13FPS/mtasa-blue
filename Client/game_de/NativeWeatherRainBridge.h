/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Direction-correct native Rain serialization without a heap borrow
 *****************************************************************************/
#pragma once
#include "NativeWeatherRainProfile.h"
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativeWeatherRainBridge
{
    inline constexpr auto        EntryRva = NativeWeatherRainProfile::EntryRva;
    inline constexpr auto        ReturnRva = NativeWeatherRainProfile::ReturnRva;
    inline constexpr auto        ContinuationRva = NativeWeatherRainProfile::ContinuationRva;
    inline constexpr auto        NativeRainRva = NativeWeatherRainProfile::NativeRainRva;
    inline constexpr auto        SaveVtableRva = NativeWeatherRainProfile::SaveVtableRva, LoadVtableRva = NativeWeatherRainProfile::LoadVtableRva;
    inline constexpr auto        SaveSerializeRva = NativeWeatherRainProfile::SaveSerializeRva, LoadSerializeRva = NativeWeatherRainProfile::LoadSerializeRva;
    inline constexpr std::size_t ArchiveFlagsOffset = 0x28;
    inline constexpr std::size_t SerializeSlotOffset = 0x150;
    inline constexpr std::array<unsigned char, 8> StockEntry = {0xf6, 0x47, 0x29, 0x20, 0x75, 0x09, 0x48, 0x8d};

    struct Context final
    {
        // This context is immutable after code publication except for the
        // aligned activity count. The retained code owner, not a callback
        // pointer held by a native archive, owns its lifetime.
        std::uintptr_t  nativeRain = 0;
        std::uintptr_t  saveVtable = 0, loadVtable = 0, saveSerialize = 0, loadSerialize = 0;
        volatile LONG64 activeCalls = 0;
        // A substituted/unknown archive must never receive our load-local
        // pointer on the strength of another class's synchronous-copy proof.
        // Preserve its original operation and latch unsupported ownership.
        volatile LONG64 unqualifiedArchive = 0;
    };
    static_assert(offsetof(Context, activeCalls) % 8 == 0 && offsetof(Context, unqualifiedArchive) % 8 == 0);
    using Serialize = void (*)(void* archive, void* bytes, std::uint64_t length);

    struct Plan final
    {
        std::array<unsigned char, 8>  entry{};
        std::array<unsigned char, 25> bridge{};
    };

    // This builds bytes only. It does not qualify the image, create memory,
    // publish a jump, or prove absence of old serializer/callee borrows.
    // Caller output is preserved when any relative/identity constraint fails.
    bool Build(std::uintptr_t image, std::uintptr_t bridge, const Context*, Plan&) noexcept;

    // This is a normal compiled Win64 callee. The tiny leaf bridge tail-jumps
    // here after the native CALL has supplied a real return address. Its own
    // compiler-emitted unwind/SEH metadata therefore describes the actual
    // stack frame; no alias of the original serializer's unwind data is used.
    // Never expose this body as an independently authorized native operation.
    void Body(Context*, void* archive);
}
