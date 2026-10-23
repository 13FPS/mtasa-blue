/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own complete live MTA weapon stats over the retained native bank
 *****************************************************************************/
#pragma once

#include "CWeaponStatManagerPolicyDE.h"
#include "CWeaponStatNativeDE.h"

namespace mta::game_de
{
    // The bank and its existing write owner outlive this manager. Constructing
    // SDK wrappers grants no engine lease and never initializes native fields.
    // Publication and every later field operation retain the original owner's
    // callback, thread, generation and exact native profile checks.
    class CWeaponStatManagerDE final : public CWeaponStatManagerPolicyDE
    {
    public:
        CWeaponStatManagerDE(CWeaponInfoBankDE& bank, NativeWeaponInfoWriteAccess& writes);
        ~CWeaponStatManagerDE();

        void CreateWeaponStat(CWeaponInfo* existing, eWeaponType type, eWeaponSkill skill) override;

        enum class StartupState : unsigned char
        {
            NotStarted,
            Running,
            Complete,
            Failed
        };
        // Separate from construction and ordinary factory calls. A qualified
        // future CGame initialization owner may attempt this exact MTA startup
        // schedule once on an empty live list; no native lease is created here.
        using StartupOwner = ClockResult (*)() noexcept;
        void InitializeLiveStats();
        // Startup preparation advances one whole row through the SAME shared
        // itinerary before a future CGame is constructed. The caller supplies
        // its current native callback check; no lease is refreshed here.
        bool         ContinueLiveStats(StartupOwner confirm = nullptr);
        std::size_t  GetCompletedStartupRows() const noexcept { return completedStartupRows; }
        std::size_t  GetRegisteredLiveRows() const noexcept { return ownedLive.size(); }
        StartupState GetStartupState() const noexcept { return startupState; }

    private:
        // The SDK lacks a virtual destructor. Keep the exact concrete native
        // type in the owning list instead of deleting through CWeaponStat.
        class LiveStat final : public CWeaponStatNativeDE
        {
        public:
            LiveStat(CWeaponInfoBankDE& bank, NativeWeaponInfoWriteAccess& writes, std::size_t row, eWeaponType type, eWeaponSkill skill,
                     CWeaponStatManagerDE& manager)
                : CWeaponStatNativeDE(bank, writes, row, type, skill, ReadOriginal, static_cast<CWeaponStatManagerPolicyDE*>(&manager))
            {
            }
        };

        void        AddLive(std::size_t row, eWeaponType type, eWeaponSkill skill);
        void        RetireLive() noexcept;
        std::size_t RequireExisting(CWeaponInfo* existing);

        CWeaponInfoBankDE&                   bank;
        NativeWeaponInfoWriteAccess&         writes;
        std::list<std::unique_ptr<LiveStat>> ownedLive;
        StartupState                         startupState = StartupState::NotStarted;
        std::size_t                          completedStartupRows = 0;
    };
    static_assert(!std::is_abstract_v<CWeaponStatManagerDE> && std::is_base_of_v<CWeaponStatManager, CWeaponStatManagerDE>);
}
