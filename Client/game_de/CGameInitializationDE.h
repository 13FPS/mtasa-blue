/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep later SDK initialization behind mandatory DE backend effects
 *****************************************************************************/
#pragma once

#include "CGameDE.h"
#include <type_traits>
#include <cstdint>

namespace mta::game_de
{
    // No complete CGame or native owner is created here. In particular, a
    // named garage facade does not supply the SDK's default numeric mapping.
    class CGameInitializationDE : public CGameDE
    {
    public:
        void Initialize() override;

    protected:
        // Preserve both existing constructor routes. Neither route invokes
        // later initialization or moves constructor weapon work into it.
        using CGameDE::CGameDE;

        // The existing backend owner must freshly admit ALL dependencies
        // before the first effect, including numeric garages, model setup,
        // renderer and native menu readiness. This is not a cached lease:
        // each later effect still retains its own current owner checks.
        virtual void RequireInitializationReady() = 0;
        void         SetupSpecialCharacters();
        void         SetupBrokenModels();

        // The qualified backend resolves the selected SDK facade even when
        // its native slot must be created/replaced by MakePedModel. Ordinary
        // loaded/valid-model lookup cannot substitute for this operation.
        // Reference lifetime and current owner remain backend obligations.
        virtual CModelInfo& ResolveStartupPedModel(std::uint32_t model) = 0;

        // Preserve FixModelCol's exact conditional borrowed-pointer copy,
        // including a null source collision. Native collision identity and
        // ownership must be qualified; SetColModel is a different policy.
        virtual void FixStartupModelCollision(std::uint32_t target, std::uint32_t source) = 0;
        // Classic Initialize is a typed CRenderWareSA method, absent from the
        // common renderer SDK. Keep its qualified DE counterpart mandatory
        // instead of inventing a common virtual or a successful no-op.
        virtual void InitializeRenderer() = 0;
        virtual void HideNativeMenu() = 0;
    };
    static_assert(std::is_abstract_v<CGameInitializationDE> && std::is_base_of_v<CGame, CGameInitializationDE>);
}
