/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve classic backend code while cleanup tickets still borrow it
 ****************************************************************************/
#pragma once

namespace MtaGame::ClassicCustomAnimationTeardown
{
    template <class Game>
    bool NeedsRetention(Game* game) noexcept
    {
        if (!game)
            return false;
        try
        {
            const auto manager = game->GetAnimManager();
            // A missing/refused owner cannot certify that ticket code is free.
            return !manager || manager->HasPendingCustomAnimationCleanup();
        }
        catch (...)
        {
            return true;
        }
    }

    template <class Game, class Unload, class Retain>
    bool Finish(Game*& game, bool earlierPending, Unload unload, Retain retain)
    {
        // Query before Terminate can self-delete the game and its manager.
        // GameSA closes its creator first and retains itself while pending.
        const bool pending = earlierPending || NeedsRetention(game);
        if (game)
        {
            game->Terminate();
            game = nullptr;
        }
        if (pending)
            retain();
        else
            unload();
        return pending;
    }
}
