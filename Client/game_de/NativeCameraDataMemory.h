/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Observe native camera scalar bits through the existing owner
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include "NativeCameraRowBorrow.h"
#include "NativeCameraDataProfile.h"
#include "NativeStreamingCompletionContext.h"
#include <cstdint>
class CVector;
namespace mta::game_de::NativeCameraDataMemory
{
    using CheckOwner = ClockResult (*)() noexcept;
    struct RowTargetWriteSnapshot
    {
        NativeStreamingCompletionContext::Value                      world;
        std::array<unsigned char, NativeCameraDataProfile::RowBytes> bytes{};
        std::uintptr_t                                               cell = 0, target = 0;
    };
    // Internal transaction observation: the setter owns the one streaming
    // scope for its whole itinerary. Taking a second public-reader gate here
    // would reject the real owner after a successful reference prefix.
    ClockResult CaptureRowTargetForWrite(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                                         const NativeCameraRowBorrow::Token& token, CheckOwner owner, CheckOwner afterApplicationOwner,
                                         RowTargetWriteSnapshot& output) noexcept;
    ClockResult AcquireRowBorrow(std::uintptr_t image, std::uint32_t row, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                                 CheckOwner owner, CheckOwner afterApplicationOwner, NativeCameraRowBorrow::Token& output) noexcept;
    ClockResult ReadRowVector(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                              const NativeCameraRowBorrow::Token& token, CameraRowVectorKind kind, CheckOwner owner, CheckOwner afterApplicationOwner,
                              CVector*& output) noexcept;
    ClockResult ReadRowHistory(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                               const NativeCameraRowBorrow::Token& token, CheckOwner owner, CheckOwner afterApplicationOwner, CVector*& output) noexcept;
    ClockResult ReadRowTarget(std::uintptr_t image, NativeCameraRowBorrow& ledger, const NativeCameraRowBorrow::Identity& identity,
                              const NativeCameraRowBorrow::Token& token, CheckOwner owner, CheckOwner afterApplicationOwner, std::uintptr_t& output) noexcept;
    // These owned scalar observations grant no camera row, writable vector,
    // native lifetime lease or setter. Refusal leaves the caller unchanged.
    // New scalar roles have a separate source profile; their addition does
    // not strengthen or otherwise change existing active/FOV observations.
    ClockResult ReadMode(std::uintptr_t image, std::uint32_t row, CheckOwner owner, std::uint16_t& rawMode) noexcept;
    ClockResult ReadDirection(std::uintptr_t image, std::uint32_t row, CheckOwner owner, std::uint32_t& horizontal, std::uint32_t& vertical) noexcept;
    ClockResult ReadActive(std::uintptr_t image, CheckOwner owner, std::uint8_t& output) noexcept;
    ClockResult ReadFov(std::uintptr_t image, std::uint32_t logicalRow, CheckOwner owner, std::uint32_t& rawBits) noexcept;
}
