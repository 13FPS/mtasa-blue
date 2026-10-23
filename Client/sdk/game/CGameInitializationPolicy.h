/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's later game initialization order across game backends
 *****************************************************************************/
#pragma once

namespace MtaGame
{
    class CGameInitializationPolicy final
    {
    public:
        template <class InitializeGarages, class SetupSpecialCharacters, class SetupBrokenModels, class InitializeRenderer, class HideNativeMenu>
        static void Initialize(InitializeGarages&& garages, SetupSpecialCharacters&& characters, SetupBrokenModels&& brokenModels,
                               InitializeRenderer&& renderer, HideNativeMenu&& menu)
        {
            // This is the later CGame::Initialize itinerary, not constructor
            // weapon preparation. Each explicit call starts the same sequence;
            // reconnects must not acquire a new global once/complete guard.
            garages();
            characters();
            brokenModels();
            renderer();
            menu();
            // Exceptions leave their already-applied prefix with the existing
            // backend owner. This policy does not catch, roll back or retry.
        }
    };
}
