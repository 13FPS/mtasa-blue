/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read-only acknowledgement of an actual accepted private row write
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceCopyCalls.h"
namespace mta::game_de::NativeAnimationSequenceOperationCalls
{
    // Explicit standalone-role controller only: the current private original
    // array can retire without interpreting partially written child metadata.
    ClockResult QualifyStandaloneRetirement(std::uintptr_t, const NativeCustomAnimSequenceStorage&, std::uint64_t,
                                            NativeCustomAnimSequenceStorageCalls::CheckOwner) noexcept;
    ClockResult AcknowledgeAcceptedRow(std::uintptr_t image, NativeCustomAnimSequenceStorage&, std::uint64_t, std::size_t index,
                                       const NativeAnimationSequenceCopyCalls::Record& actualIntent, bool frameBinding,
                                       NativeCustomAnimSequenceStorageCalls::CheckOwner) noexcept;
}
