/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CWeapon raw fields through the registered ped's native owner
 *****************************************************************************/
#pragma once

#include "CWeaponPolicyDE.h"
#include "NativePedReferenceToken.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativeGameTimingAccess;

    // A selected ped slot borrows the same immutable reference token as the
    // surrounding ped. It is not an allocated standalone engine weapon.
    // Initialization, removal, firing and reload remain pure.
    class CWeaponNativeDE : public CWeaponPolicyDE
    {
    public:
        eWeaponType   GetType() const override;
        void          SetType(eWeaponType type) override;
        eWeaponState  GetState() const override;
        void          SetState(eWeaponState state) override;
        std::uint32_t GetAmmoInClip() const override;
        void          SetAmmoInClip(std::uint32_t ammo) override;
        std::uint32_t GetAmmoTotal() const override;
        void          SetAmmoTotal(std::uint32_t ammo) override;
        CWeaponInfo*  GetInfo(eWeaponSkill skill) const override;

    protected:
        CWeaponNativeDE(NativeGameTimingAccess& access, const NativePedReferenceToken& token, CPed* borrowedOwner, eWeaponSlot slot) noexcept;

    private:
        static void                   Require(ClockResult result, const char* operation);
        NativeGameTimingAccess&       native;
        const NativePedReferenceToken retainedToken;
        const std::uint32_t           nativeSlot;
    };
    static_assert(std::is_abstract_v<CWeaponNativeDE> && std::is_base_of_v<CWeapon, CWeaponNativeDE>);
    static_assert(sizeof(eWeaponType) == sizeof(std::uint32_t) && sizeof(eWeaponState) == sizeof(std::uint32_t) &&
                  sizeof(eWeaponSlot) == sizeof(std::uint32_t));
}
