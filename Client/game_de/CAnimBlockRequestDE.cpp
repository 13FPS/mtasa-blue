/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual animation request/ref/load boundaries and refusal
 *****************************************************************************/
#include "CAnimBlockRequestDE.h"
#include <game/CAnimBlockRequestPolicy.h>
#include <game/CGame.h>
#include <game/CStreaming.h>
#include <stdexcept>

namespace mta::game_de
{
    CStreaming& CAnimBlockRequestDE::RequireStreaming()
    {
        auto* streaming = game.GetStreaming();
        if (!streaming)
            throw std::runtime_error("DE animation block request requires the qualified streaming service");
        return *streaming;
    }

    void CAnimBlockRequestDE::Request(EModelRequestType type, bool allowBlockingFail)
    {
        MtaGame::CAnimBlockRequestPolicy::Request(
            static_cast<std::uint32_t>(type), allowBlockingFail, [this] { return IsLoaded(); }, [this] { return GetIndex(); },
            [this] { return game.GetBaseIDforIFP(); }, [this](std::uint32_t model) { return game.GetModelInfo(model); },
            [this](std::uint32_t model, std::uint32_t flags) { RequireStreaming().RequestModel(model, flags); }, [this] { AddRef(); }, [this](bool onlyPriority)
            { RequireStreaming().LoadAllRequestedModels(onlyPriority); }, [this](std::uint32_t model) { return RequireStreaming().HasModelLoaded(model); });
    }
}
