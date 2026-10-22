/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA live-stat lookup, defaults and concrete ownership
 *****************************************************************************/
#include "CWeaponStatManagerDE.h"
#include <stdexcept>

namespace mta::game_de
{
    CWeaponStatManagerDE::CWeaponStatManagerDE(CWeaponInfoBankDE& existingBank, NativeWeaponInfoWriteAccess& existingWrites)
        : bank(existingBank), writes(existingWrites)
    {
        // Actual MTA constructs CPU originals here; CGame later explicitly
        // creates its live stats. Keep that registry empty until the caller
        // invokes the real factory with native default-write authority.
    }

    CWeaponStatManagerDE::~CWeaponStatManagerDE()
    {
        RetireLive();
    }

    void CWeaponStatManagerDE::RetireLive() noexcept
    {
        while (!ownedLive.empty())
        {
            UnregisterLive(*ownedLive.back());
            ownedLive.pop_back();
        }
    }

    std::size_t CWeaponStatManagerDE::RequireExisting(CWeaponInfo* existing)
    {
        // Never cast or dereference a caller's object as a native row. Even a
        // genuine wrapper belonging to another bank is a foreign identity.
        if (existing)
            for (std::size_t row = 0; row < NativeWeaponInfoProfile::RowCount; ++row)
                if (existing == bank.GetSlot(row))
                    return row;
        throw std::invalid_argument("DE live weapon stat requires this manager's existing SDK bank wrapper");
    }

    void CWeaponStatManagerDE::AddLive(std::size_t row, eWeaponType type, eWeaponSkill skill)
    {
        auto  value = std::make_unique<LiveStat>(bank, writes, row, type, skill, *this);
        auto* pointer = value.get();
        ownedLive.push_back(std::move(value));
        try
        {
            if (!RegisterLive(*pointer))
                throw std::logic_error("DE live weapon-stat identity was already registered");
        }
        catch (...)
        {
            // RegisterLive's list insertion has the standard strong guarantee;
            // failure has published no borrowed entry for the concrete value.
            ownedLive.pop_back();
            throw;
        }
        // Preserve actual MTA factory ordering: append first, then load shared
        // defaults. A refusal during native writes keeps that registered stat
        // and accepted prefix; no hidden row rollback or replacement CPU copy.
        LoadDefault(pointer, type, skill);
    }

    void CWeaponStatManagerDE::CreateWeaponStat(CWeaponInfo* existing, eWeaponType type, eWeaponSkill skill)
    {
        AddLive(RequireExisting(existing), type, skill);
    }
}
