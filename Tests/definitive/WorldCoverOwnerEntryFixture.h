/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Explicit final native cover-original doubles for owned fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeWorldCoverCalls.h"
namespace NativeWorldCoverFixture
{
    struct State;
}
namespace ClockWorldCoverOwnerTest
{
    inline NativeWorldCoverFixture::State* current = nullptr;
    inline unsigned                        removes = 0, clears = 0, checksAfterFirst = 0;
    inline bool                            failAfterEffect = false, invalidInvocation = false, knownRefusal = false;
    mta::game_de::ClockResult              Invoke(mta::game_de::NativeWorldCoverCalls::Operation, std::uintptr_t, std::uintptr_t,
                                                  mta::game_de::NativeWorldCoverCalls::Receipt&) noexcept;
}
