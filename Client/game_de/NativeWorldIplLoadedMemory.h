/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit actual loaded IPL receivers inside the existing native owner
 ****************************************************************************/
#pragma once
#include "NativeStreamingCompletionContext.h"
#include <array>
#include <vector>
namespace mta::game_de::NativeWorldIplLoadedMemory
{
    struct Ipl
    {
        std::uint32_t                   index = 0;
        std::array<unsigned char, 0x34> bytes{};
    };
    struct Token
    {
        std::uintptr_t             address = 0;
        std::vector<unsigned char> bytes;
    };
    struct Pool
    {
        std::uintptr_t root = 0, address = 0, objects = 0, flags = 0;
        std::int32_t   capacity = 0;
        std::size_t    stride = 0;
    };
    struct Deleted
    {
        std::uintptr_t address = 0, flag = 0;
        std::uint32_t  ipl = 0;
        unsigned char  generation = 0;
    };
    struct Snapshot
    {
        std::vector<Token>                                   tokens;
        std::vector<Pool>                                    pools;
        std::vector<Deleted>                                 deleted;
        std::vector<NativeUObjectMembershipAccess::Snapshot> objects;
        // Class/provider authorities survive entity destruction; their raw
        // identity must still agree after a returned original, while deleted
        // native rows and their UE actors must never be dereferenced again.
        std::vector<Token>                                   authorityTokens;
        std::vector<NativeUObjectMembershipAccess::Snapshot> authorities;
    };
    // The outer IPL Capture already owns GameThread/no-GC/AfterApplication
    // authority. This value-only gate never calls a native engine original,
    // retains a raw receiver beyond that callback, or creates a second owner.
    ClockResult Capture(std::uintptr_t image, const NativeStreamingCompletionContext::Value&, const std::vector<Ipl>&, Snapshot&);
    bool        Seal(std::uintptr_t image, const Snapshot&) noexcept;
    // Whole RemoveModel legitimately destroys the captured receivers. Check
    // pool identities and freed generations without dereferencing dead rows.
    bool Removed(std::uintptr_t image, const Snapshot&, std::uint32_t ipl) noexcept;
}
