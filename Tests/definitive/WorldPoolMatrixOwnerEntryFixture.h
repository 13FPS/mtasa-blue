/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Declare explicit final native-original doubles for owned bank tests
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldPoolEntryCalls.h"
#include <array>
namespace NativeWorldPoolMatrixFixture
{
    struct State;
}
namespace ClockWorldPoolMatrixOwnerTest
{
    inline NativeWorldPoolMatrixFixture::State* current = nullptr;
    inline std::uintptr_t                       image = 0;
    inline unsigned                             removes = 0, allocations = 0;
    inline bool                                 failAllocationAfterEffect = false, invalidInvocation = false;
    inline std::array<unsigned char, 40>        allocatedTail{};
    mta::game_de::ClockResult                   Invoke(mta::game_de::NativeWorldPoolEntryCalls::Entry, void*, std::uint32_t&) noexcept;
}
