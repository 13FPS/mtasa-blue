/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Checked x64 native memory operations for the borrowed weather owner
 *****************************************************************************/
#pragma once
#include "NativeWeatherOverrideOwner.h"
#include "NativeWeatherOverrideSlot.h"

namespace mta::game_de
{
    class NativeWeatherOverrideWin32 final : public NativeWeatherOverridePlatform
    {
    public:
        CodeRead    Reader() const noexcept override;
        bool        Revalidate(const Binding&, const NativeWeatherOverrideCodeView&) noexcept override;
        bool        CompareExchange128() const noexcept override;
        ClockResult AllocateNear(const Binding&, Field, const NativeWeatherOverrideProfile::Range&, std::uintptr_t&) noexcept override;
        bool        ScratchValid(std::uintptr_t) const noexcept override;
        bool        ReleaseScratch(std::uintptr_t) noexcept override;
        bool        OpenCode(const Binding&, std::uintptr_t, std::size_t, std::uint32_t&) noexcept override;
        bool        CloseCode(const Binding&, std::uintptr_t, std::size_t, std::uint32_t) noexcept override;
        Exchange    ExchangeCode(const Binding&, std::uintptr_t, std::size_t, const Bytes&, const Bytes&) noexcept override;
        bool        Flush(std::uintptr_t, std::size_t) noexcept override;
        bool        ReadScalar(const Binding&, Field, std::uint32_t&) noexcept override;
        Exchange    ExchangeScalar(const Binding&, Field, std::uint32_t, std::uint32_t) noexcept override;
        bool        SupportsRain() const noexcept override { return true; }
        ClockResult QualifyRain(const Binding&, const NativeWeatherOverrideCodeView&) noexcept override;
        ClockResult PrepareRain(const Binding&, const NativeWeatherOverrideCodeView&, NativeWeatherRainLeaf::State&) noexcept override;
        bool        ReadRain(NativeWeatherRainLeaf::State&, std::uint64_t&, std::uint64_t&) noexcept override;
        bool        RevalidateRain(const Binding&, const NativeWeatherOverrideCodeView&, const NativeWeatherRainLeaf::State&, bool) noexcept override;
        bool        ReleaseRain(const NativeWeatherRainLeaf::State&, NativeWeatherRainLeaf::ReleaseCheck, void*) noexcept override;

    private:
        // Interior accumulators are borrowed by code; only these retained slot
        // objects know the independent VirtualAlloc base that may be released.
        std::array<NativeWeatherOverrideSlot, 8> slots{};
        NativeWeatherRainLeaf                    rainLeaf;
        std::uintptr_t                           rainImage = 0, rainCopyTarget = 0;
    };
}
