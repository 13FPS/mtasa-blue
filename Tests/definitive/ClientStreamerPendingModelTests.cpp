// Build with test_client_streamer_pending_model.py. The grid, streamer, entity
// lifecycle, object stream methods and model request queue come from the client.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using uint = unsigned int;
using ElementID = unsigned int;
template <class T, size_t N>
using SFixedArray = std::array<T, N>;
template <class C, class T>
bool ListContains(const C& c, const T& value)
{
    return std::find(c.begin(), c.end(), value) != c.end();
}
template <class T, class U>
T* DynamicCast(U* object)
{
    return dynamic_cast<T*>(object);
}
#define OutputDebugLine(...) ((void)0)
#define DECLARE_CLASS(derived, base) \
    struct ClassInitStub \
    { \
        explicit ClassInitStub(void*) \
        { \
        } \
    } ClassInit;

struct CVector2D
{
    float fX = 0, fY = 0;
    CVector2D() = default;
    CVector2D(float x, float y) : fX(x), fY(y) {}
};
struct CVector
{
    float   fX = 0, fY = 0, fZ = 0;
    CVector operator+(const CVector& b) const { return {fX + b.fX, fY + b.fY, fZ + b.fZ}; }
    CVector operator-(const CVector& b) const { return {fX - b.fX, fY - b.fY, fZ - b.fZ}; }
    float   LengthSquared() const { return fX * fX + fY * fY + fZ * fZ; }
    bool    operator!=(const CVector& b) const { return fX != b.fX || fY != b.fY || fZ != b.fZ; }
};
struct CSphere
{
};
struct CLuaArguments
{
};
struct CElapsedTime
{
    void         SetMaxIncrement(int) {}
    void         Reset() {}
    unsigned int Get() const { return 0; }
};
struct CClientTime
{
    inline static unsigned int now = 1000;
    static unsigned int        GetTime() { return now; }
};
enum ModelRequestType
{
    BLOCKING,
    NON_BLOCKING
};
struct CModelInfo
{
    bool ready = false;
    bool failAcquire = false;
    int  references = 0, applications = 0;
    bool IsLoaded() const { return ready; }
    void MakeCustomModel() { ++applications; }
    void Request(ModelRequestType, const char*) {}
    void ModelAddRef(ModelRequestType, const char*)
    {
        if (failAcquire)
            throw std::runtime_error("SDK acquire failure");
        ++references;
    }
    void RemoveRef()
    {
        assert(references > 0);
        --references;
    }
};
struct Native
{
    float GetHealth() { return 1000; }
};
using DWORD = unsigned long;
struct Game
{
    CModelInfo  model, replacement;
    CModelInfo* GetModelInfo(unsigned short id) { return id == 1337 ? &model : &replacement; }
    bool        IsASyncLoadingEnabled() { return true; }
    bool        full = false, lowLodFull = false, allocationFails = false;
    int         objects = 0, vehicles = 0, trains = 0;
    Game*       GetPools() { return this; }
    void        AllowCreatedObjectsInVerticalLineTest(bool) {}
    Native*     Allocate(int& attempts)
    {
        ++attempts;
        return allocationFails ? nullptr : new Native;
    }
    Native* AddObject(void*, unsigned short, bool, bool) { return Allocate(objects); }
    Native* AddVehicle(void*, unsigned short, unsigned char, unsigned char) { return Allocate(vehicles); }
    Native* AddTrain(void*, CVector, const std::vector<DWORD>&, bool, unsigned char) { return Allocate(trains); }
    void    RemoveObject(Native* n) { delete n; }
    void    RemoveVehicle(Native* n) { delete n; }
} game;
auto* g_pGame = &game;
auto* g_pMultiplayer = &game;
struct CClientObjectManager
{
    static bool StaticIsObjectLimitReached() { return game.full; }
    static bool StaticIsLowLodObjectLimitReached() { return game.lowLodFull; }
    static bool IsValidModel(unsigned short) { return true; }
    static bool IsBreakableModel(unsigned short) { return false; }
};
struct CClientVehicleManager
{
    static bool IsVehicleLimitReached() { return game.full; }
    static bool IsTrainModel(unsigned short model) { return model == 537; }
};
class CClientModel
{
};
struct ModelManager
{
    std::shared_ptr<CClientModel> FindModelByID(unsigned short) { return {}; }
};
struct Camera
{
    CVector position;
    void    GetPosition(CVector& result) { result = position; }
};
class CClientEntity;
class CClientStreamElement;
#include "CClientModelRequestManager.h"
struct CClientManager
{
    Camera                     camera;
    ModelManager               models;
    CClientModelRequestManager requests;
    Camera*                    GetCamera() { return &camera; }
    ModelManager*              GetModelManager() { return &models; }
    void                       OnUpdateStreamPosition(CClientStreamElement*) {}
    void                       InvalidateEntity(CClientStreamElement*) {}
    void                       PulseModels() { requests.DoPulse(); }
} manager;
struct CClientGame
{
    bool            IsBeingDeleted() { return false; }
    CClientManager* GetManager() { return &manager; }
} clientGame;
CClientGame* g_pClientGame = &clientGame;
class CClientEntity
{
public:
    explicit CClientEntity(ElementID) {}
    virtual ~CClientEntity() = default;
    virtual void   SetDimension(unsigned short dimension) { m_usDimension = dimension; }
    virtual void   ModelRequestCallback(CModelInfo*) {}
    unsigned short GetDimension() const { return m_usDimension; }
    CClientEntity* GetAttachedTo() { return attachedTo; }
    void           UpdateSpatialData() {}
    bool           IsDoubleSided() { return false; }
    void           SetDoubleSided(bool) {}
    void           CallEvent(const std::string& name, CLuaArguments&, bool)
    {
        events.push_back(name);
        if (onEvent)
            onEvent(name);
    }
    CClientEntity*                          attachedTo = nullptr;
    std::vector<std::string>                events;
    std::function<void(const std::string&)> onEvent;

protected:
    CClientManager*             m_pManager = &manager;
    std::vector<CClientEntity*> m_AttachedEntities;
    CVector                     m_vecAttachedPosition;
    unsigned short              m_usDimension = 0;
    bool                        m_bDoubleSidedInit = false, m_bDoubleSided = false;
};
#include "CClientStreamElement.h"
#include "CClientStreamSectorRow.h"
#include "CClientStreamSector.h"
#include "CClientStreamer.h"
constexpr int PURESYNC_TYPE_LIGHTSYNC = 1;
struct CClientPlayer : CClientStreamElement
{
    int GetLastPuresyncType() { return 0; }
};
struct CClientVehicle : CClientStreamElement
{
    explicit CClientVehicle(CClientStreamer& streamer) : CClientStreamElement(&streamer, 2) {}
    ~CClientVehicle()
    {
        m_pModelRequester->Cancel(this, true);
        Destroy();
    }
    void                          Create();
    void                          Destroy();
    void                          ReCreate();
    void                          SetModelBlocking(unsigned short, unsigned char, unsigned char);
    void                          StreamIn(bool) override;
    void                          StreamOut() override;
    void                          ModelRequestCallback(CModelInfo*) override;
    void                          NotifyDestroy() {}
    Native*                       m_pVehicle = nullptr;
    CClientModelRequestManager*   m_pModelRequester = &manager.requests;
    CModelInfo*                   m_pModelInfo = &game.model;
    std::shared_ptr<CClientModel> m_clientModel;
    unsigned short                m_usModel = 1337;
    unsigned char                 m_ucVariation{}, m_ucVariation2{}, m_ucTrackID{};
    bool                          m_bTrainDirection = false;
    struct
    {
        CVector vPos;
    } m_Matrix;
    CClientPlayer*  GetOccupant() { return nullptr; }
    CClientVehicle* GetTowedByVehicle() { return nullptr; }
};
#define IS_PLAYER(entity)  (dynamic_cast<CClientPlayer*>(entity) != nullptr)
#define IS_VEHICLE(entity) (dynamic_cast<CClientVehicle*>(entity) != nullptr)

