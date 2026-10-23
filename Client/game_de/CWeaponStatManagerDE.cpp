/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve MTA live-stat lookup, defaults and concrete ownership
 *****************************************************************************/
#include "CWeaponStatManagerDE.h"
#include <game/CWeaponStartupPolicy.h>
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

    void CWeaponStatManagerDE::InitializeLiveStats()
    {
        if (startupState != StartupState::NotStarted || !ownedLive.empty())
            throw std::logic_error("DE weapon-stat startup requires a fresh unattempted live registry");
        // Retain the existing synchronous entry for callers that already own
        // its full operation. Native constructor preparation uses one row per
        // natural callback instead of renewing a lease inside this loop.
        while (!ContinueLiveStats())
        {
        }
    }

    bool CWeaponStatManagerDE::ContinueLiveStats(StartupOwner confirm)
    {
        if (startupState == StartupState::Complete || startupState == StartupState::Failed || ownedLive.size() != completedStartupRows ||
            completedStartupRows >= NativeWeaponInfoProfile::RowCount)
            throw std::logic_error("DE weapon-stat startup cannot replay or adopt another live registry");
        startupState = StartupState::Running;
        const auto selectedRow = completedStartupRows;
        try
        {
            bool found = false;
            // Filter the genuine shared schedule by one captured cursor. Never
            // copy the standard/skill itinerary or let advancing the cursor
            // select another row during this same iteration.
            MtaGame::CWeaponStartupPolicy::Initialize(
                [this, selectedRow, &found](const MtaGame::CWeaponStartupPolicy::Entry& entry)
                {
                    if (entry.row == selectedRow)
                    {
                        if (found)
                            throw std::logic_error("DE shared weapon startup row is not unique");
                        found = true;
                        CreateWeaponStat(bank.GetSlot(entry.row), entry.statType, entry.skill);
                    }
                });
            if (!found)
                throw std::logic_error("DE shared weapon startup row was not found");
            if (confirm && confirm() != ClockResult::Ok)
                throw std::runtime_error("DE weapon startup lost its native owner after the row");
            ++completedStartupRows;
            if (completedStartupRows == NativeWeaponInfoProfile::RowCount)
                startupState = StartupState::Complete;
            return startupState == StartupState::Complete;
        }
        catch (...)
        {
            // Registration and accepted native writes precede acknowledgement.
            // Preserve the failing row plus every completed row, but make this
            // manager lifetime terminal: no replay, rollback or cursor reset.
            startupState = StartupState::Failed;
            throw;
        }
    }
}
