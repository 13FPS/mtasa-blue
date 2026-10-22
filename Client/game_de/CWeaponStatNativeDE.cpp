/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA live stat intent through the existing native owner
 ****************************************************************************/
#include "CWeaponStatNativeDE.h"
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    CWeaponStatNativeDE::CWeaponStatNativeDE(CWeaponInfoBankDE& bank, NativeWeaponInfoWriteAccess& writes, std::size_t row, eWeaponType weapon,
                                             eWeaponSkill skill, MtaGame::WeaponStatOriginalLookup lookup, void* context)
        : info(RequireSlot(bank, row)), native(writes), slot(row), weaponType(weapon), skillLevel(skill), originalLookup(lookup), originalContext(context)
    {
        if (!lookup)
            throw std::invalid_argument("DE native weapon stat requires its current original-stat provider");
    }

    CWeaponInfo& CWeaponStatNativeDE::RequireSlot(CWeaponInfoBankDE& bank, std::size_t row)
    {
        auto* value = bank.GetSlot(row);
        if (!value)
            throw std::invalid_argument("DE native weapon stat requires an existing SDK bank row");
        return *value;
    }

    void CWeaponStatNativeDE::RequireWrite(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE native CWeaponStat ") + operation + " unavailable (status " +
                                     std::to_string(static_cast<std::uint32_t>(result)) + ")");
    }

    void CWeaponStatNativeDE::Flags(DWORD desired)
    {
        // Resolve mutable manager originals lazily through the same MTA policy
        // used by classic and owned CPU stats. Accepted native component writes
        // remain persistent if a later component loses its callback lease.
        MtaGame::ApplyWeaponStatFlags(*this, desired, originalLookup, originalContext);
    }
    void CWeaponStatNativeDE::ToggleFlagBits(DWORD bits)
    {
        Flags(static_cast<DWORD>(GetFlags()) ^ bits);
    }
    void CWeaponStatNativeDE::SetFlagBits(DWORD bits)
    {
        Flags(static_cast<DWORD>(GetFlags()) | bits);
    }
    void CWeaponStatNativeDE::ClearFlagBits(DWORD bits)
    {
        Flags(static_cast<DWORD>(GetFlags()) & ~bits);
    }
    void CWeaponStatNativeDE::SetModel(int value)
    {
        RequireWrite(native.SetWeaponDword(slot, NativeWeaponDwordField::Model, static_cast<std::uint32_t>(value)), "SetModel");
    }
    void CWeaponStatNativeDE::SetModel2(int value)
    {
        RequireWrite(native.SetWeaponDword(slot, NativeWeaponDwordField::Model2, static_cast<std::uint32_t>(value)), "SetModel2");
    }
    void CWeaponStatNativeDE::SetRequiredStatLevel(int value)
    {
        // The integer SDK overload must preserve all signed32 bits rather than
        // round through float before reaching the existing exact DWORD writer.
        static_assert(sizeof(value) == sizeof(std::uint32_t));
        RequireWrite(native.SetWeaponDword(slot, NativeWeaponDwordField::RequiredStatLevel, static_cast<std::uint32_t>(value)), "SetRequiredStatLevel");
    }
    void CWeaponStatNativeDE::SetFireType(eFireType value)
    {
        RequireWrite(native.SetWeaponDword(slot, NativeWeaponDwordField::FireType, static_cast<std::uint32_t>(value)), "SetFireType");
    }
    void CWeaponStatNativeDE::SetWeaponAnimLoopStart(float value)
    {
        RequireWrite(native.SetWeaponFloat(slot, NativeWeaponFloatField::AnimLoopStart, value), "SetWeaponAnimLoopStart");
    }
    void CWeaponStatNativeDE::SetWeaponAnimLoopStop(float value)
    {
        RequireWrite(native.SetWeaponFloat(slot, NativeWeaponFloatField::AnimLoopStop, value), "SetWeaponAnimLoopStop");
    }
    void CWeaponStatNativeDE::SetWeaponAnim2LoopStart(float value)
    {
        RequireWrite(native.SetWeaponFloat(slot, NativeWeaponFloatField::Anim2LoopStart, value), "SetWeaponAnim2LoopStart");
    }
    void CWeaponStatNativeDE::SetWeaponAnim2LoopStop(float value)
    {
        RequireWrite(native.SetWeaponFloat(slot, NativeWeaponFloatField::Anim2LoopStop, value), "SetWeaponAnim2LoopStop");
    }
    void CWeaponStatNativeDE::SetWeaponAnim2LoopFireTime(float value)
    {
        RequireWrite(native.SetWeaponFloat(slot, NativeWeaponFloatField::Anim2LoopFireTime, value), "SetWeaponAnim2LoopFireTime");
    }
    void CWeaponStatNativeDE::SetAimOffsetIndex(short value)
    {
        RequireWrite(native.SetWeaponShort(slot, NativeWeaponShortField::AimOffsetIndex, value), "SetAimOffsetIndex");
    }
    void CWeaponStatNativeDE::SetDefaultCombo(BYTE value)
    {
        RequireWrite(native.SetWeaponShort(slot, NativeWeaponShortField::DefaultCombo, value), "SetDefaultCombo");
    }
    void CWeaponStatNativeDE::SetCombosAvailable(BYTE value)
    {
        // Classic stores the supplied byte directly. The native owner's
        // aligned masked exchange preserves DefaultCombo in the low byte;
        // accepting storage grants no claim about safe gameplay combinations.
        RequireWrite(native.SetWeaponShort(slot, NativeWeaponShortField::CombosAvailable, value), "SetCombosAvailable");
    }
}
