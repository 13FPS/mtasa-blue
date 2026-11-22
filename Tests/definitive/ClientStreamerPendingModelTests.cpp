// Build with test_client_streamer_pending_model.py. The grid, streamer, entity
// lifecycle, object stream methods and model request queue come from the client.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
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
        explicit ClassInitStub(void*) {} \
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
    static unsigned int GetTime() { return 1000; }
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
struct Game
{
    CModelInfo  model;
    CModelInfo* GetModelInfo(unsigned short) { return &model; }
    bool        IsASyncLoadingEnabled() { return true; }
} game;
auto* g_pGame = &game;
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
    virtual void             SetDimension(unsigned short dimension) { m_usDimension = dimension; }
    virtual void             ModelRequestCallback(CModelInfo*) {}
    unsigned short           GetDimension() const { return m_usDimension; }
    CClientEntity*           GetAttachedTo() { return attachedTo; }
    void                     UpdateSpatialData() {}
    bool                     IsDoubleSided() { return false; }
    void                     SetDoubleSided(bool) {}
    void                     CallEvent(const std::string& name, CLuaArguments&, bool) { events.push_back(name); }
    CClientEntity*           attachedTo = nullptr;
    std::vector<std::string> events;

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
    CClientPlayer*  GetOccupant() { return nullptr; }
    CClientVehicle* GetTowedByVehicle() { return nullptr; }
};
#define IS_PLAYER(entity)  (dynamic_cast<CClientPlayer*>(entity) != nullptr)
#define IS_VEHICLE(entity) (dynamic_cast<CClientVehicle*>(entity) != nullptr)

struct CClientObject : CClientStreamElement
{
    explicit CClientObject(CClientStreamer& streamer) : CClientStreamElement(&streamer, 1) {}
    ~CClientObject() { m_pModelRequester->Cancel(this, true); }
    void StreamIn(bool) override;
    void StreamOut() override;
    void ModelRequestCallback(CModelInfo*) override;
    bool IsLowLod() { return false; }
    void Create()
    {
        if (!m_pObject)
        {
            m_pObject = &native;
            NotifyCreate();
        }
    }
    void Destroy() { m_pObject = nullptr; }
    struct Native
    {
        float GetHealth() { return 1000; }
    } native;
    Native*                     m_pObject = nullptr;
    CClientModelRequestManager* m_pModelRequester = &manager.requests;
    CModelInfo*                 m_pModelInfo = &game.model;
    unsigned short              m_usModel = 1337;
    bool                        m_bRespawnEnabled = true;
    float                       m_fHealth = 1000;
};
#include "CClientStreamSector.cpp"
#include "CClientStreamSectorRow.cpp"
#include "CClientStreamer.cpp"
#include "CClientStreamElement.generated.inl"
#include "CClientObject.generated.inl"
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
    Require(!manager.requests.HasRequested(&object) && model.references == 0, "stale or completed reference is still queued");
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
    Require(object.IsStreamedIn() && model.references == 0 && model.applications == 1 && object.events == std::vector<std::string>{"onClientElementStreamIn"},
            "eligible retry did not create exactly once");
}
int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    try
    {
        Test(argv[1]);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
