/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Immediate current native counts and bounded first-match name lookup
 ****************************************************************************/
#pragma once
#include "NativeAnimationManagerAccess.h"
#include "NativeAnimationAssociationGroupAccess.h"
#include "NativeAnimationStaticAssociationAccess.h"
namespace mta::game_de
{
    class NativeAnimationManagerMemory
    {
    public:
        using Binding = NativePedPoolReader::Binding;
        using CheckOwner = NativePedPoolReader::CheckOwner;
        NativeAnimationManagerMemory(Binding value, CheckOwner callback) noexcept : binding(value), owner(callback) {}
        static bool Qualify(Binding&) noexcept;
        static bool Revalidate(const Binding&) noexcept;
        ClockResult ReadCounts(NativeAnimationManagerCounts&) const noexcept;
        // Separate source observations preserve the old count contract and do
        // not couple ordinary block/animation reads to definition metadata.
        ClockResult ReadDefinitionCount(std::int32_t&) const noexcept;
        ClockResult ReadGroupName(std::uint32_t, std::uintptr_t&) const noexcept;
        ClockResult ReadGroupBlockName(std::uint32_t, std::uintptr_t&) const noexcept;
        ClockResult FindFirstGroupByBlockName(const std::string&, std::uint32_t&) const noexcept;
        // Read only the current native array's source DWORD Created value;
        // a copied bool grants no group, child-array or release ownership.
        ClockResult ReadGroupCreated(std::uint32_t, bool&) const noexcept;
        ClockResult ReadStaticAssociationAnimID(std::uint32_t, std::uint32_t, std::int16_t&) const noexcept;
        ClockResult ResolveStaticAssociationById(std::uint32_t, std::uint32_t, NativeAnimationStaticAssociationReferenceToken&) const noexcept;
        ClockResult ResolveAssociationGroup(std::uint32_t, NativeAnimationAssociationGroupReferenceToken&) const noexcept;
        ClockResult ReadAssociationGroup(const NativeAnimationAssociationGroupReferenceToken&, NativeAnimationAssociationGroupValues&) const noexcept;
        ClockResult ReadAssociationGroupLoaded(const NativeAnimationAssociationGroupReferenceToken&, bool&) const noexcept;
        ClockResult ResolveStaticAssociation(std::uint32_t, std::size_t, NativeAnimationStaticAssociationReferenceToken&) const noexcept;
        ClockResult ReadStaticAssociation(const NativeAnimationStaticAssociationReferenceToken&, NativeAnimationStaticAssociationValues&) const noexcept;
        enum class StaticAssociationScalar : std::uint8_t
        {
            NumBlendNodes,
            AnimID,
            AnimGroup,
            Flags
        };
        struct StaticAssociationWriteReceipt
        {
            StaticAssociationScalar field = StaticAssociationScalar::NumBlendNodes;
            std::uintptr_t          address = 0;
            std::uint16_t           before = 0, target = 0, observed = 0;
            std::uint32_t           exceptionCode = 0;
            bool                    entered = false, accepted = false, verified = false;
        };
        using StaticAssociationExchange = ClockResult (*)(std::uintptr_t, std::uint16_t, std::uint16_t, std::uint16_t&, bool&, std::uint32_t&) noexcept;
        // One persistent WORD store, never node-vector allocation authority.
        // A lost post-entry acknowledgement must stop without replay/rollback.
        ClockResult WriteStaticAssociation(const NativeAnimationStaticAssociationReferenceToken&, StaticAssociationScalar, std::uint16_t,
                                           StaticAssociationWriteReceipt* = nullptr, StaticAssociationExchange = nullptr) const noexcept;
        ClockResult ResolveStaticAssociationReference(std::uintptr_t, NativeAnimationStaticAssociationReferenceToken&) const noexcept;
        ClockResult FindBlockIndex(const NativeAnimationBlockName&, std::int32_t&) const noexcept;

    private:
        Binding    binding;
        CheckOwner owner;
    };
}
