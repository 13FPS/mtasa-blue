/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact cover/list/static-reference observations before native effects
 ****************************************************************************/
#pragma once
#include "NativeWorldCoverProfile.h"
#include "NativeWorldPoolMatrixMemory.h"
#include <array>
#include <vector>
namespace mta::game_de::NativeWorldCoverMemory
{
    struct Slot
    {
        std::uintptr_t address = 0, value = 0;
    };
    struct Value
    {
        NativeStreamingCompletionContext::Value world;
        NativeModelReferenceHeap::Value         heap;
        // Plain identities survive a callback; private membership snapshots do
        // not. Persisting a journal must clear world before retaining it.
        std::vector<std::uintptr_t> identity;
        std::vector<unsigned char>  points = std::vector<unsigned char>(NativeWorldCoverProfile::PointCapacity * NativeWorldCoverProfile::PointBytes);
        std::vector<unsigned char>  references = std::vector<unsigned char>(NativeWorldCoverProfile::RefCapacity * NativeWorldCoverProfile::RefBytes);
        std::array<unsigned char, NativeWorldCoverProfile::PoolHeaderBytes> header{};
        std::uintptr_t                                                      pool = 0, objects = 0, flags = 0, head = 0, free = 0;
        std::uint32_t                                                       count = 0;
        std::vector<unsigned char>                                          nodes, generations;
        std::vector<std::size_t>                                            order;
        std::vector<NativeWorldPoolMatrixMemory::Entity>                    owners;
        std::vector<Slot>                                                   slots;
    };
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult Capture(std::uintptr_t image, Value&, const Value* retained = nullptr);
    bool        Seal(std::uintptr_t image, const Value&) noexcept;
    bool        Same(const Value&, const Value&) noexcept;
    bool        ExpectRemove(std::uintptr_t image, Value&, std::uintptr_t entity) noexcept;
    bool        ExpectClear(Value&) noexcept;
}
