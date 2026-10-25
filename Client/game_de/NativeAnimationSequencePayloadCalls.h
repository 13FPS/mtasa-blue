/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Held current original-frame spans for genuine private postprocessors
 ****************************************************************************/
#pragma once
#include "NativeAnimationSequenceDependencyCalls.h"
namespace mta::game_de::NativeAnimationSequencePayloadCalls
{
    using Storage = NativeCustomAnimSequenceStorage;
    using Epoch = NativeAnimationSequenceDependencyCalls::Epoch;
    using CheckOwner = NativeAnimationSequenceDependencyCalls::CheckOwner;
    using ResolveFrame = NativeAnimationSequenceDependencyCalls::ResolveFrame;
    enum class Operation : std::uint32_t
    {
        RemoveQuaternionFlips,
        CalculateTotalTime
    };
    // ResolveFrame is the serialized owner's accepted Uncompressed descriptor
    // producer. This native half independently qualifies exact original record
    // identity/pages/heap and the full span; raw flags alone are insufficient.
    // Epoch borrows the existing gate. No recursive TryEnter or payload read.
    ClockResult QualifyHeld(const Epoch&, const Storage& array, std::uint16_t count, Operation, CheckOwner, ResolveFrame, void*) noexcept;
}
