/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bound only the five native builtin rows and source-qualified sparse fields
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
    // Fields/OwnedBytes remain the established five-float prefix contract.
    // New logical fields use a sparse map: the parser writes BYTE50 sparks,
    // unlike the classic DWORD, and widens the FX pointer to QWORD30..37.
    // That pointer and padding are equality-only raw row seals, never payload
    // fields or whole-row restore authority. Vector components remain separate
    // DWORD writes; the domain retains any accepted multi-component prefix.
    inline constexpr std::size_t PropertyCount = 22;
    struct Descriptor
    {
        std::size_t offset, bytes;
    };
    inline constexpr std::array<Descriptor, PropertyCount> Descriptors = {{
        {0x00, 4},  // Mass
        {0x04, 4},  // TurnMass
        {0x08, 4},  // AirResistance
        {0x0c, 4},  // Elasticity
        {0x10, 4},  // Buoyancy
        {0x14, 4},  // UprootLimit
        {0x18, 4},  // CollisionDamageMultiplier
        {0x1c, 1},  // CollisionDamageEffect
        {0x1d, 1},  // CollisionSpecialResponseCase
        {0x1e, 1},  // CameraAvoidObject
        {0x1f, 1},  // CausesExplosion
        {0x20, 1},  // FxType
        {0x24, 4},  // FxOffsetX
        {0x28, 4},  // FxOffsetY
        {0x2c, 4},  // FxOffsetZ
        {0x38, 4},  // SmashMultiplier
        {0x3c, 4},  // BreakVelocityX
        {0x40, 4},  // BreakVelocityY
        {0x44, 4},  // BreakVelocityZ
        {0x48, 4},  // BreakVelocityRandomness
        {0x4c, 4},  // BreakMode
        {0x50, 1},  // SparksOnImpact
    }};
    // Keep future descriptor edits from silently widening a BYTE, crossing
    // the native FX pointer, aliasing another field, or leaving the row.
    static_assert(
        []() constexpr
        {
            for (std::size_t index = 0; index != Descriptors.size(); ++index)
            {
                const auto field = Descriptors[index];
                const auto word = field.offset & ~std::size_t{3};
                if ((field.bytes != 1 && field.bytes != 4) || field.offset + field.bytes > RowBytes ||
                    (field.bytes == 4 && field.offset % sizeof(std::uint32_t)) || word + sizeof(std::uint32_t) > RowBytes ||
                    (word < 0x38 && word + sizeof(std::uint32_t) > 0x30))
                    return false;
                for (std::size_t previous = 0; previous != index; ++previous)
                    if (field.offset < Descriptors[previous].offset + Descriptors[previous].bytes && Descriptors[previous].offset < field.offset + field.bytes)
                        return false;
            }
            return true;
        }());
    inline constexpr std::uintptr_t                FormatRva = 0x43096d0;
    inline constexpr std::array<unsigned char, 72> Format = {
        0x25, 0x73, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20,
        0x25, 0x64, 0x20, 0x25, 0x64, 0x20, 0x25, 0x64, 0x20, 0x25, 0x64, 0x20, 0x25, 0x64, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20,
        0x25, 0x73, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x66, 0x20, 0x25, 0x64, 0x20, 0x25, 0x64, 0x00};
    inline constexpr std::array<Pin, 5> Pins = {{
        {Source::ObjectDatProducerRva, Source::ObjectDatProducer.data(), Source::ObjectDatProducer.size(), true},
        {0x1212ba0, Source::PhysicsConsumer.data(), Source::PhysicsConsumer.size(), true},
        {Source::ObjectDatNameRva, Source::ObjectDatName.data(), Source::ObjectDatName.size(), false},
        {Source::ReadModeRva, Source::ReadMode.data(), Source::ReadMode.size(), false},
        {FormatRva, Format.data(), Format.size(), false},
    }};
}
