/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only classification of a pinned native custom presenter
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderObservation.h"
#include <array>
#include <cstddef>

namespace mta::game_de
{
    inline constexpr std::uintptr_t CustomPresentTableRva = 0x42296f0;
    struct CustomPresentSignature
    {
        std::uintptr_t                functionRva;
        std::size_t                   bytes;
        std::array<unsigned char, 32> sha256;
    };
    const std::array<CustomPresentSignature, 7>& CustomPresentSignatures() noexcept;

    // The real reader requires image-owned table/code pages. An owned fixture
    // can exercise the same classifier and fault handling without loading or
    // invoking the game image. Neither reader ever calls a presenter method.
    class CustomPresentReads
    {
    public:
        virtual bool Object(std::uintptr_t address, void* output, std::size_t bytes) = 0;
        virtual bool Image(std::uintptr_t address, void* output, std::size_t bytes, bool executable) = 0;

    protected:
        ~CustomPresentReads() = default;
    };
    struct CustomPresentProbeRoots
    {
        std::uintptr_t presenter = 0, table = 0, device = 0;
        bool           tableRead = false, deviceRead = false;
    };
    void ClassifyCustomPresent(CustomPresentReads& reads, std::uintptr_t image, std::uintptr_t presenter, std::uintptr_t nativeDevice,
                               CustomPresentClassificationState& state, CustomPresentProbeRoots& roots) noexcept;
    bool CustomPresentRootsUnchanged(CustomPresentReads& reads, const CustomPresentProbeRoots& roots) noexcept;
}
