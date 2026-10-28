/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow the genuine already initialized shared ped collision default
 ****************************************************************************/
#pragma once
#include "NativeModelBankReader.h"
#include <array>

namespace mta::game_de::NativePedModelDefaultCollision
{
    using CheckOwner = NativeModelBankReader::CheckOwner;
    struct Value
    {
        std::uintptr_t                  canonical = 0, data = 0, spheres = 0, segments = 0;
        std::uintptr_t                  dataAllocation = 0, sphereAllocation = 0, segmentAllocation = 0;
        std::array<unsigned char, 0x38> canonicalBytes{};
        std::array<unsigned char, 0x20> dataBytes{};
        std::array<unsigned char, 0x3b> sphereBytes{};
        std::array<unsigned char, 0x3c> segmentBytes{};
        std::array<unsigned char, 16>   boundsLiteral{};
        std::array<unsigned char, 4>    maxYLiteral{};
    };

    // The caller retains the existing shared native gate and synchronous game
    // owner. These immediate snapshots create no collision API, initializer,
    // native lifetime lease, allocation adoption or deallocation authority.
    ClockResult Read(std::uintptr_t image, CheckOwner, Value&) noexcept;
    ClockResult Recheck(std::uintptr_t image, CheckOwner, const Value&) noexcept;
    bool        Same(const Value&, const Value&) noexcept;
}
