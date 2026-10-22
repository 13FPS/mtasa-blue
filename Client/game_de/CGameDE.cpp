/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse completed SDK services in the future complete DE game object
 *****************************************************************************/
#include "CGameDE.h"

// Pool SDK headers use the shared vector constants normally supplied by the
// classic PCH; the DE backend must provide that dependency explicitly.
#include <SharedUtil.Defines.h>
#include <game/CPools.h>
#include <game/CWeaponStatManager.h>

#include <stdexcept>
#include <string>

namespace mta::game_de
{
    CGameDE::CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter,
                     const NativeStreamingFileIdsReader& streamingReader, const NativeGameStateReader& stateReader) noexcept
        : CGameWeaponInfoDE(reader, timingWriter), clock(nativeClock), streaming(streamingReader), gameState(stateReader)
    {
    }

    CWeaponInfo* CGameDE::ResolveWeaponInfoSlot(std::size_t slot)
    {
        // Selection stays with MTA's shared SDK policy. Its native owner alone
        // may publish a stable, genuine wrapper after qualifying this callback
        // and row; a native row pointer cannot substitute for that SDK object.
        CWeaponInfo* output = nullptr;
        RequireNativeWrite(NativeWrites().GetWeaponInfoSlot(slot, output), "GetWeaponInfo");
        return output;
    }

    eGameVersion CGameDE::GetGameVersion()
    {
        // This identifies the actual SDK backend without borrowing a world
        // object or impersonating a classic regional executable version.
        return VERSION_DE;
    }

    CClock* CGameDE::GetClock() noexcept
    {
        return &clock;
    }

    CKeyGen* CGameDE::GetKeyGen() noexcept
    {
        return &keyGen;
    }

    CWeaponStatManager* CGameDE::GetWeaponStatManager()
    {
        // Reuse the complete manager owned by this callback runtime. CPU
        // originals and an empty live registry do not imply that CGame native
        // initialization/default population has already run.
        CWeaponStatManager* output = nullptr;
        RequireNativeWrite(NativeWrites().GetWeaponStatManager(output), "GetWeaponStatManager");
        if (!output)
            RequireNativeWrite(ClockResult::NotInitialized, "GetWeaponStatManager");
        return output;
    }

    CWeaponStat* CGameDE::CreateWeaponStat(eWeaponType weapon, eWeaponSkill skill)
    {
        // Like the classic CGame factory, use this manager's unlisted CPU
        // factory so later flag transitions read its mutable originals. The
        // caller owns the record and must retire it before this borrowed
        // manager; no live registration or native default writes occur here.
        auto* output = GetWeaponStatManager()->CreateWeaponStatUnlisted(weapon, skill);
        if (!output)
            RequireNativeWrite(ClockResult::NotInitialized, "CreateWeaponStat");
        return output;
    }

    CStats* CGameDE::GetStats()
    {
        // The runtime owns a complete SDK stats object and its native reader.
        // Publish only that qualified borrowed object, never a copied bank or
        // an incomplete read-only slice masquerading as a writable service.
        CStats* output = nullptr;
        RequireNativeWrite(NativeWrites().GetStats(output), "GetStats");
        if (!output)
            RequireNativeWrite(ClockResult::NotInitialized, "GetStats");
        return output;
    }

    CRopes* CGameDE::GetRopes()
    {
        // Publish only the complete object owned by this same callback runtime.
        // Its identity is stable, but every action requires a fresh native scope.
        CRopes* output = nullptr;
        RequireNativeWrite(NativeWrites().GetRopes(output), "GetRopes");
        if (!output)
            RequireNativeWrite(ClockResult::NotInitialized, "GetRopes");
        return output;
    }

    CPlayerInfo* CGameDE::GetPlayerInfo()
    {
        // The callback runtime owns the complete SDK service and its Wanted
        // dependency. Borrow that qualified identity, rather than treating a
        // native row address or an unavailable result as a player-info object.
        CPlayerInfo* output = nullptr;
        RequireNativeWrite(NativeWrites().GetPlayerInfo(output), "GetPlayerInfo");
        if (!output)
            RequireNativeWrite(ClockResult::NotInitialized, "GetPlayerInfo");
        return output;
    }

    CWeather* CGameDE::GetWeather()
    {
        // MTA receives the complete service owned by the existing callback
        // runtime. A partial wrapper or successful null result cannot satisfy
        // the SDK's weather getter/reset contract.
        CWeather* output = nullptr;
        RequireNativeWrite(NativeWrites().GetWeather(output), "GetWeather");
        if (!output)
            RequireNativeWrite(ClockResult::NotInitialized, "GetWeather");
        return output;
    }

    void CGameDE::StartGame()
    {
        // The normal native menu request owns additional UI/loading effects.
        // Delegate it only through the explicit startup-frame owner; a state
        // write or an observation callback cannot impersonate that transaction.
        RequireNativeWrite(NativeWrites().StartGame(), "StartGame");
    }

    void CGameDE::SetAsyncLoadingFromScript(bool enabled, bool forced)
    {
        asyncLoading.SetScriptOverride(enabled, forced);
    }

    void CGameDE::SuspendASyncLoading(bool suspended, uint autoUnsuspendDelay)
    {
        // This is the same MTA scheduling policy used by the classic backend;
        // it does not fabricate native streaming requests or CStreamingInfo.
        asyncLoading.Suspend(suspended, autoUnsuspendDelay);
    }

    bool CGameDE::IsASyncLoadingEnabled(bool ignoreSuspend)
    {
        return asyncLoading.IsEnabled(ignoreSuspend);
    }

    int& CGameDE::GetCheckStatus()
    {
        // MTA owns this diagnostic slot independently of the engine's startup
        // state. Returning the actual slot preserves the SDK reference contract.
        return checkStatus;
    }

    void CGameDE::OnPedContextChange(CPed* context)
    {
        pedContext = context;
    }

    CPed* CGameDE::GetPedContext()
    {
        if (!pedContext)
        {
            // Preserve MTA's lazy local-player fallback through its real pool
            // interface. A raw DE actor pointer is not an SDK CPed instance.
            auto* pools = GetPools();
            if (!pools)
                throw std::runtime_error("DE CGame ped context requires an available pool service");
            pedContext = pools->GetPedFromRef(DWORD{1});
        }
        return pedContext;
    }

    void CGameDE::SetPreWeaponFireHandler(PreWeaponFireHandler* handler)
    {
        preWeaponFireHandler = handler;
    }

    void CGameDE::SetPostWeaponFireHandler(PostWeaponFireHandler* handler)
    {
        postWeaponFireHandler = handler;
    }

    void CGameDE::SetTaskSimpleBeHitHandler(TaskSimpleBeHitHandler* handler)
    {
        taskSimpleBeHitHandler = handler;
    }

    void CGameDE::SetJetpackWeaponEnabled(eWeaponType weaponType, bool enabled)
    {
        // Reuse MTA's policy table while DE's actual native jetpack consumer
        // remains unimplemented. This setter does not claim native effects.
        jetpackWeapons.Set(static_cast<int>(weaponType), enabled);
    }

    bool CGameDE::GetJetpackWeaponEnabled(eWeaponType weaponType)
    {
        return jetpackWeapons.Get(static_cast<int>(weaponType));
    }

    SystemState::Enum CGameDE::GetSystemState()
    {
        SystemState::Enum state;
        const auto        result = gameState.Read(state);
        // DE startup indices are not a portable enum contract. Unknown early
        // states must refuse rather than admit MTA's gameplay/loading paths
        // using an unproved cast or an old snapshot from another native state.
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame system state unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return state;
    }

    bool CGameDE::IsAtMenu()
    {
        bool       visible;
        const auto result = gameState.ReadMenu(visible);
        // DE pause menus have a managed widget owner as well as the legacy
        // frontend stack. An unavailable owner cannot silently admit MTA's
        // gameplay or chat paths by returning a guessed false menu state.
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame menu state unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return visible;
    }

    void CGameDE::SetSystemState(SystemState::Enum state)
    {
        // Use the same native generation and engine-thread owner as timing.
        // A scalar dispatcher request does not perform the additional native
        // frontend teardown and loading operations required by StartGame.
        RequireNativeWrite(NativeWrites().SetSystemState(state), "SetSystemState");
    }

    bool CGameDE::VerifySADataFileNames()
    {
        // Like the classic SDK method, compare the names the native loaders
        // actually consume. Filesystem existence cannot detect redirected
        // operands and checking a nearby unused literal would miss that edit.
        auto result = dataFileNames.IsBound() ? Status::Ok : dataFileNames.Bind();
        bool verified = false;
        if (result == Status::Ok)
            result = dataFileNames.Read(verified);
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame data filenames unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return verified;
    }

    bool CGameDE::IsCheatEnabled(const char* name)
    {
        MtaGame::CWorldCheatPolicy::Id id;
        if (!MtaGame::CWorldCheatPolicy::Find(name, id))
            return false;
        bool enabled = false;
        // A known but unqualified registry row must refuse explicitly, never
        // look like a successfully observed disabled native cheat.
        RequireNativeWrite(NativeWrites().ReadWorldCheat(id, enabled), "IsCheatEnabled");
        return enabled;
    }

    bool CGameDE::SetCheatEnabled(const char* name, bool enabled)
    {
        MtaGame::CWorldCheatPolicy::Id id;
        NativeCheatProfile::Flag       flag;
        if (!MtaGame::CWorldCheatPolicy::Find(name, id) || !MtaGame::CWorldCheatPolicy::Get(id)->canBeSet || !NativeCheatProfile::Find(name, flag))
            return false;
        // These four properties are the existing MTA settable registry. The
        // native command dispatcher also performs unrelated actions, so the
        // admitted owner writes only the independently qualified flag byte.
        // ResetCheats/PerformChecks remain pure until the other registry rows
        // have genuine DE implementations; this is no anti-cheat certificate.
        RequireNativeWrite(NativeWrites().SetCheatFlag(flag, enabled), "SetCheatEnabled");
        return true;
    }

    NativeStreamingFileIds CGameDE::RequireStreamingLayout() const
    {
        NativeStreamingFileIds ids;
        const auto             result = streaming.Read(ids);
        // File-ID ranges are part of the SDK contract, not a reason to reuse
        // classic CStreamingInfo storage. A changed DE layout must refuse
        // explicitly before a client can index an unrelated native structure.
        if (result != Status::Ok)
            throw std::runtime_error("DE CGame streaming IDs unavailable (status " + std::to_string(static_cast<std::uint32_t>(result)) + ")");
        return ids;
    }

    uint32_t CGameDE::GetBaseIDforDFF()
    {
        return RequireStreamingLayout().dff;
    }

    uint32_t CGameDE::GetBaseIDforTXD()
    {
        return RequireStreamingLayout().txd;
    }

    uint32_t CGameDE::GetBaseIDforCOL()
    {
        return RequireStreamingLayout().col;
    }

    uint32_t CGameDE::GetBaseIDforIPL()
    {
        return RequireStreamingLayout().ipl;
    }

    uint32_t CGameDE::GetBaseIDforDAT()
    {
        return RequireStreamingLayout().dat;
    }

    uint32_t CGameDE::GetBaseIDforIFP()
    {
        return RequireStreamingLayout().ifp;
    }

    uint32_t CGameDE::GetBaseIDforRRR()
    {
        return RequireStreamingLayout().rrr;
    }

    uint32_t CGameDE::GetBaseIDforSCM()
    {
        return RequireStreamingLayout().scm;
    }

    uint32_t CGameDE::GetCountOfAllFileIDs()
    {
        // This SDK count includes the native array's four sentinel rows;
        // neither count nor category bases imply a classic row layout.
        return RequireStreamingLayout().count;
    }
}
