/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Compose the qualified ped and player slices under one native owner
 ****************************************************************************/
#pragma once
#include "CPedRebuildDE.h"
#include "CPedPooledDE.h"
#include "CPlayerPedMoveAnimDE.h"

namespace mta::game_de
{
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4250)
#endif
    // These existing chains intentionally share virtual CPed. One complete
    // wrapper therefore has one SDK identity and one physics repair state,
    // while its actual CPlayerPed conversion requires no native pointer cast.
    // The enclosing typed owner still supplies all remaining abstract methods,
    // registration lifetime, borrowed game/wanted lifetime and retirement.
    class CPlayerPedCompositionDE : public CPedRebuildDE, public CPedPooledDE, public CPlayerPedMoveAnimDE
    {
    public:
        ~CPlayerPedCompositionDE() override = default;

    protected:
        // Match CPedBorrowedFactoryDE's owner/token constructor prefix. Copying
        // the already-issued identity does not acquire another native reference
        // or register this still-abstract wrapper in the pool.
        CPlayerPedCompositionDE(NativeGameTimingAccess& owner, const NativePedReferenceToken& token, CGame& game, CPoolsDE& pools, CWanted* wanted,
                                std::uint32_t wrapperType);
        CPlayerPedCompositionDE(NativeGameTimingAccess& owner, const NativePedReferenceToken& token, CGame& game, CPoolsDE& pools, CWanted* wanted,
                                std::uint32_t wrapperType, CAnimManagerDE& animations);

    private:
        static NativeGameTimingAccess& RequirePoolOwner(CPoolsDE&, NativeGameTimingAccess&);
    };
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
    // Keep the incomplete factory boundary visible. In particular, intelligence,
    // physical repair, initial state and targeting are not supplied by composition.
    static_assert(std::is_abstract_v<CPlayerPedCompositionDE>);
    static_assert(std::is_convertible_v<CPlayerPedCompositionDE*, CPlayerPed*>);
    static_assert(std::is_convertible_v<CPlayerPedCompositionDE*, CPed*>);
    static_assert(std::is_convertible_v<CPlayerPedCompositionDE*, CPedVitalsDE*>);
    static_assert(!std::is_copy_constructible_v<CPlayerPedCompositionDE> && !std::is_move_constructible_v<CPlayerPedCompositionDE>);
}
