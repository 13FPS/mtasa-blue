// Compile the production startup methods with counters in place of GTA, Lua and network services.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using DWORD = unsigned long;
using std::size_t;
struct CClientPed;
struct CClientPlayer;
struct CClientGame;
static CClientGame* g_pClientGame;

static void Expect(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

struct Scenario
{
    bool pools = true, init = true, model = true, gameLoaded = true, connected = true;
    bool associate = true, matchingSlot = true, hasIntelligence = true, hasTasks = true, withdrawStream = false;
    int references = 0, registrations = 0, invalidations = 0, pedReads = 0, restored = 0;
    int streamEvents = 0, attemptsRefused = 0, created = 0, deleted = 0, resets = 0;
    int watchdog = 0, notices = 0, initialized = 0, buffers = 0, dialogs = 0, unloads = 0;
    int nativeResets = 0, animations = 0, vehicleReleases = 0;
    bool throwAdd = false, throwTasks = false, throwRender = false, requested = true;
    int remoteAdds = 0, remoteRemoves = 0, storageAdds = 0, streamOutEvents = 0;
    std::function<void()> onAcquire, onStream, onFocus, onDialog, onNotice, onVehicleRelease;
} scenario;

struct CVector { CVector(float, float, float) {} };
struct Tasks {};
using CTaskManager = Tasks;
struct Intelligence
{
    Tasks tasks;
    Tasks* GetTaskManager()
    {
        if (scenario.throwTasks) throw std::runtime_error("task lookup refused");
        return scenario.hasTasks ? &tasks : nullptr;
    }
};
struct CPed { virtual ~CPed() = default; };
struct CPlayerPed : CPed
{
    Intelligence intelligence;
    void* stored = nullptr;
    unsigned long model = 16;
    Intelligence* GetPedIntelligence() { return scenario.hasIntelligence ? &intelligence : nullptr; }
    void SetStoredPointer(void* owner) { stored = owner; }
    void* GetStoredPointer() const { return stored; }
    unsigned long GetModelIndex() const { return model; }
    void SetModelIndex(unsigned long value) { model = value; }
    void SetLanding(bool) {}
    void SetFightingStyle(int, int) {}
    void SetMoveAnim(int) {}
    void SetInitialState() { ++scenario.nativeResets; }
};
enum { BLOCKING };
struct CModelInfo
{
    void MakeCustomModel() {}
    void ModelAddRef(int, const char*)
    {
        if (scenario.onAcquire) scenario.onAcquire();
        ++scenario.references;
    }
    void RemoveRef()
    {
        Expect(scenario.references > 0, "removed an unowned model reference");
        --scenario.references;
    }
    bool GetRwObject() const
    {
        if (scenario.throwRender) throw std::runtime_error("render lookup refused");
        return scenario.model;
    }
};
static CPlayerPed native, other;
static CPed plainPed;
struct Row { CPed* pEntity = &native; CClientPed* pClientEntity = nullptr; };
struct Pools
{
    Row row;
    CPed* handle = &native;
    CPed* AddPed(CClientPed* owner, unsigned long)
    {
        ++scenario.remoteAdds;
        if (scenario.throwAdd) throw std::runtime_error("pool refused");
        row.pEntity = handle;
        row.pClientEntity = handle ? owner : nullptr;
        return handle;
    }
    void RemovePed(CPed* ped)
    {
        Expect(ped && ped == row.pEntity && row.pClientEntity, "removed a foreign or unregistered ped");
        ++scenario.remoteRemoves;
        row = {nullptr, nullptr};
    }
    Row* GetPed(size_t slot)
    {
        Expect(slot == 0, "local registry slot must be zero");
        ++scenario.pedReads;
        return &row;
    }
    CPed* GetPedFromRef(DWORD handleId)
    {
        Expect(handleId == 1, "local game handle must be one");
        ++scenario.pedReads;
        return handle;
    }
    void InvalidateLocalPlayerClientEntity()
    {
        row.pClientEntity = nullptr;
        ++scenario.invalidations;
    }
    CClientPed* GetPedClientEntity(CPed* wrapper) { return row.pEntity == wrapper ? row.pClientEntity : nullptr; }
} pools, replacement;
using CPools = Pools;
static Pools* activePools = &pools;

struct GameServices
{
    CModelInfo defaultModel;
    CModelInfo* GetModelInfo(int) { return &defaultModel; }
    Pools* GetPools() { return scenario.pools ? activePools : nullptr; }
    bool InitLocalPlayer(CClientPed* owner)
    {
        ++scenario.registrations;
        if (scenario.init && scenario.associate)
            activePools->row.pClientEntity = owner;
        if (!scenario.matchingSlot)
            activePools->row.pEntity = &other;
        return scenario.init;
    }
    GameServices* GetHud() { return this; }
    GameServices* GetBuildingRemoval() { return this; }
    GameServices* GetWorld() { return this; }
    GameServices* GetStats() { return this; }
    GameServices* Get3DMarkers() { return this; }
    GameServices* GetPlayerInfo() { return this; }
    GameServices* GetWanted() { return this; }
    GameServices* GetWaterManager() { return this; }
    GameServices* GetVehicleAudioSettingsManager() { return this; }
    void Pause(bool) {}
    void SetComponentVisible(int, bool) {}
    void DeleteAndDisableGangTags() {}
    void AddRemoteDataStorage(CPlayerPed* ped, void*) { Expect(ped->stored != nullptr, "storage before association"); ++scenario.storageAdds; }
    void ClearRemovedBuildingLists() {}
    void SetOcclusionsEnabled(bool) {}
    void ResetModelLodDistances() {}
    void ResetModelFlags() {}
    void ResetAlphaTransparencies() {}
    void ResetModelTimes() {}
    void SetWeaponRenderEnabled(bool) {}
    void ModifyStat(int, double) {}
    template<typename... Types> void CreateMarker(Types...) {}
    void SetWantedLevel(int) {}
    void Reset() {}
    void SetWaterDrawnLast(bool) {}
    void SetDoesNotGetTired(bool) {}
    void ResetAudioSettingsData() {}
    void Initialize() { ++scenario.initialized; }
} gameServices;
static auto* g_pGame = &gameServices;
static auto* g_pMultiplayer = &gameServices;
using CHud = GameServices;
enum { HUD_HELP_TEXT, HUD_VITAL_STATS, HUD_AREA_NAME, HUD_ALL, CITIES_PASSED,
       SERVER_TYPE_EDITOR, PLAYER_INGAME_NOTICE, MB_BUTTON_OK, MB_ICON_ERROR };
enum class T3DMarkerType { MARKER3D_CONE };
enum class eHudComponentProperty { ALL_PROPERTIES };
struct CLuaPlayerDefs { static void ResetPlayerHudComponentProperty(int, eHudComponentProperty) {} };

template<typename T> bool ListContains(const std::vector<T>& items, T value)
{
    return std::find(items.begin(), items.end(), value) != items.end();
}
template<typename T> void ListRemove(std::vector<T>& items, T value)
{
    items.erase(std::remove(items.begin(), items.end(), value), items.end());
}
struct CClientPedManager
{
    std::vector<CClientPed*> m_StreamedIn;
    void OnCreation(CClientPed*);
    void OnDestruction(CClientPed*);
} pedManager;
struct ClientServices
{
    bool IsGameLoaded() const { return scenario.gameLoaded; }
    bool IsConnected() const { return scenario.connected; }
    GameServices* GetGame() { return g_pGame; }
    ClientServices* GetModelManager() { return this; }
    ClientServices* GetIMGManager() { return this; }
    ClientServices* GetModManager() { return this; }
    CClientPedManager* GetPedManager() { return &pedManager; }
    void InvalidateEntity(CClientPed*) {}
    bool RequestForStreamIn(unsigned short, CClientPed*) { return scenario.requested; }
    int GetDimension() const { return 0; }
    void RestoreEntity(CClientPed*) { ++scenario.restored; }
    void RemoveAll() {}
    void DoPulse() {}
    void ResetCameraClip() {}
    void InitDefaultBufferSize() { ++scenario.buffers; }
    int GetHookedWindow() { return 0; }
    void ShowMessageBox(const std::string&, const std::string&, int)
    {
        ++scenario.dialogs;
        if (scenario.onDialog) scenario.onDialog();
    }
    void RequestUnload() { ++scenario.unloads; }
    void RPC(int type)
    {
        Expect(type == PLAYER_INGAME_NOTICE, "unexpected RPC");
        ++scenario.notices;
        if (scenario.onNotice) scenario.onNotice();
    }
} clientServices;
static auto* g_pCore = &clientServices;
static std::string _(const char* message) { return message; }
static std::string _E(const char* code) { return code; }
static void SetActiveWindow(int) { if (scenario.onFocus) scenario.onFocus(); }
static void SetFocus(int) {}
static void WatchDogCompletedSection(const char*) { ++scenario.watchdog; }
static void SetApplicationSettingInt(const char*, int) {}
static int GetApplicationSettingInt(const char*) { return 0; }
struct CClientTime { static int GetTime() { return 1; } };
struct CLuaArguments {};

struct CClientStreamElement
{
    virtual ~CClientStreamElement() = default;
    virtual void StreamIn(bool) = 0;
    virtual void StreamOut() = 0;
    bool m_bStreamedIn = false, m_bAttemptingToStreamIn = false;
    std::uint32_t m_lastStreamOutTime{};
    std::vector<CClientStreamElement*> m_AttachedEntities;
    void InternalStreamIn(bool);
    void InternalStreamOut();
    void NotifyUnableToCreate();
    void CallEvent(const char* event, CLuaArguments&, bool)
    {
        Expect(std::string(event) == "onClientElementStreamOut", "unexpected event");
        ++scenario.streamOutEvents;
    }
    void NotifyCreate()
    {
        ++scenario.streamEvents;
        m_bStreamedIn = true;
        m_bAttemptingToStreamIn = false;
        if (scenario.onStream) scenario.onStream();
        if (scenario.withdrawStream) m_bStreamedIn = false;
    }
};
using uint = unsigned int;
struct CClientObject : CClientStreamElement { bool IsLowLod() const { return false; } };
template<typename T> T* DynamicCast(CClientStreamElement* e) { return dynamic_cast<T*>(e); }
struct CVehicle {};
struct CClientVehicle
{
    CVehicle* GetGameVehicle() { return nullptr; }
    void RemoveStreamReference()
    {
        ++scenario.vehicleReleases;
        if (scenario.onVehicleRelease) scenario.onVehicleRelease();
    }
};
struct CStaticFunctionDefinitions
{
    template<typename... Types> static void SetPedAnimation(CClientPed&, Types...) { ++scenario.animations; }
};
struct CClientPed : CClientStreamElement
{
    CPlayerPed* m_pPlayerPed = nullptr;
    Tasks* m_pTaskManager = nullptr;
    CModelInfo model;
    CModelInfo* m_pModelInfo = &model;
    CModelInfo* m_pLoadedModelInfo = nullptr;
    std::shared_ptr<int> m_clientModel, m_loadedClientModel;
    ClientServices* m_pManager = &clientServices;
    ClientServices* m_pRequester = &clientServices;
    bool m_bIsLocalPlayer = true, m_bHasJetPack = false;
    bool m_shouldRecreate = false;
    void* m_remoteDataStorage = nullptr;
    ClientServices* m_pStreamer = &clientServices;
    CClientVehicle* m_pOccupyingVehicle = nullptr;
    int m_FightingStyle = 0, m_MoveAnim = 0;
    unsigned long m_ulModel = 0;
    void _CreateLocalModel();
    void _DestroyLocalModel();
    void ReleaseLocalModel();
    bool HasLocalModel() const;
    void BeginDestruction();
    void StreamIn(bool) override;
    void StreamOut() override { if (m_pPlayerPed) _DestroyModel(); }
    void ReCreateGameEntity();
    void ModelRequestCallback(CModelInfo*);
    bool IsVisibleInAllDimensions() const { return false; }
    int GetDimension() const { return 0; }
    void _ChangeModel() { throw std::logic_error("unexpected change to an existing model"); }
    void NotifyCreate();
    CClientVehicle* GetRealOccupiedVehicle() { return nullptr; }
    CClientVehicle* GetOccupiedVehicle() { return nullptr; }
    void InternalRemoveFromVehicle(CVehicle*) {}
    void _CreateModel();
    void _DestroyModel()
    {
        pools.RemovePed(m_pPlayerPed);
        m_pPlayerPed->SetStoredPointer(nullptr);
        m_pPlayerPed = nullptr;
        m_pTaskManager = nullptr;
        ReleaseLoadedModel();
        pedManager.OnDestruction(this);
    }
    bool AcquireRequestedModel(const char*);
    void ReleaseLoadedModel();
    void SetHasJetPack(bool) {}
    void RebuildModel() {}
    CPlayerPed* GetGamePlayer() { return m_pPlayerPed; }
    bool IsStreamedIn() const { return m_bStreamedIn; }
    void NotifyUnableToCreate() { CClientStreamElement::NotifyUnableToCreate(); ++scenario.attemptsRefused; }
};

struct CClientPlayer : CClientPed
{
    CClientPlayer(ClientServices*, int, bool) { ++scenario.created; _CreateLocalModel(); }
    ~CClientPlayer()
    {
        BeginDestruction();
        if (m_pPlayerPed) _DestroyLocalModel();
        ++scenario.deleted;
    }
    void SetParent(void*) {}
    void SetNick(const std::string&) {}
    void SetHealth(int) {}
    void SetPosition(CVector) {}
    void SetFrozen(bool) {}
    void ResetInterpolation() {}
    void ResetStats() {}
};
struct CClientGame
{
    ClientServices* m_pManager = &clientServices;
    ClientServices* m_pBlendedWeather = &clientServices;
    ClientServices* m_pCamera = &clientServices;
    ClientServices* m_pNetAPI = &clientServices;
    CClientPlayer* m_pLocalPlayer = nullptr;
    void* m_pRootEntity = nullptr;
    std::string m_strLocalNick = "local";
    int m_LocalID = 0, m_ServerType = 0, m_ulVerifyTimeStart = 0;
    bool m_bWaitingForLocalConnect = false, m_bTriggeredIngameAndConnected = false;
#include "startup-state.inl"
#include "declaration.inl"
    void Event_OnIngameAndConnected();
    void DoPulses();
    void ResetMapInfo() { ++scenario.resets; }
    ~CClientGame() { delete m_pLocalPlayer; }
};
struct CPacketHandler
{
    int accepted = 0, refused = 0;
    void RaiseProtocolError(int code) { Expect(code == 2, "unexpected join error"); ++refused; }
    void EnterJoined();
};

#include "production.inl"

static void CheckPed(const std::string& name)
{
    CClientPed ped, previous;
    pools.row.pClientEntity = &previous;
    native.stored = &previous;
    if (name.rfind("cleanup-", 0) == 0)
    {
        ped._CreateLocalModel();
        Expect(ped.GetGamePlayer() == &native && ListContains(pedManager.m_StreamedIn, &ped), "setup player not registered");
        native.model = 17;
        CClientVehicle vehicle;
        auto transfer = [&]
        {
            pools.row.pClientEntity = &previous;
            native.stored = &previous;
        };
        const bool lostBeforeDestruction = name == "cleanup-reassociated" || name == "cleanup-missing-pool";
        if (lostBeforeDestruction)
        {
            transfer();
            scenario.pools = name != "cleanup-missing-pool";
        }
        const bool fromCallback = name == "cleanup-after-vehicle-callback";
        if (fromCallback)
        {
            ped.m_pOccupyingVehicle = &vehicle;
            scenario.onVehicleRelease = transfer;
        }
        ped.BeginDestruction();
        if (ped.GetGamePlayer()) ped._DestroyLocalModel();
        const bool transferred = lostBeforeDestruction || fromCallback;
        Expect(!ListContains(pedManager.m_StreamedIn, &ped), "destroyed local player left in streamed list");
        Expect(!ped.GetGamePlayer() && !ped.m_pTaskManager && !scenario.references, "teardown retained model or tasks");
        Expect(native.stored == (transferred ? &previous : nullptr), "teardown changed another stored owner or retained this");
        Expect(native.model == (transferred ? 17ul : 0ul) && scenario.nativeResets == int(!transferred), "teardown reset a foreign ped");
        Expect(scenario.animations == int(!lostBeforeDestruction) && scenario.vehicleReleases == int(fromCallback),
               "teardown reentered or animated a stale native ped");
        Expect(pools.row.pClientEntity == (transferred ? &previous : nullptr) && scenario.invalidations == int(!transferred),
               "teardown invalidated the wrong association");
        return;
    }
    if (name == "foreign-stream-in")
    {
        ped._CreateLocalModel();
        ped.m_bStreamedIn = false;
        pools.row.pClientEntity = &previous;
        ped.StreamIn(false);
        Expect(!ped.IsStreamedIn() && scenario.streamEvents == 1 && scenario.attemptsRefused == 1, "foreign resident wrapper streamed in");
        return;
    }
    if (name == "early-stream-in" || name == "ped-only-stream-in" || name == "model-only-stream-in")
    {
        if (name == "ped-only-stream-in") ped.m_pPlayerPed = &native;
        if (name == "model-only-stream-in") ped.m_pLoadedModelInfo = &ped.model;
        ped.StreamIn(true);
        Expect(!ped.IsStreamedIn() && !scenario.streamEvents && scenario.attemptsRefused == 1, "false local stream-in");
        return;
    }
    scenario.init = name != "init-refusal";
    scenario.pools = name != "missing-pools";
    scenario.model = name != "model-refusal";
    scenario.associate = name != "foreign-owner";
    scenario.matchingSlot = name != "foreign-slot";
    scenario.hasIntelligence = name != "missing-intelligence";
    scenario.hasTasks = name != "missing-tasks";
    if (name == "missing-ped") pools.handle = nullptr;
    if (name == "wrong-ped") pools.handle = &plainPed;
    if (name == "pool-replaced")
    {
        replacement.row.pEntity = &other;
        replacement.handle = &other;
        scenario.onAcquire = [] { activePools = &replacement; };
        ped._CreateLocalModel();
        Expect(ped.GetGamePlayer() == &other && replacement.row.pClientEntity == &ped && other.stored == &ped && ped.IsStreamedIn(),
               "registration must use the pool service active after blocking load");
        Expect(pools.row.pClientEntity == &previous && native.stored == &previous && native.model == 16,
               "blocking load retained a stale pool");
        return;
    }
    ped._CreateLocalModel();
    if (name == "success")
    {
        Expect(ped.GetGamePlayer() == &native && ped.m_pTaskManager == &native.intelligence.tasks, "native identity missing");
        Expect(ped.IsStreamedIn() && native.stored == &ped && native.model == 0 && pools.row.pClientEntity == &ped, "wrong player published");
        Expect(scenario.references == 1 && scenario.restored == 1 && scenario.streamEvents == 1 && !scenario.attemptsRefused,
               "local success must own its model and announce creation once");
        ped.m_bStreamedIn = false;  // The streamer can clear this flag while the local model stays resident.
        ped.StreamIn(false);
        Expect(ped.IsStreamedIn() && scenario.streamEvents == 2 && scenario.registrations == 1 && scenario.references == 1,
               "resident local restream must reuse its current model");
        return;
    }
    Expect(!ped.GetGamePlayer() && !ped.m_pTaskManager && !ped.m_pLoadedModelInfo && !ped.IsStreamedIn(), "published partial player");
    Expect(!scenario.references && !scenario.streamEvents && !scenario.restored && scenario.attemptsRefused == 1, "refused player notified success or leaked its model");
    Expect(native.stored == &previous && native.model == 16, "modified an unadmitted wrapper");
    const bool reachedRegistration = scenario.pools && scenario.model;
    const bool reachedLookup = reachedRegistration && scenario.init;
    Expect(scenario.registrations == int(reachedRegistration) && !!scenario.pedReads == reachedLookup, "unexpected pool registration or lookup");
    const bool ownedAssociation = reachedLookup && scenario.associate;
    Expect(scenario.invalidations == int(ownedAssociation), "wrong association invalidated");
    Expect(pools.row.pClientEntity == (ownedAssociation ? nullptr : &previous), "pool contains a stale or overwritten owner");
    ped.StreamIn(false);
    Expect(!ped.IsStreamedIn() && !scenario.streamEvents && scenario.attemptsRefused == 2, "streamer published a refused native player");
}

static void CheckGame(const std::string& name)
{
    CClientGame game;
    g_pClientGame = &game;
    CPacketHandler gate;
    CClientPed replacementOwner;
    gate.EnterJoined();
    Expect(!gate.accepted && gate.refused == 1, "join accepted before startup");
    bool entered = false;
    auto duringStartup = [&]
    {
        if (entered) return;
        entered = true;
        gate.EnterJoined();
        game.DoPulses();
        Expect(!gate.accepted && !scenario.notices && !scenario.initialized && !scenario.buffers, "reentrant startup published readiness");
    };
    if (name == "reentrant-success") scenario.onStream = duringStartup;
    if (name == "reentrant-refusal") scenario.onAcquire = duringStartup;
    if (name == "reentrant-focus") scenario.onFocus = duringStartup;
    scenario.withdrawStream = name == "withdrawn-stream-in";
    const bool transferred = name == "transferred-during-stream";
    if (transferred)
    {
        scenario.onStream = [&] { pools.row.pClientEntity = &replacementOwner; native.stored = &replacementOwner; };
    }
    const bool lostHandle = name == "lost-handle-during-stream";
    if (lostHandle)
        scenario.onStream = [] { pools.handle = nullptr; native.model = 17; };
    const bool refuse = name == "repeated-refusal" || name == "reentrant-refusal" || scenario.withdrawStream || transferred || lostHandle;
    if (refuse)
    {
        scenario.init = scenario.withdrawStream || transferred || lostHandle;
        scenario.onDialog = [&] { gate.EnterJoined(); game.DoPulses(); };
    }
    scenario.connected = name != "offline-then-online";
    game.m_bWaitingForLocalConnect = name == "server-starting";
    scenario.gameLoaded = name != "waiting-for-game";
    scenario.onNotice = [&]
    {
        Expect(game.m_pLocalPlayer && game.m_pLocalPlayer->IsStreamedIn() && scenario.watchdog == 1, "notice before local player readiness");
        game.DoPulses();
    };
    game.DoPulses();
    game.DoPulses();
    if (refuse)
    {
        gate.EnterJoined();
        Expect(!gate.accepted && !game.m_pLocalPlayer && !scenario.watchdog && !scenario.notices && !scenario.initialized && !scenario.buffers,
               "failed local startup advanced the session");
        Expect(scenario.created == 1 && scenario.deleted == 1 && scenario.dialogs == 1 && scenario.unloads == 1 && scenario.resets == 1,
               "failed startup must dispose the candidate and request unload once");
        const auto* expectedOwner = transferred ? &replacementOwner : nullptr;
        Expect(pedManager.m_StreamedIn.empty() && !scenario.references && pools.row.pClientEntity == expectedOwner && native.stored == expectedOwner,
               "failed candidate left a reference in a live service");
        if (lostHandle)
            Expect(native.model == 17 && !scenario.nativeResets && !scenario.animations && scenario.invalidations == 1,
                   "handle loss must release the CPU registration without resetting the stale native ped");
        if (name == "reentrant-refusal") Expect(entered, "model callback did not run");
        return;
    }
    if (!scenario.gameLoaded)
    {
        Expect(!scenario.created && !scenario.notices && !scenario.watchdog, "startup advanced before game load");
        scenario.gameLoaded = true;
        game.DoPulses();
    }
    if (!scenario.connected || game.m_bWaitingForLocalConnect)
    {
        Expect(game.m_pLocalPlayer && scenario.watchdog == 1 && !scenario.notices && !scenario.initialized && !scenario.buffers,
               "offline local readiness emitted an online notice");
        scenario.connected = true;
        game.m_bWaitingForLocalConnect = false;
        game.DoPulses();
    }
    game.DoPulses();
    gate.EnterJoined();
    Expect(scenario.resets == 1 && scenario.created == 1 && !scenario.deleted && !scenario.unloads && !scenario.dialogs, "successful startup repeated");
    Expect(gate.accepted == 1 && scenario.notices == 1 && scenario.initialized == 1 && scenario.buffers == 1,
           "connected event, game initialization and buffer setup must run once");
    if (name == "reentrant-success" || name == "reentrant-focus") Expect(entered, "startup callback did not run");
}

static void CheckRemotePed(const std::string& name)
{
    CClientPed ped;
    ped.m_bIsLocalPlayer = false;
    pools.row = {nullptr, nullptr};
    auto checkReady = [&]
    {
        Expect(ped.IsStreamedIn() && !ped.m_bAttemptingToStreamIn && ped.GetGamePlayer() == &native, "remote ped did not stream in");
        Expect(ped.m_pTaskManager == &native.intelligence.tasks && native.stored == &ped, "incomplete remote association");
        Expect(scenario.references == 1 && pedManager.m_StreamedIn.size() == 1, "remote stream ref or registry entry missing");
    };

    const bool recreate = name == "remote-recreate-failure" || name == "remote-recreate-model-refusal" ||
                          name == "remote-recreate-throw-render" || name == "remote-reentrant-recreate";
    if (recreate)
    {
        ped.InternalStreamIn(false);
        checkReady();
        ped.m_shouldRecreate = true;
        if (name == "remote-reentrant-recreate")
        {
            scenario.onStream = [&] { ped.m_shouldRecreate = true; };
            ped.ReCreateGameEntity();
            checkReady();
            Expect(ped.m_shouldRecreate, "lost recreation requested by the new ped's stream-in callback");
            return;
        }
    }

    scenario.pools = name != "remote-missing-pool";
    scenario.model = name != "remote-model-refusal" && name != "remote-recreate-model-refusal";
    scenario.throwAdd = name == "remote-throw-add";
    scenario.throwTasks = name == "remote-throw-tasks";
    scenario.throwRender = name == "remote-throw-render" || name == "remote-recreate-throw-render";
    if (name == "remote-missing-ped" || name == "remote-recreate-failure") pools.handle = nullptr;
    if (name == "remote-wrong-ped") pools.handle = &plainPed;
    scenario.hasIntelligence = name != "remote-missing-intelligence";
    scenario.hasTasks = name != "remote-missing-tasks";
    const bool deferred = name == "remote-missing-intelligence" || name == "remote-wrong-ped" || name == "remote-throw-render";
    scenario.requested = !deferred;
    bool threw = false;
    try
    {
        if (recreate)
            ped.ReCreateGameEntity();
        else
        {
            ped.InternalStreamIn(false);
            if (deferred)
                ped.ModelRequestCallback(ped.m_pModelInfo);
        }
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }
    Expect(threw == (scenario.throwAdd || scenario.throwTasks || scenario.throwRender), "did not preserve the SDK error");
    if (name == "remote-success")
    {
        checkReady();
        Expect(scenario.storageAdds == 1 && scenario.streamEvents == 1 && scenario.restored == 1, "stream publication count");
        return;
    }
    Expect(!ped.IsStreamedIn() && !ped.m_bAttemptingToStreamIn && !ped.GetGamePlayer() && !ped.m_pTaskManager, "failed admission left a live ped");
    Expect(!scenario.references && !pools.row.pClientEntity && !native.stored && pedManager.m_StreamedIn.empty(), "leaked failed ped or model reference");
    Expect(scenario.streamOutEvents == int(recreate) && scenario.storageAdds == int(recreate) &&
           scenario.streamEvents == int(recreate) && scenario.restored == int(recreate), "failed candidate was published or wrong stream-out count");
    scenario.pools = scenario.model = scenario.hasIntelligence = scenario.hasTasks = scenario.requested = true;
    scenario.throwAdd = scenario.throwTasks = scenario.throwRender = false;
    pools.handle = &native;
    ped.InternalStreamIn(false);
    checkReady();
    Expect(scenario.streamEvents == 1 + int(recreate) && scenario.storageAdds == 1 + int(recreate), "retry did not publish exactly once");
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) return 2;
        const std::string name = argv[1];
        if (name.rfind("remote-", 0) == 0)
            CheckRemotePed(name);
        else if (name == "online" || name == "offline-then-online" || name == "server-starting" || name == "reentrant-success" ||
            name == "reentrant-focus" || name == "reentrant-refusal" || name == "repeated-refusal" || name == "withdrawn-stream-in" ||
            name == "transferred-during-stream" || name == "lost-handle-during-stream" || name == "waiting-for-game")
            CheckGame(name);
        else
            CheckPed(name);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
