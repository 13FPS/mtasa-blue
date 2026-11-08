/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Copy the native IPL bank/tree without exposing native row borrows
 ****************************************************************************/
#pragma once
#include "NativeWorldIplCalls.h"
#include "NativeWorldIplPlan.h"
#include "NativeStreamingCompletionContext.h"
#include "NativeModelReferenceHeap.h"
#include <array>
#include <vector>

namespace mta::game_de::NativeWorldIplMemory
{
    struct Pool
    {
        std::uintptr_t                address = 0, objects = 0, flags = 0;
        std::int32_t                  capacity = 0, cursor = 0;
        std::array<unsigned char, 32> header{};
        std::vector<unsigned char>    states;
        std::uint32_t                 free = 0;
    };
    struct Row
    {
        std::array<unsigned char, 0x34>    bytes{};
        std::uintptr_t                     address = 0;
        std::uint32_t                      index = 0;
        unsigned char                      flag = 0, streamState = 0;
        NativeWorldIplPlan::Rect           bounds{};
        MtaGame::CGameWorldPolicy::IplName name{};
    };
    struct Node
    {
        std::uintptr_t                address = 0, head = 0;
        std::array<unsigned char, 64> bytes{};
        NativeWorldIplPlan::Node      plan;
    };
    struct Link
    {
        std::uintptr_t                address = 0, item = 0, next = 0;
        std::array<unsigned char, 16> bytes{};
    };
    struct Snapshot
    {
        NativeStreamingCompletionContext::Value        world;
        NativeModelReferenceHeap::Value                heap;
        std::uintptr_t                                 tree = 0;
        Pool                                           ipl, quad, links;
        std::array<std::array<unsigned char, 16>, 256> stream{};
        std::vector<Row>                               rows;
        std::vector<Node>                              nodes;
        std::vector<Link>                              list;
    };
    bool        Profile(std::uintptr_t image) noexcept;
    ClockResult Capture(std::uintptr_t image, NativeWorldIplCalls::CheckOwner, NativeWorldIplCalls::CheckOwner afterApplicationOwner, Snapshot&,
                        bool forRemoval = false);
    // No owner/filter callback occurs in Seal. It is the final raw equality
    // bookend after the last owner call, including allocation generations,
    // row/name/bounds, stream states and tree/list links.
    bool Seal(std::uintptr_t image, const Snapshot&) noexcept;
    bool SameIdentity(const Snapshot&, const Snapshot&) noexcept;
    bool SameRows(const Snapshot&, const Snapshot&) noexcept;
    bool ReadRow(const Snapshot&, std::uint32_t, const Row*&) noexcept;
    bool StoreEnabled(const Row&) noexcept;
}
