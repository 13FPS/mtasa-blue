/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bound only the five native builtin rows and consumed scalar fields
 ****************************************************************************/
#pragma once
#include "NativeModelObjectGroupProfile.h"

namespace mta::game_de::NativeObjectGroupPropertiesProfile
{
    namespace Source = NativeModelObjectGroupProfile;
    using Pin = Source::Pin;
    // The complete OBJECT.DAT producer initializes these five rows before it
    // starts the dynamic/deduplicated local row index at five. This is a finite
    // lower bound on real storage, not an inferred160-row table capacity.
    inline constexpr std::uintptr_t TableRva = 0x526a760;
    inline constexpr std::size_t    RowBytes = 0x58, BuiltinRows = 5, Fields = 5, OwnedBytes = Fields * sizeof(std::uint32_t);
    inline constexpr std::size_t    TableBytes = BuiltinRows * RowBytes, MaximumPinBytes = Source::MaximumPinBytes;
    // Independent object physics initialization copies these five FLOATs from
    // offsets0/4/8/c/10 to native object mass/turnmass/air/elasticity/buoyancy.
    // The FX pointer at30 and every unqualified field are deliberately outside
    // the owned range; a full88-byte memcpy would lack their lifetime authority.
    inline constexpr std::array<Pin, 4> Pins = {{
        {Source::ObjectDatProducerRva, Source::ObjectDatProducer.data(), Source::ObjectDatProducer.size(), true},
        {0x1212ba0, Source::PhysicsConsumer.data(), Source::PhysicsConsumer.size(), true},
        {Source::ObjectDatNameRva, Source::ObjectDatName.data(), Source::ObjectDatName.size(), false},
        {Source::ReadModeRva, Source::ReadMode.data(), Source::ReadMode.size(), false},
    }};
}
