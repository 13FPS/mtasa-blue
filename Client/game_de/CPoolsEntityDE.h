/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Translate existing controlled ped interfaces through the actual pools SDK
 ****************************************************************************/
#pragma once
#include "CPoolsDE.h"
namespace mta::game_de
{
    // This partial receiver supports raw null and existing same-owner controlled
    // peds. An unknown nonzero address refuses; it cannot claim that an object,
    // vehicle or building lacks an SDK/client association.
    class CPoolsEntityDE : public CPoolsDE
    {
    public:
        CEntity*       GetEntity(DWORD* nativeInterface) override;
        CClientEntity* GetClientEntity(DWORD* nativeInterface) override;

    protected:
        explicit CPoolsEntityDE(NativeGameTimingAccess& owner) noexcept : CPoolsDE(owner), translationOwner(owner) {}

    private:
        ClockResult             ReadEntity(std::uintptr_t, CEntity*&, CClientEntity**) noexcept;
        static void             RequireTranslation(ClockResult, const char*);
        NativeGameTimingAccess& translationOwner;
    };
    // Translation adds neither a complete pool service nor native adoption.
    // Every allocation/removal/factory and unsupported pool operation stays pure.
    static_assert(std::is_abstract_v<CPoolsEntityDE> && std::is_base_of_v<CPoolsDE, CPoolsEntityDE>);
}
