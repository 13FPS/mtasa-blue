/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep the main target role separate from selected camera-row cells
 ****************************************************************************/
#pragma once
#include "NativeCameraDataProfile.h"
#include "NativeCameraRestoreProfile.h"
#include "NativeCameraWidescreenProfile.h"
namespace mta::game_de::NativeCameraMainTargetProfile
{
    inline constexpr std::size_t TargetOffset = 0x7f8, TargetBytes = 8, CameraBytes = 0x800;
    inline constexpr std::size_t MaximumPinBytes = NativeCameraWidescreenProfile::MaximumPinBytes;
    // Reuse literal source identity, not either operation's effect domain.
    // The consumer fills main+7F8 and copies it to the distinct row+1A0.
    inline constexpr auto&                                       Restore = NativeCameraRestoreProfile::Restore;
    inline constexpr auto&                                       Consumer = NativeCameraWidescreenProfile::Consumer;
    inline constexpr std::array<NativeCameraDataProfile::Pin, 2> Pins = {{
        {NativeCameraRestoreProfile::EntryRva, Restore.data(), Restore.size()},
        {0x111c770, Consumer.data(), Consumer.size()},
    }};
}
