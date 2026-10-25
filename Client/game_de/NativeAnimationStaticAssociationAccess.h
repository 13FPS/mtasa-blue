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
    };
}