struct CClientObject : CClientStreamElement
{
    explicit CClientObject(CClientStreamer& streamer) : CClientStreamElement(&streamer, 1) {}
    ~CClientObject()
    {
        m_pModelRequester->Cancel(this, true);
        Destroy();
    }
    void                          StreamIn(bool) override;
    void                          StreamOut() override;
    void                          ModelRequestCallback(CModelInfo*) override;
    bool                          IsLowLod() { return m_bIsLowLod; }
    void                          Create();
    void                          Destroy();
    void                          ReCreate();
    void                          SetModel(unsigned short);
    void                          NotifyDestroy() {}
    std::shared_ptr<CClientModel> m_clientModel;
    bool                          m_bIsLowLod = false, m_bBreakingDisabled = false;
    Native*                       m_pObject = nullptr;
    CClientModelRequestManager*   m_pModelRequester = &manager.requests;
    CModelInfo*                   m_pModelInfo = &game.model;
    unsigned short                m_usModel = 1337;
    bool                          m_bRespawnEnabled = true;
    float                         m_fHealth = 1000;
};
#include "CClientStreamSector.cpp"
#include "CClientStreamSectorRow.cpp"
#include "CClientStreamer.cpp"
#include "CClientStreamElement.generated.inl"
#include "CClientObject.generated.inl"
#include "CClientVehicle.generated.inl"
#include "CClientModelRequestManager.cpp"

