/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Use MTA's animation request itinerary with genuine borrowed SDK services
 *****************************************************************************/
#pragma once
#include "CAnimBlockDE.h"
#include <type_traits>

class CGame;
class CStreaming;

namespace mta::game_de
{
    // AddRef and all missing native animation ownership/interface operations
    // remain pure. This slice does not publish a complete animation factory.
    class CAnimBlockRequestDE : public CAnimBlockDE
    {
    public:
        void Request(EModelRequestType type, bool allowBlockingFail = false) override;

    protected:
        CAnimBlockRequestDE(NativeAnimationBlockAccess& owner, const NativeAnimationBlockIdentity& identity, CGame& game)
            : CAnimBlockDE(owner, identity), game(game)
        {
        }

    private:
        CStreaming& RequireStreaming();
        // The same actual game and native block owner must outlive the facade;
        // request orchestration creates no second native owner or ref counter.
        CGame& game;
    };
    static_assert(std::is_abstract_v<CAnimBlockRequestDE> && std::is_base_of_v<CAnimBlock, CAnimBlockRequestDE>);
}
