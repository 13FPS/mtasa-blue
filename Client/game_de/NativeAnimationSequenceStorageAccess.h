/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Optional current-owner custom animation storage service for MTA's SDK
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstddef>
#include <cstdint>
namespace mta::game_de
{
    class NativeAnimationSequenceStorageAccess
    {
    public:
        virtual ~NativeAnimationSequenceStorageAccess() = default;
        // Even zero/null calls admit the current generation. The runtime owns
        // issued allocation records and retained native cleanup debt; this
        // borrowed service never adopts arbitrary engine-bank interfaces.
        virtual ClockResult AllocateAnimationSequences(std::uint64_t ownerGeneration, std::size_t capacity, std::uintptr_t& output) noexcept = 0;
        virtual ClockResult FreeAnimationSequences(std::uint64_t ownerGeneration, std::uintptr_t issuedTag) noexcept = 0;
        // Keyframe bytes are a distinct issued allocation kind. Preserve the
        // owner's nullable allocator result; no array cookie is exposed.
        virtual ClockResult AllocateAnimationKeyFrames(std::uint64_t, std::uint32_t, std::uintptr_t&) noexcept = 0;
        virtual ClockResult FreeAnimationKeyFrames(std::uint64_t, std::uintptr_t) noexcept = 0;
        // A trusted SDK caller owns the currently issued, unfreed first tag.
        // A raw pointer adds no serial or stale-address ABA guarantee.
    };
}
