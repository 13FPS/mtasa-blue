/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current physical static-association row reads, not ID reselection
 ****************************************************************************/
#pragma once
#include <game/CGameClockService.h>
#include <cstddef>
#include <cstdint>
namespace mta::game_de
{
    struct NativeAnimationStaticAssociationReferenceToken
    {
        std::uint64_t  ownerGeneration = 0;
        std::uint32_t  groupIndex = 0;
        std::size_t    index = 0;
        std::uintptr_t locator = 0;
    };
    struct NativeAnimationStaticAssociationValues
    {
        std::uintptr_t interfaceAddress = 0;
        std::uint16_t  numBlendNodes = 0;
        std::int16_t   animId = 0, animGroup = 0, flags = 0;
        std::uintptr_t hierarchyAddress = 0;
    };
    class NativeAnimationStaticAssociationAccess
    {
    public:
        virtual ~NativeAnimationStaticAssociationAccess() = default;
        // Resolve a physical child index. The copied locator/index remains
        // stable if a group's ID offset changes; no animation-ID reselection
        // or retained row/child lifetime is implied by this immediate token.
        virtual ClockResult ResolveAnimationStaticAssociation(std::uint64_t, std::uint32_t, std::size_t,
                                                              NativeAnimationStaticAssociationReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Current parent/child/cookie/row qualification does not require
        // Created, node payloads, or a nonnull/valid hierarchy target. +18 is
        // observed as an opaque pointer; +10 is the separate node array.
        virtual ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken&,
                                                           NativeAnimationStaticAssociationValues&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Factory selectors return a freshly admitted physical row. Numeric
        // animation IDs use the original modulo32 offset/physical capacity;
        // raw interface tags are resolved, never adopted as private storage.
        virtual ClockResult ResolveAnimationStaticAssociationById(std::uint64_t, std::uint32_t, std::uint32_t,
                                                                  NativeAnimationStaticAssociationReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult ResolveAnimationStaticAssociationInterface(std::uint64_t, std::uintptr_t, NativeAnimationStaticAssociationReferenceToken&) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // This is the original initializer on the current genuine node-less
        // same-bank row and a registered live clump. A tag alone grants no
        // clump, node allocation, arbitrary hierarchy or replacement authority.
        virtual ClockResult InitializeAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken&, std::uintptr_t, std::uintptr_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        // Source setters store exactly one WORD. The owner must re-admit the
        // physical row before/after; borrowing it grants no node allocation,
        // initialization or free rights. Count growth needs separate payload
        // authority and is explicitly refused in the supported domain.
        virtual ClockResult SetAnimationStaticAssociationNumBlendNodes(const NativeAnimationStaticAssociationReferenceToken&, std::uint16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetAnimationStaticAssociationAnimID(const NativeAnimationStaticAssociationReferenceToken&, std::int16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetAnimationStaticAssociationAnimGroup(const NativeAnimationStaticAssociationReferenceToken&, std::int16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
        virtual ClockResult SetAnimationStaticAssociationFlags(const NativeAnimationStaticAssociationReferenceToken&, std::int16_t) noexcept
        {
            return ClockResult::NotInitialized;
        }
    };
}
