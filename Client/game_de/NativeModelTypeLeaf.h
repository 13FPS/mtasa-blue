/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the entire executed path of an immediate native model-type getter
 ****************************************************************************/
#pragma once
#include "NativeImageAccess.h"
#include <array>
#include <cstdint>
namespace mta::game_de::NativeModelTypeLeaf
{
    // The caller supplies a separately qualified CURRENT native model virtual20
    // under its own bank/vtable/thread/GC ownership. A matching instruction path
    // does not establish class identity, archive allocation or a retained lease.
    // No virtual call is made, and every refusal preserves both outputs.
    inline bool Read(std::uintptr_t image, std::uintptr_t function, std::array<unsigned char, 6>& observedBytes, std::uint8_t& type) noexcept
    {
        std::array<unsigned char, 6> bytes{};
        std::uint8_t                 value = 0;
        if (!NativeImageAccess::ReadImage(image, function, bytes.data(), bytes.size(), false, true))
            return false;
        if (bytes[0] == 0xb0 && bytes[2] == 0xc3)
            value = bytes[1];
        else if (bytes[0] == 0xb8 && bytes[2] == 0 && bytes[3] == 0 && bytes[4] == 0 && bytes[5] == 0xc3)
            value = bytes[1];
        else if (bytes[0] == 0x33 && bytes[1] == 0xc0 && bytes[2] == 0xc3)
            value = 0;
        else
            return false;
        // Native consumers compare AL (including native type8). Keep that raw
        // byte intact; interpretation belongs to each genuine caller's policy.
        observedBytes = bytes;
        type = value;
        return true;
    }
}
