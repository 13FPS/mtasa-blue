/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw CWeapon field intent without bypassing native ownership
 *****************************************************************************/
#include "CWeaponNativeDE.h"
#include "NativeGameTimingAccess.h"
#include <cstring>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        template <class Enum>
        std::uint32_t WeaponEnumBits(const Enum& value) noexcept
        {
            static_assert(sizeof(Enum) == sizeof(std::uint32_t));
            std::uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
        // These unfixed SDK enums have language ranges wider than their named
        // values. Raw native storage remains DWORD, but a typed SDK return
        // must not manufacture an enum outside the language's valid range.
        static_assert(WEAPONTYPE_INVALID == 255 && WEAPONSTATE_MELEE_MADECONTACT == 4);
    }

    CWeaponNativeDE::CWeaponNativeDE(NativeGameTimingAccess& access, const NativePedReferenceToken& token, CPed* borrowedOwner, eWeaponSlot slot) noexcept
        : CWeaponPolicyDE(borrowedOwner, slot), native(access), retainedToken(token), nativeSlot(WeaponEnumBits(slot))
    {
    }

    void CWeaponNativeDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE CWeapon ") + operation + " unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) +
                                     ")");
    }

    eWeaponType CWeaponNativeDE::GetType() const
    {
        std::uint32_t value = 0;
        Require(native.ReadPedWeaponType(retainedToken, nativeSlot, value), "GetType");
        if (value > 255)
            Require(ClockResult::InvalidArgument, "GetType");
        return static_cast<eWeaponType>(value);
    }

    void CWeaponNativeDE::SetType(eWeaponType type)
    {
        Require(native.SetPedWeaponType(retainedToken, nativeSlot, WeaponEnumBits(type)), "SetType");
    }

    eWeaponState CWeaponNativeDE::GetState() const
    {
        std::uint32_t value = 0;
        Require(native.ReadPedWeaponState(retainedToken, nativeSlot, value), "GetState");
        if (value > 7)
            Require(ClockResult::InvalidArgument, "GetState");
        return static_cast<eWeaponState>(value);
    }

    void CWeaponNativeDE::SetState(eWeaponState state)
    {
        Require(native.SetPedWeaponState(retainedToken, nativeSlot, WeaponEnumBits(state)), "SetState");
    }

    std::uint32_t CWeaponNativeDE::GetAmmoInClip() const
    {
        std::uint32_t value = 0;
        Require(native.ReadPedWeaponAmmoInClip(retainedToken, nativeSlot, value), "GetAmmoInClip");
        return value;
    }

    void CWeaponNativeDE::SetAmmoInClip(std::uint32_t ammo)
    {
        // A receiver may have accepted an effect before losing its token.
        // Forward once and surface that failure; never retry or compensate.
        Require(native.SetPedWeaponAmmoInClip(retainedToken, nativeSlot, ammo), "SetAmmoInClip");
    }

    std::uint32_t CWeaponNativeDE::GetAmmoTotal() const
    {
        std::uint32_t value = 0;
        Require(native.ReadPedWeaponAmmoTotal(retainedToken, nativeSlot, value), "GetAmmoTotal");
        return value;
    }

    void CWeaponNativeDE::SetAmmoTotal(std::uint32_t ammo)
    {
        Require(native.SetPedWeaponAmmoTotal(retainedToken, nativeSlot, ammo), "SetAmmoTotal");
    }

    CWeaponInfo* CWeaponNativeDE::GetInfo(eWeaponSkill skill) const
    {
        // Selection and wrapper identity belong to the same genuine weapon
        // bank and registered native owner, not a duplicate SDK-side table.
        CWeaponInfo* value = nullptr;
        Require(native.ReadPedWeaponInfo(retainedToken, nativeSlot, skill, value), "GetInfo");
        return value;
    }
}