CSphere CClientStreamElement::GetWorldBoundingSphere()
{
    return {};
}
float CClientStreamElement::GetDistanceToBoundingBoxSquared(const CVector& position)
{
    return (GetStreamPosition() - position).LengthSquared();
}

static void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <class T>
static void TestRecreation(const std::string& scenario)
{
    const std::string in = "onClientElementStreamIn", out = "onClientElementStreamOut";
    const bool        initial = scenario.find("-initial-") != std::string::npos;
    const bool        delayed = scenario.find("-delayed-") != std::string::npos;
    const bool        limit = scenario.find("-limit") != std::string::npos;
    const bool        broken = scenario == "object-delayed-broken";
    const bool        successful = scenario.find("-success") != std::string::npos;
    CClientStreamer   streamer([] { return false; }, 80, 200, 200);
    manager.camera.position = {800, 0, 0};
    streamer.DoPulse(manager.camera.position);
    manager.camera.position = {};
    streamer.DoPulse(manager.camera.position);
    T entity(streamer);
    if constexpr (std::is_same_v<T, CClientObject>)
        entity.m_bIsLowLod = scenario == "object-recreate-lod-limit";
    auto hasNative = [&]
    {
        if constexpr (std::is_same_v<T, CClientObject>)
            return entity.m_pObject != nullptr;
        else
            return entity.m_pVehicle != nullptr;
    };
    game.model.ready = true;
    game.replacement.ready = !delayed;
    if (!initial)
    {
        streamer.DoPulse(manager.camera.position);
        Require(entity.IsStreamedIn() && hasNative() && game.model.references == 1 && entity.events == std::vector<std::string>{in}, "initial entity missing");
    }
    game.lowLodFull = scenario == "object-recreate-lod-limit";
    game.full = limit && !game.lowLodFull;
    game.allocationFails = !limit && !broken && !successful;
    entity.onEvent = [&](const std::string& event)
    {
        if (event == out)
            Require(!hasNative() && game.model.references == 0 && game.replacement.references == int(delayed),
                    "stream-out callback observed an unreleased native model reference");
    };
    if (initial)
        streamer.DoPulse(manager.camera.position);
    else if (scenario.find("-recreate-") != std::string::npos)
        entity.ReCreate();
    else if constexpr (std::is_same_v<T, CClientObject>)
    {
        entity.SetModel(1338);
        if (delayed)
        {
            entity.ModelRequestCallback(&game.model);
            Require(entity.IsStreamedIn() && !hasNative() && entity.events == std::vector<std::string>{in} &&
                        manager.requests.GetRequestedModelInfo(&entity) == &game.replacement,
                    "a stale completion disturbed the pending replacement");
            entity.m_bRespawnEnabled = !broken;
            entity.m_fHealth = broken ? 0.0f : 1000.0f;
            game.replacement.ready = true;
            manager.PulseModels();
        }
    }
    else
        entity.SetModelBlocking(scenario == "vehicle-train-allocation" ? 537 : 411, 0, 0);
    if (successful)
    {
        Require(entity.IsStreamedIn() && hasNative() && entity.events == std::vector<std::string>{in, in} && entity.m_pModelInfo->references == 1 &&
                    game.model.references + game.replacement.references == 1,
                "successful replacement changed its event or reference behavior");
        return;
    }
    Require(!entity.IsStreamedIn() && !entity.IsAttemptingToStreamIn() && !hasNative(), "refused creation stranded a streamed-in element");
    Require(entity.events == (initial ? std::vector<std::string>{} : std::vector<std::string>{in, out}), "wrong events after refusal");
    Require(!manager.requests.HasRequested(&entity) && !game.model.references && !game.replacement.references,
            "refusal leaked a queue or resident model reference");
    if (scenario == "vehicle-train-allocation")
        Require(game.trains == 1, "train allocation branch was not exercised");
    game.full = game.lowLodFull = game.allocationFails = false;
    if constexpr (std::is_same_v<T, CClientObject>)
        entity.m_fHealth = 1000.0f;
    CClientTime::now += 101;
    streamer.DoPulse(manager.camera.position);
    Require(entity.IsStreamedIn() && hasNative() && entity.m_pModelInfo->references == 1, "the distance streamer failed to retry the selected model");
    Require(entity.events == (initial ? std::vector<std::string>{in} : std::vector<std::string>{in, out, in}), "retry emitted redundant events");
    entity.onEvent = {};
}

