/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit final BoundRect/WorldRemove originals for real Runtime tests
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldRemoveCalls.h"
#include "../../Client/game_de/NativeWorldBoundRectCalls.h"
#include "../../Client/game_de/NativeWorldBoundRectProfile.h"
#include "../../Client/game_de/NativePedOrientationProfile.h"

namespace NativeWorldRemoveFixture
{
    struct State;
}
namespace ClockWorldRemoveOwnerTest
{
    inline NativeWorldRemoveFixture::State* current = nullptr;
    inline std::uintptr_t                   image = 0;
    inline unsigned                         removes = 0, bounds = 0;
    inline bool                             failAfterEffect = false, invalidInvocation = false;
    mta::game_de::ClockResult               Invoke(mta::game_de::NativeWorldRemoveCalls::Entry, void*, std::uint32_t&) noexcept;
    mta::game_de::ClockResult ReadBound(mta::game_de::NativeWorldBoundRectCalls::Entry, std::uintptr_t, mta::game_de::NativeWorldBoundRectCalls::Rect&,
                                        mta::game_de::NativeWorldBoundRectCalls::Rect*&, std::uint32_t&) noexcept;
}
