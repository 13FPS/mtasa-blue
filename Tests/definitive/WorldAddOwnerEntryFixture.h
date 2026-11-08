/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit final BoundRect/WorldAdd originals for real Runtime tests
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldAddEntryCalls.h"
#include "../../Client/game_de/NativeWorldBoundRectProfile.h"
#include "../../Client/game_de/NativeWorldAddProfile.h"

namespace WorldAddOwnerFixture
{
    struct State;
}
namespace ClockWorldAddOwnerTest
{
    inline WorldAddOwnerFixture::State* current = nullptr;
    inline std::uintptr_t               image = 0;
    inline unsigned                     adds = 0, bounds = 0;
    inline bool                         failAfterEffect = false, invalidInvocation = false;
    mta::game_de::ClockResult           Invoke(mta::game_de::NativeWorldAddEntryCalls::Entry, std::uintptr_t, std::uint32_t&) noexcept;
    mta::game_de::ClockResult ReadBound(mta::game_de::NativeWorldBoundRectCalls::Entry, std::uintptr_t, mta::game_de::NativeWorldBoundRectCalls::Rect&,
                                        mta::game_de::NativeWorldBoundRectCalls::Rect*&, std::uint32_t&) noexcept;
}
