// Production entity setters and streaming callbacks run against a controlled model
// registry. A retired registration may die only after native refs are released.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>
static int  checks;
static void Check(bool ok, const char* why)
{
    ++checks;
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
constexpr int BLOCKING = 1;
struct CModelInfo
{
    bool ready = true;
    int  refs = 0;
    void ModelAddRef(int, const char*) { ++refs; }
    void RemoveRef()
    {
        Check(refs > 0, "balanced native model ref");
        --refs;
    }
    void* GetRwObject() { return ready ? this : nullptr; }
    bool  IsLoaded() const { return ready; }
    void  Request(int, const char*) {}
    void  MakeCustomModel() {}
    int   GetParentID() const { return 400; }
    void  GetVoice(short* a, short* b) { *a = *b = 0; }
};
struct CClientModel
{
    int         id;
    CModelInfo& native;
    int         GetModelID() const { return id; }
    ~CClientModel() { Check(native.refs == 0, "last client owner outlives every native reference"); }
};
using Handle = std::shared_ptr<CClientModel>;
struct Handling
{
    void      Assign(Handling*) {}
    Handling* GetOriginalHandlingData(unsigned) { return this; }
    Handling* GetOriginalFlyingHandlingData(unsigned) { return this; }
    Handling* GetOriginalBoatHandlingData(unsigned) { return nullptr; }
    Handling* GetOriginalBikeHandlingData(unsigned) { return nullptr; }
    Handling* CreateBikeHandlingData() { return this; }
    Handling* CreateBoatHandlingData() { return this; }
};
struct Game
{
    std::map<unsigned long, CModelInfo> models;
    Handling                            handling;
    CModelInfo*                         GetModelInfo(unsigned long id) { return &models[id]; }
    Handling*                           GetHandlingManager() { return &handling; }
} game;
auto* g_pGame = &game;
struct CVector
{
};
struct CEntitySAInterface
{
};
struct SSatchelsData
{
    void*    pProjectileInterface;
    CVector *vecAttachedOffsets, *vecAttachedRotation;
};
struct CClientProjectile
{
    bool IsBeingDeleted() const { return false; }
    void SetAttachedOffsets(CVector, CVector) {}
    void InternalAttachTo(void*) {}
};
struct CClientProjectileManager
{
    CClientProjectile* Get(CEntitySAInterface*) { return nullptr; }
};
struct Manager
{
    std::map<unsigned long, Handle> active;
    CClientProjectileManager        projectiles;
    Manager*                        GetModelManager() { return this; }
    CClientProjectileManager*       GetProjectileManager() { return &projectiles; }
    Handle                          FindModelByID(unsigned long id)
    {
        auto i = active.find(id);
        return i == active.end() ? nullptr : i->second;
    }
    std::weak_ptr<CClientModel> Register(int id) { return active[id] = Handle(new CClientModel{id, *game.GetModelInfo(id)}); }
} manager;
struct Requester
{
    bool        ready = true;
    CModelInfo* pending = nullptr;
    bool        Request(unsigned short id, void*)
    {
        pending = ready ? nullptr : game.GetModelInfo(id);
        return ready;
    }
    bool RequestForStreamIn(unsigned short id, void* entity) { return Request(id, entity); }
    bool RequestBlocking(unsigned short, const char*) const { return ready; }
} requester;
struct Streamer
{
    int GetDimension() { return 0; }
} streamer;
struct EntityStreaming
{
    Streamer*                   m_pStreamer = &streamer;
    Requester*                  m_pModelRequester = &requester;
    bool                        streamedIn = true;
    int                         dimension = 0;
    std::vector<unsigned short> created;
    bool                        IsStreamedIn() const { return streamedIn; }
    bool                        IsVisibleInAllDimensions() const { return false; }
    int                         GetDimension() const { return dimension; }
    void                        NotifyUnableToCreate() {}
};
struct CClientObjectManager
{
    static bool IsValidModel(unsigned short id) { return id < 20000; }
};
struct CClientPlayerManager
{
    static bool IsValidModel(unsigned long id) { return id < 20000; }
};
struct CClientObject : EntityStreaming
{
    Manager*              m_pManager = &manager;
    Handle                m_clientModel;
    unsigned short        m_usModel = 400;
    CModelInfo*           m_pModelInfo = game.GetModelInfo(400);
    std::function<void()> onDestroy;
    bool                  m_bRespawnEnabled = true;
    float                 m_fHealth = 1000.0f;
    void                  Create() { created.push_back(m_usModel); }
    void                  UpdateSpatialData() {}
    void                  Destroy()
    {
        if (onDestroy)
            onDestroy();
    }
    void SetModel(unsigned short);
    void StreamIn(bool);
    void ModelRequestCallback(CModelInfo*);
};
using eClientVehicleType = int;
constexpr int CLIENTVEHICLE_TRAIN = 1;
struct CClientVehicleManager
{
    static bool HasDoors(unsigned short) { return true; }
    static int  GetVehicleType(unsigned short) { return 0; }
    static bool IsStandardModel(unsigned short) { return false; }
    static bool HasDamageModel(int) { return true; }
    static bool HasAdjustableProperty(unsigned short) { return false; }
    static int  GetMaxPassengerCount(unsigned short) { return 1; }
    static void GetRandomVariation(unsigned short, unsigned char&, unsigned char&) {}
};
struct Upgrades
{
    void RemoveAll(bool) {}
};
struct CClientVehicle : EntityStreaming
{
    Manager*           m_pManager = &manager;
    Handle             m_clientModel;
    unsigned short     m_usModel = 400;
    CModelInfo*        m_pModelInfo = game.GetModelInfo(400);
    CClientVehicle *   m_pVehicle = this, *m_pNextLink = nullptr, *m_pPreviousLink = nullptr;
    Upgrades*          m_pUpgrades = nullptr;
    eClientVehicleType m_eVehicleType = 0;
    unsigned char      m_ucVariation{}, m_ucVariation2{}, m_ucDoorStates[4]{}, m_ucWheelStates[4]{};
    bool               m_bHasDamageModel{}, m_bHasLandingGear{}, m_bHasAdjustableProperty{}, m_copyDummyPositions{};
    int                m_usAdjustablePropertyValue{}, m_ucMaxPassengers{}, m_dummyPositions{};
    struct Component
    {
        bool m_bVisible;
    };
    std::map<int, Component> m_ComponentData;
    std::map<int, bool>      m_ComponentVisibilityBackup;
    Handling *               m_pOriginalHandlingEntry = nullptr, *m_pOriginalFlyingHandlingEntry = nullptr, *m_pOriginalBoatHandlingEntry = nullptr,
             *m_pOriginalBikeHandlingEntry = nullptr;
    Handling *m_HandlingEntry = &game.handling, *m_FlyingHandlingEntry = &game.handling, *m_BoatHandlingEntry = nullptr, *m_BikeHandlingEntry = nullptr;
    std::function<void()> onDestroy;
    void                  Destroy()
    {
        if (onDestroy)
            onDestroy();
        m_pVehicle = nullptr;
    }
    bool failCreate = false;
    void Create()
    {
        if (!m_pVehicle)
        {
            created.push_back(m_usModel);
            m_pVehicle = failCreate ? nullptr : this;
        }
    }
    bool  IsLocalEntity() { return false; }
    void  GetInitialDoorStates(unsigned char*) {}
    bool  DoCheckHasLandingGear() { return false; }
    void  SetPreviousTrainCarriage(void*) {}
    void  SetNextTrainCarriage(void*) {}
    void  ApplyHandling() {}
    void  SetSirenOrAlarmActive(bool) {}
    void  GetMoveSpeed(CVector&) {}
    void  GetTurnSpeed(CVector&) {}
    float GetTrainSpeed() { return 0; }
    void  SetMoveSpeed(CVector) {}
    void  SetTurnSpeed(CVector) {}
    void  SetTrainSpeed(float) {}
    void  SetModelBlocking(unsigned short, unsigned char, unsigned char);
    void  StreamIn(bool);
    void  ModelRequestCallback(CModelInfo*);
};
struct Player
{
    unsigned long         id = 400;
    std::function<void()> onSwitch;
    unsigned long         GetModelIndex() const { return id; }
    void                  SetModelIndex(unsigned long value)
    {
        if (onSwitch)
            onSwitch();
        id = value;
    }
    void GetAttachedSatchels(std::vector<SSatchelsData>&) {}
    void GetFightingStyle() {}
    void RebuildPlayer() {}
};
struct Multiplayer
{
    void SetAutomaticVehicleStartupOnPedEnter(bool) {}
} multiplayer;
auto* g_pMultiplayer = &multiplayer;
struct Clothes
{
    void AddAllToModel() {}
} clothes;
struct CClientPed
{
    Manager*      m_pManager = &manager;
    Requester*    m_pRequester = &requester;
    Player        actor;
    Player*       m_pPlayerPed = &actor;
    Clothes*      m_pClothes = &clothes;
    Streamer*     m_pStreamer = &streamer;
    Handle        m_clientModel, m_loadedClientModel, m_storedClientModel;
    unsigned long m_ulModel = 400, m_ulStoredModel = 0;
    bool m_bIsLocalPlayer = true, m_bisCurrentAnimationCustom = false, m_bisNextAnimationCustom = false, m_bDontChangeRadio = false, m_shouldRecreate = false;
    bool m_pAnimationBlock = false;
    int  m_ucInterior = 0;
    CModelInfo *    m_pModelInfo = game.GetModelInfo(400), *m_pLoadedModelInfo = nullptr;
    void            UpdateSpatialData() {}
    void            SetStat(int, float) {}
    void            Respawn(void*, bool, bool) {}
    void            SetVoice(short, short) {}
    void            RunAnimationFromCache() {}
    bool            IsAnimationInProgress() { return false; }
    CClientVehicle* GetOccupiedVehicle() { return nullptr; }
    unsigned        GetOccupiedVehicleSeat() { return 0; }
    bool            IsLeavingVehicle() { return false; }
    void            RemoveFromVehicle() {}
    void            WarpIntoVehicle(CClientVehicle*, unsigned) {}
    bool            IsVisibleInAllDimensions() { return true; }
    int             GetDimension() { return 0; }
    void            NotifyUnableToCreate() {}
    void            _DestroyModel()
    {
        ReleaseLoadedModel();
        m_pPlayerPed = nullptr;
    }
    void _CreateModel()
    {
        if (AcquireRequestedModel("fixture create"))
        {
            m_pPlayerPed = &actor;
            actor.id = m_ulModel;
        }
    }
    bool AcquireRequestedModel(const char*);
    void ReleaseLoadedModel();
    void RestoreTemporaryModel();
    bool SetModel(unsigned long, bool = false);
    void _ChangeModel();
    void ModelRequestCallback(CModelInfo*);
    void ReCreateGameEntity();
};
#include "EntityModelOwnership.generated.inl"

template <typename Entity, typename SetModel>
static void CheckStreamingCompletion(Entity& entity, SetModel setModel)
{
    entity.streamedIn = false;
    requester.ready = false;
    setModel(1000);
    entity.StreamIn(false);
    CModelInfo* completed = std::exchange(requester.pending, nullptr);
    Check(completed == game.GetModelInfo(1000), "initial asynchronous stream-in requests the old model");
    setModel(1001);
    Check(!requester.pending && entity.created.empty(), "switch while not streamed in leaves the new model unrequested");
    entity.ModelRequestCallback(completed);
    Check(entity.created.empty(), "completion of the previous request must not create the new model");
    entity.StreamIn(false);
    completed = std::exchange(requester.pending, nullptr);
    Check(completed == game.GetModelInfo(1001), "the next stream-in requests the selected model");
    entity.dimension = 1;
    entity.ModelRequestCallback(completed);
    Check(entity.created.empty(), "current model in another dimension is still refused");
    entity.dimension = 0;
    entity.StreamIn(false);
    entity.ModelRequestCallback(std::exchange(requester.pending, nullptr));
    Check(entity.created == std::vector<unsigned short>{1001}, "matching completion creates the selected model once");
}

int main(int argc, char** argv)
{
    if (argc == 2)
    {
        if (std::strcmp(argv[1], "object-completion") == 0)
        {
            CClientObject object;
            CheckStreamingCompletion(object, [&](unsigned short id) { object.SetModel(id); });
        }
        else if (std::strcmp(argv[1], "vehicle-completion") == 0)
        {
            CClientVehicle vehicle;
            vehicle.m_pVehicle = nullptr;
            CheckStreamingCompletion(vehicle, [&](unsigned short id) { vehicle.SetModelBlocking(id, 0, 0); });
        }
        else
            return 2;
        std::printf("Passed %d %s checks\n", checks, argv[1]);
        return 0;
    }
    constexpr int a = 1000, b = 1001;
    {
        CClientObject o;
        auto          old = manager.Register(a);
        o.SetModel(a);
        manager.active.erase(a);
        Check(!old.expired(), "stock object switching to a registered ID retains it");
        auto next = manager.Register(b);
        o.onDestroy = [&] { Check(!old.expired(), "old object survives Destroy"); };
        o.SetModel(b);
        manager.active.erase(b);
        Check(old.expired() && !next.expired(), "object transfers the last owner");
        o.onDestroy = {};
        o.SetModel(25000);
        Check(!next.expired(), "invalid object request preserves the owner");
        o.SetModel(b);
        Check(!next.expired(), "same-model recreation preserves the retired identity");
        o.SetModel(400);
        Check(next.expired(), "stock object releases the old registration");
    }
    {
        CClientVehicle v;
        auto           old = manager.Register(a);
        v.SetModelBlocking(a, 0, 0);
        manager.active.erase(a);
        Check(!old.expired(), "stock vehicle acquires a custom model");
        auto next = manager.Register(b);
        v.onDestroy = [&] { Check(!old.expired(), "old vehicle survives native teardown"); };
        v.SetModelBlocking(b, 0, 0);
        manager.active.erase(b);
        Check(old.expired() && !next.expired(), "blocking vehicle switch transfers ownership");
        v.onDestroy = {};
        v.SetModelBlocking(400, 0, 0);
        Check(next.expired(), "stock vehicle releases custom model");
        auto refused = manager.Register(b);
        v.failCreate = true;
        game.GetModelInfo(b)->ready = false;
        v.SetModelBlocking(b, 0, 0);
        manager.active.erase(b);
        Check(!v.m_pVehicle && !refused.expired(), "failed recreation retains selected vehicle model");
        v.SetModelBlocking(400, 0, 0);
        game.GetModelInfo(b)->ready = true;
        Check(refused.expired(), "replacing a failed vehicle selection releases its registration");
    }
    {
        CClientPed p;
        auto       old = manager.Register(a), next = manager.Register(b);
        requester.ready = true;
        p.SetModel(a);
        Check(p.actor.id == a && p.m_pLoadedModelInfo == game.GetModelInfo(a), "local custom model installed");
        requester.ready = false;
        p.SetModel(b);
        manager.active.erase(a);
        manager.active.erase(b);
        Check(!old.expired() && !next.expired() && p.actor.id == a, "pending local request retains current and requested models");
        game.GetModelInfo(b)->ready = false;
        p.ModelRequestCallback(game.GetModelInfo(b));
        Check(!old.expired() && game.GetModelInfo(a)->refs == 1 && game.GetModelInfo(b)->refs == 0, "failed local load preserves prior owner and native ref");
        game.GetModelInfo(b)->ready = true;
        p.ModelRequestCallback(game.GetModelInfo(a));
        Check(p.actor.id == a, "stale model completion is ignored");
        p.actor.onSwitch = [&] { Check(!old.expired() && !next.expired(), "old and new local models survive native SetModelIndex"); };
        p.ModelRequestCallback(game.GetModelInfo(b));
        p.actor.onSwitch = {};
        Check(p.actor.id == b && old.expired() && game.GetModelInfo(a)->refs == 0, "completed local switch releases prior allocation");
        p.ReleaseLoadedModel();
        requester.ready = true;
        p.SetModel(400);
        p.ReleaseLoadedModel();
        Check(next.expired(), "local target eventually releases final owner");
    }
    {
        CClientPed p;
        p.m_bIsLocalPlayer = false;
        p.m_pPlayerPed = nullptr;
        auto old = manager.Register(a), next = manager.Register(b);
        p.SetModel(a);
        p.ModelRequestCallback(game.GetModelInfo(a));
        requester.ready = true;
        p.SetModel(b);
        manager.active.erase(a);
        manager.active.erase(b);
        Check(p.m_shouldRecreate && !old.expired() && !next.expired() && p.actor.id == a, "remote pending recreation holds both registrations");
        p.ReCreateGameEntity();
        Check(old.expired() && !next.expired() && p.actor.id == b, "remote recreation releases old after destruction");
        p._DestroyModel();
        p.SetModel(400);
        Check(next.expired(), "streamed-out remote ped releases replaced selection");
    }
    {
        CClientPed p;
        p.m_pPlayerPed = nullptr;
        auto old = manager.Register(a);
        p.SetModel(a);
        p.SetModel(0, true);
        manager.active.erase(a);
        p.RestoreTemporaryModel();
        Check(p.m_ulModel == 0 && old.expired(), "retired temporary ped skin cannot return");
        auto previous = manager.Register(a);
        p.SetModel(a);
        p.SetModel(0, true);
        manager.active.erase(a);
        auto fresh = manager.Register(a);
        p.RestoreTemporaryModel();
        Check(p.m_ulModel == 0 && previous.expired() && !fresh.expired(), "temporary restoration rejects a reused numeric ID");
        p.SetModel(a);
        p.SetModel(0, true);
        p.SetModel(0);
        p.RestoreTemporaryModel();
        Check(p.m_ulModel == 0 && !p.m_storedClientModel, "explicit same-ID CJ supersedes temporary restoration");
        p.SetModel(a);
        p.SetModel(0, true);
        p.RestoreTemporaryModel();
        Check(p.m_ulModel == a, "temporary live registration is restored");
        manager.active.erase(a);
        p.SetModel(400);
        Check(fresh.expired(), "restored temporary registration is released");
    }
    std::printf("Passed %d entity model ownership checks\n", checks);
}
