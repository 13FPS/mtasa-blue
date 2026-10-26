/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Independent common-entity ped render-field provenance
 ****************************************************************************/
#pragma once
#include "NativePedMetadataProfile.h"
namespace mta::game_de::NativePedRenderProfile
{
    inline constexpr std::size_t    RenderOffset = 0x20, MaximumPinBytes = 56;
    inline constexpr std::uintptr_t PedVtableRva = NativePedMetadataProfile::PedVtableRva, PlayerVtableRva = NativePedMetadataProfile::PlayerVtableRva;
    using Pin = NativePedMetadataProfile::Pin;
    // Same common entity lineage, but this channel does not acquire model/type
    // field predicates or make the returned render value a clump lease.
    inline constexpr std::array<unsigned char, 46> CreateReturn = {
        0x49, 0x8b, 0x06, 0x48, 0x89, 0x4c, 0x24, 0x20, 0x49, 0x8b, 0xce, 0x4c, 0x8b, 0x50, 0x60, 0x74, 0x13, 0x44, 0x88, 0x05, 0xdd, 0xc3, 0x5f,
        0x04, 0x41, 0xff, 0xd2, 0x44, 0x88, 0x3d, 0xd3, 0xc3, 0x5f, 0x04, 0xeb, 0x03, 0x41, 0xff, 0xd2, 0x48, 0x89, 0x47, 0x20, 0x48, 0x8b, 0xd8};
    inline constexpr std::array<unsigned char, 7> RetireReadAndZero{0x48, 0x8b, 0x4d, 0x20, 0x45, 0x33, 0xe4};
    inline constexpr std::array<unsigned char, 4> RetireClear{0x4c, 0x89, 0x65, 0x20};
    inline constexpr std::array<Pin, 7>           Code{
                  {{0x112e8a0, NativePedMetadataProfile::BaseEntityMetadataInitialization.data(), NativePedMetadataProfile::BaseEntityMetadataInitialization.size()},
                   {0x115c8f0, NativePedMetadataProfile::PhysicalCallsEntityBase.data(), NativePedMetadataProfile::PhysicalCallsEntityBase.size()},
                   {0x1219860, NativePedMetadataProfile::GenericPedCallsPhysicalBase.data(), NativePedMetadataProfile::GenericPedCallsPhysicalBase.size()},
                   {0x124dffa, NativePedMetadataProfile::PlayerInheritsCommonPedMetadata.data(), NativePedMetadataProfile::PlayerInheritsCommonPedMetadata.size()},
                   {0x112f91b, CreateReturn.data(), CreateReturn.size()},
                   {0x112fdbe, RetireReadAndZero.data(), RetireReadAndZero.size()},
                   {0x112fde3, RetireClear.data(), RetireClear.size()}}};
}
