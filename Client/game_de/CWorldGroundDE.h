/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the actual MTA world SDK for one qualified scalar query
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include <CVector2D.h>
#include "NativeWorldGroundAccess.h"
// The shared legacy entity/world headers contain an enum forward declaration
// and anonymous surface-property fields. Keep those existing ABI declarations
// unchanged while retaining strict diagnostics for this implementation.
#pragma warning(push)
#pragma warning(disable : 4471 4201)
#include <game/CWorld.h>
#pragma warning(pop)
#include <type_traits>
namespace mta::game_de
{
    class CWorldGroundDE : public CWorld
    {
    public:
        float FindGroundZFor3DPosition(CVector* position) override;

    protected:
        explicit CWorldGroundDE(NativeWorldGroundAccess& owner) noexcept : groundOwner(owner) {}

    private:
        NativeWorldGroundAccess& groundOwner;
    };
    // Ground query admission supplies neither a complete world nor ownership
    // of a collision entity. Every remaining native world operation is pure.
    static_assert(std::is_abstract_v<CWorldGroundDE> && std::is_base_of_v<CWorld, CWorldGroundDE>);
}