static void Test(const std::string& scenario)
{
    CClientStreamer streamer([] { return false; }, 80, 200, 200);
    Camera&         camera = manager.camera;
    CModelInfo&     model = game.model;
    const CVector   origin{}, far{800, 0, 0};
    // The streamer activates the neighboring sectors on its first sector transition.
    camera.position = far;
    streamer.DoPulse(camera.position);
    camera.position = origin;
    streamer.DoPulse(camera.position);
    CClientObject object(streamer);
    if (scenario == "repair-broken" || scenario == "enable-respawn" || scenario == "break-while-loading")
    {
        if (scenario == "break-while-loading")
        {
            streamer.DoPulse(camera.position);
            Require(manager.requests.HasRequested(&object) && model.references == 1, "expected a pending model before breaking the object");
        }
        object.m_bRespawnEnabled = false;
        object.m_fHealth = 0.0f;
        model.ready = true;
        manager.PulseModels();
        streamer.DoPulse(camera.position);
        Require(!object.IsStreamedIn() && !object.m_pObject && object.events.empty() && !model.references,
                "broken object respawned while respawn was disabled");
        if (scenario == "enable-respawn")
            object.m_bRespawnEnabled = true;
        else
            object.m_fHealth = 1000.0f;
        streamer.DoPulse(camera.position);
        Require(object.IsStreamedIn() && object.events == std::vector<std::string>{"onClientElementStreamIn"},
                "eligible object missed its next stream-in after being broken");
        return;
    }
    if (scenario == "acquire-retry")
    {
        model.failAcquire = true;
        bool caught = false;
        try
        {
            streamer.DoPulse(camera.position);
        }
        catch (const std::runtime_error& e)
        {
            caught = std::string(e.what()) == "SDK acquire failure";
        }
        Require(caught && !manager.requests.HasRequested(&object) && !model.references, "failed acquire must propagate and leave no pending reference");
        model.failAcquire = false;
    }
    streamer.DoPulse(camera.position);
    Require(streamer.IsActiveElement(&object) && manager.requests.HasRequested(&object) && model.references == 1,
            "expected an outstanding asynchronous model in the initial active sector");

    if (scenario == "forced")
        object.AddStreamReference();
    const bool explicitDemand = scenario == "explicit-request" || scenario == "direct-stream-in";
    if (explicitDemand)
    {
        if (scenario == "explicit-request")
            manager.requests.Request(object.m_usModel, &object);
        else
            object.StreamIn(false);
        // Subsequent distance-driven requests for this same model must preserve the explicit demand.
        streamer.DoPulse(camera.position);
        Require(model.references == 1, "repeated requests must share one SDK reference");
    }
    if (scenario == "object-moves")
        object.UpdateStreamPosition(far);
    else if (scenario != "parent-streams-out" && scenario != "acquire-retry")
        camera.position = (scenario == "hysteresis") ? CVector{125, 0, 0} : far;
    if (scenario == "camera-sector" || scenario == "forced" || explicitDemand)
        streamer.DoPulse(camera.position);
    if (scenario == "camera-sector" || scenario == "object-moves" || scenario == "forced" || explicitDemand)
        Require(!streamer.IsActiveElement(&object), "production sector change did not remove the object");
    else
        Require(streamer.IsActiveElement(&object), "setup expected old sector membership at model pulse");

    std::unique_ptr<CClientObject> parent;
    if (scenario == "parent-streams-out")
    {
        parent = std::make_unique<CClientObject>(streamer);
        parent->Create();
        object.attachedTo = parent.get();
        parent->InternalStreamOut();
        Require(!parent->IsStreamedIn(), "parent should have streamed out before the child's completion");
    }

    model.ready = scenario != "object-moves";
    manager.PulseModels();
    Require(!manager.requests.HasRequested(&object) && model.references == int(object.m_pObject != nullptr), "stale or completed reference is still queued");
    if (scenario == "forced" || scenario == "hysteresis" || scenario == "acquire-retry" || explicitDemand)
    {
        Require(object.IsStreamedIn() && object.events == std::vector<std::string>{"onClientElementStreamIn"},
                "eligible completion failed to create the native object exactly once");
        if (scenario == "forced")
        {
            object.RemoveStreamReference();
            streamer.DoPulse(camera.position);
            Require(!object.IsStreamedIn() && object.events == std::vector<std::string>{"onClientElementStreamIn", "onClientElementStreamOut"} &&
                        object.GetLastStreamOutTime() == CClientTime::GetTime(),
                    "normal removal must emit one stream-out and set its cooldown");
        }
        return;
    }
    Require(!object.IsStreamedIn() && !object.m_pObject, "stale asynchronous completion created a native object");
    Require(object.events.empty() && !object.GetLastStreamOutTime(), "cancellation emitted a Lua streaming event or set a cooldown");
    Require(!model.applications, "cancellation applied the SDK model");
    // Return without a loaded model, so the normal stationary pulse must issue a fresh asynchronous request.
    object.attachedTo = nullptr;
    parent.reset();
    model.ready = false;
    camera.position = origin;
    object.UpdateStreamPosition(origin);
    streamer.DoPulse(camera.position);
    streamer.DoPulse(camera.position);
    Require(manager.requests.HasRequested(&object) && model.references == 1, "re-entry failed to acquire a new model reference");
    model.ready = true;
    manager.PulseModels();
    Require(object.IsStreamedIn() && model.references == 1 && model.applications == 1 && object.events == std::vector<std::string>{"onClientElementStreamIn"},
            "eligible retry did not create exactly once");
}
int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    try
    {
        const std::string scenario = argv[1];
        if (scenario.rfind("vehicle-", 0) == 0)
            TestRecreation<CClientVehicle>(scenario);
        else if (scenario.rfind("object-", 0) == 0 && scenario != "object-moves")
            TestRecreation<CClientObject>(scenario);
        else
            Test(scenario);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
