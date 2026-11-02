/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact current-owner hook overlays without weakening native profiles
 ****************************************************************************/
#pragma once
#include "NativePlayerMoveAnimHookBridge.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de
{
    class NativePlayerMoveAnimHookController;
}

namespace mta::game_de::NativePlayerMoveAnimHookOverlay
{
    class Patch final
    {
    public:
        Patch() noexcept = default;
        ~Patch() = default;
        Patch(const Patch&) = delete;
        Patch& operator=(const Patch&) = delete;
        Patch(Patch&&) = delete;
        Patch& operator=(Patch&&) = delete;

        // Const delegates keep the controller as the sole authority-writing
        // friend; free matching functions need no access to mutable proof data.
        bool MatchesProcess(std::uintptr_t expectedImage, const unsigned char* span, std::size_t spanBytes) const noexcept;
        bool MatchesRebuild(std::uintptr_t expectedImage, const unsigned char* span, std::size_t spanBytes) const noexcept;

    private:
        friend class ::mta::game_de::NativePlayerMoveAnimHookController;

        // Only the real controller acknowledges write/protect/flush and binds
        // these bytes to its live admission. Withdrawal clears this same object;
        // deleted copying/moving prevents a surviving duplicate acknowledgement.
        std::uintptr_t                       image = 0, site = 0, stub = 0;
        std::uint64_t                        generation = 0, admissionSerial = 0;
        NativePlayerMoveAnimHookBridge::Kind kind = NativePlayerMoveAnimHookBridge::Kind::ProcessCompare;
        std::array<unsigned char, 6>         bytes{};
        std::uint32_t                        size = 0;
        bool                                 installedVerified = false;

        void Invalidate() noexcept;
        bool Matches(std::uintptr_t expectedImage, const unsigned char* span, std::size_t spanBytes,
                     NativePlayerMoveAnimHookBridge::Kind expectedKind) const noexcept;
    };

    // Input spans are caller-owned readable buffers. This helper performs no
    // allocation, native patching or process read and grants no admission lease.
    // The controller must serialize proof withdrawal and bookend its generation.
    bool MatchesProcess(const Patch&, std::uintptr_t image, const unsigned char* span, std::size_t spanBytes) noexcept;
    bool MatchesRebuild(const Patch&, std::uintptr_t image, const unsigned char* span, std::size_t spanBytes) noexcept;
}
