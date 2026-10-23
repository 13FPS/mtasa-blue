/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Assemble completed DE services behind the existing MTA CGame SDK
 *****************************************************************************/
#pragma once

#include "CGameWeaponInfoDE.h"
#include "CClockDE.h"
#include "CKeyGenDE.h"
#include "CAsyncLoadingDE.h"
#include "NativeStreamingFileIdsReader.h"
#include "NativeGameStateReader.h"
#include "NativeDataFileNamesReader.h"
#include <CJetpackWeaponPolicy.h>
#include "WeaponFireHandlerAccess.h"

namespace mta::game_de
{
    // Keep the actual SDK object graph in the backend. The clock remains the
    // engine callback owner's existing instance, so a future complete CGame
    // cannot accidentally create a second native clock or cleanup owner.
    // Unimplemented SDK methods remain pure: this is not yet a factory product.
    class CGameDE : public CGameWeaponInfoDE, public WeaponFireHandlerAccess
    {
    public:
        eGameVersion        GetGameVersion() override;
        CClock*             GetClock() noexcept override;
        CPlayerInfo*        GetPlayerInfo() override;
        CRopes*             GetRopes() override;
        CStats*             GetStats() override;
        CWeaponStatManager* GetWeaponStatManager() override;
        CWeaponStat*        CreateWeaponStat(eWeaponType weapon, eWeaponSkill skill) override;
        CWeather*           GetWeather() override;
        void                StartGame() override;
        CKeyGen*            GetKeyGen() noexcept override;
        SystemState::Enum   GetSystemState() override;
        void                SetSystemState(SystemState::Enum state) override;
        bool                IsAtMenu() override;
        bool                VerifySADataFileNames() override;
        bool                IsCheatEnabled(const char* name) override;
        bool                SetCheatEnabled(const char* name, bool enabled) override;

        void SetAsyncLoadingFromScript(bool enabled, bool forced) override;
        void SuspendASyncLoading(bool suspended, uint autoUnsuspendDelay = 0) override;
        bool IsASyncLoadingEnabled(bool ignoreSuspend = false) override;

        int&  GetCheckStatus() override;
        void  OnPedContextChange(CPed* context) override;
        CPed* GetPedContext() override;

        void SetPreWeaponFireHandler(PreWeaponFireHandler* handler) override;
        void SetPostWeaponFireHandler(PostWeaponFireHandler* handler) override;
        void SetTaskSimpleBeHitHandler(TaskSimpleBeHitHandler* handler) override;

        void SetJetpackWeaponEnabled(eWeaponType weaponType, bool enabled) override;
        bool GetJetpackWeaponEnabled(eWeaponType weaponType) override;

        uint32_t GetBaseIDforDFF() override;
        uint32_t GetBaseIDforTXD() override;
        uint32_t GetBaseIDforCOL() override;
        uint32_t GetBaseIDforIPL() override;
        uint32_t GetBaseIDforDAT() override;
        uint32_t GetBaseIDforIFP() override;
        uint32_t GetBaseIDforRRR() override;
        uint32_t GetBaseIDforSCM() override;
        uint32_t GetCountOfAllFileIDs() override;

        CGameDE(const CGameDE&) = delete;
        CGameDE& operator=(const CGameDE&) = delete;

    protected:
        CWeaponInfo* ResolveWeaponInfoSlot(std::size_t slot) override;

        enum class ConstructorServices : unsigned char
        {
            PreparedWeapons
        };
        // Native preparation happens on prior admitted normal frames. This
        // explicit constructor consumes ready services without replaying the
        // native schedule or moving it into later CGame::Initialize.
        CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter,
                const NativeStreamingFileIdsReader& streamingReader, const NativeGameStateReader& stateReader, ConstructorServices);

        // Unlike PreparedWeapons, this explicit overload consumes every
        // completed service from one current loaded timing-owner generation.
        // The supplied value is re-admitted; it is not a producer-made lease.
        CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter,
                const NativeStreamingFileIdsReader& streamingReader, const NativeGameStateReader& stateReader, const NativeGameConstructorServices& services);

        // Borrowed services must outlive this object. Their qualification
        // and engine-thread rules stay with their existing native owners.
        CGameDE(const NativeGameTimingReader& reader, CClockDE& nativeClock, NativeGameTimingAccess& timingWriter,
                const NativeStreamingFileIdsReader& streamingReader, const NativeGameStateReader& stateReader) noexcept;

        // Registration is SDK bookkeeping only. A future qualified DE hook
        // must read these borrowed handlers rather than installing a second
        // registration store or assuming that registration enabled dispatch.
        PreWeaponFireHandler*   GetPreWeaponFireHandler() const noexcept override { return preWeaponFireHandler; }
        PostWeaponFireHandler*  GetPostWeaponFireHandler() const noexcept override { return postWeaponFireHandler; }
        TaskSimpleBeHitHandler* GetTaskSimpleBeHitHandler() const noexcept { return taskSimpleBeHitHandler; }

    private:
        NativeStreamingFileIds               RequireStreamingLayout() const;
        const NativeGameConstructorServices& RequirePreparedServices(const char* operation);

        NativeGameConstructorServices                                                      preparedServices;
        CWeaponStatManager*                                                                preparedWeaponManager = nullptr;
        CClockDE&                                                                          clock;
        CKeyGenDE                                                                          keyGen;
        CAsyncLoadingDE                                                                    asyncLoading;
        const NativeStreamingFileIdsReader&                                                streaming;
        const NativeGameStateReader&                                                       gameState;
        NativeDataFileNamesReader                                                          dataFileNames;
        int                                                                                checkStatus = 0;
        CPed*                                                                              pedContext = nullptr;
        PreWeaponFireHandler*                                                              preWeaponFireHandler = nullptr;
        PostWeaponFireHandler*                                                             postWeaponFireHandler = nullptr;
        TaskSimpleBeHitHandler*                                                            taskSimpleBeHitHandler = nullptr;
        MtaGame::CJetpackWeaponPolicy<WEAPONTYPE_LAST_WEAPONTYPE, WEAPONTYPE_BRASSKNUCKLE> jetpackWeapons;
    };
    static_assert(std::is_abstract_v<CGameDE> && std::is_base_of_v<CGame, CGameDE>);
}
