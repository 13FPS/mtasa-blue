// Compile the actual checkpoint and marker visibility code over controlled game services.
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <SharedUtil.Misc.h>
#include <SharedUtil.Legacy.h>
#include <SString.hpp>
#include <CMatrix.h>
#include <game/C3DMarker.h>
#include <game/C3DMarkers.h>
#include <game/CCheckpoint.h>
#include <game/CCheckpoints.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace SharedUtil;
void Check(bool value, const char* label)
{
    if (!value)
        throw std::runtime_error(label);
}

class CClientMarker;
using CClientMarkerPtr = CClientMarker*;
struct CClientEntity
{
    void     GetPosition(CVector&) const { std::abort(); }
    unsigned GetID() const { return 42; }
};
struct CClientEntityRefManager
{
    static void AddEntityRefs(...) {}
    static void RemoveEntityRefs(...) {}
};
#define ENTITY_REF_DEBUG(p, name) p
#include "CClientCheckpoint.h"

class Checkpoint final : public CCheckpoint
{
public:
    unsigned removes = 0, arrows = 0;
    void     SetRotateRate(short rate) override { Check(rate == 0, "unexpected rotate rate"); }
    void     SetTargetArrowData(SColor, float) noexcept override { ++arrows; }
    void     Remove() override { ++removes; }
#include "UnusedCheckpoint.inl"
};

struct Checkpoints final : CCheckpoints
{
    Checkpoint   value;
    DWORD        id{};
    WORD         type{};
    CVector      direction, position;
    unsigned     creates = 0;
    bool         available = true;
    CCheckpoint* CreateCheckpoint(DWORD identifier, WORD style, CVector* pos, CVector* dir, float, float, SColor) override
    {
        ++creates;
        id = identifier;
        type = style;
        position = *pos;
        direction = *dir;
        return available ? &value : nullptr;
    }
    CCheckpoint* FindFreeMarker() override { std::abort(); }
    CCheckpoint* FindMarker(DWORD) override { std::abort(); }
};
struct Markers
{
    unsigned   finds = 0;
    C3DMarker* FindMarker(DWORD)
    {
        ++finds;
        return nullptr;
    }
};
struct CCamera
{
    CVector  center;
    float    radius = 0;
    unsigned calls = 0;
    bool     sphere = true, point = true;
    bool     IsSphereVisible(CVector* pos, float size)
    {
        ++calls;
        center = *pos;
        radius = size;
        return size == 0 ? point : sphere;
    }
};
struct CRegisteredCorona
{
    float range = 10, pull = 2;
    float GetRange() const { return range; }
    float GetNearClipDistance() const { return pull; }
};
struct CCoronas
{
    CRegisteredCorona  corona;
    bool               available = true;
    CRegisteredCorona* FindCorona(DWORD id)
    {
        Check(id == 67, "incorrect corona identifier");
        return available ? &corona : nullptr;
    }
};
struct Visibility
{
    bool visible = false;
    bool IsAtomicVisible(RpAtomic* atomic) const
    {
        Check(atomic != nullptr, "queried visibility with an absent atomic");
        return visible;
    }
};
struct Game
{
    Checkpoints   checkpoints;
    Markers       markers;
    Visibility    visibility;
    CCoronas      coronas;
    CCamera       camera;
    CCheckpoints* GetCheckpoints() { return &checkpoints; }
    Markers*      Get3DMarkers() { return &markers; }
    Visibility*   GetVisibilityPlugins() { return &visibility; }
    CCoronas*     GetCoronas() { return &coronas; }
    CCamera*      GetCamera() { return &camera; }
} game;
Game* g_pGame = &game;

struct CClientCamera
{
    CMatrix pose;
    bool    available = true;
    bool    GetMatrix(CMatrix& out) const
    {
        out = pose;
        return available;
    }
    void GetPosition(CVector& out) const { out = pose.vPos; }
};
struct Manager
{
    CClientCamera  camera;
    CClientCamera* GetCamera() { return &camera; }
};
struct CClientCorona
{
    RpAtomic* atomic = nullptr;
    RpAtomic* GetAtomic() const { return atomic; }
    DWORD     GetIdentifier() const { return 67; }
};
class CClientMarker : public CClientEntity
{
public:
    enum
    {
        MARKER_CORONA,
        MARKER_RING
    };
    unsigned           kind = MARKER_CORONA;
    Manager            manager;
    Manager*           m_pManager = &manager;
    CVector            pos;
    CClientCorona      corona;
    CClientCorona*     m_pMarker = &corona;
    CClientCheckpoint* checkpoint = nullptr;
    unsigned           GetMarkerType() const { return kind; }
    void               GetPosition(CVector& out) const { out = pos; }
    float              GetSize() const { return 5; }
    CClientCheckpoint* GetCheckpoint() { return checkpoint; }
    bool               IsOnScreen() const;
};
struct CStaticFunctionDefinitions
{
    static bool GetMarkerTarget(CClientMarker&, CVector&);
    static bool SetMarkerTarget(CClientEntity&, const CVector*);
};
struct Position
{
    struct
    {
        CVector vecPosition;
    } data;
};
struct NetBitStreamInterface
{
    bool                       initialTarget = false;
    CVector                    initialPosition;
    std::vector<unsigned char> bytes;
    std::vector<float>         numbers;
    std::size_t                byte = 0, number = 0;
    bool                       ReadBit() const { return initialTarget; }
    bool                       Read(Position* out) const
    {
        out->data.vecPosition = initialPosition;
        return true;
    }
    bool Read(unsigned char& value)
    {
        if (byte == bytes.size())
            return false;
        value = bytes[byte++];
        return true;
    }
    bool Read(float& value)
    {
        if (number == numbers.size())
            return false;
        value = numbers[number++];
        return true;
    }
};
struct CClientGame
{
    enum
    {
        MARKER_CHECKPOINT,
        MARKER_RING
    };
};
struct CMarkerRPCs
{
    struct Registry
    {
        CClientMarker* marker = nullptr;
        CClientMarker* Get(unsigned id) const { return id == 42 ? marker : nullptr; }
    } registry;
    Registry* m_pMarkerManager = &registry;
    void      SetMarkerTarget(CClientEntity*, NetBitStreamInterface&);
};
#define RUN_CHILDREN(method)
#define IS_MARKER(p) true

// Full production checkpoint class and its common base; only unrelated entity
// bookkeeping and the rendering services are supplied by this host.
#include "MarkerDistances.inl"
#include "CClientMarkerCommon.cpp"
#include "CClientCheckpoint.cpp"
#include "MarkerServices.inl"

// This is the target portion of the entity-add marker case. Values arrive from
// an in-memory reader after the enclosing handler has created the marker.
void ReadInitialTarget(NetBitStreamInterface& bitStream, CClientCheckpoint* pCheckpoint, unsigned ucType)
{
    Position position;
#include "InitialMarkerTarget.inl"
}

void CheckTargetPackets()
{
    CClientMarker     marker;
    CClientCheckpoint checkpoint(&marker);
    marker.checkpoint = &checkpoint;
    const CVector start(0, 16, 0), origin, update(4, 0, 2);
    checkpoint.SetPosition(start);
    NetBitStreamInterface initial;
    ReadInitialTarget(initial, &checkpoint, CClientGame::MARKER_CHECKPOINT);
    Check(!checkpoint.HasTarget(), "new entity without target reported one");
    initial.initialTarget = true;
    initial.initialPosition = origin;
    initial.bytes = {1, 2, 3, 4};
    initial.numbers = {0.75f};
    ReadInitialTarget(initial, &checkpoint, CClientGame::MARKER_CHECKPOINT);
    CVector returned;
    Check(CStaticFunctionDefinitions::GetMarkerTarget(marker, returned) && returned == origin && checkpoint.GetTargetArrowSize() == 0.75f &&
              checkpoint.GetTargetArrowColor() == SColorRGBA(1, 2, 3, 4),
          "initial entity target and arrow properties failed");
    CMarkerRPCs handler;
    handler.registry.marker = &marker;
    NetBitStreamInterface patch;
    patch.bytes = {1, 1, 0};
    patch.numbers = {update.fX, update.fY, update.fZ, 99.0f};
    handler.SetMarkerTarget(&marker, patch);
    Check(CStaticFunctionDefinitions::GetMarkerTarget(marker, returned) && returned == update, "RPC update did not publish the new target");
    handler.SetMarkerTarget(&marker, patch);
    Check(CStaticFunctionDefinitions::GetMarkerTarget(marker, returned) && returned == update, "incomplete update changed the target");
    handler.SetMarkerTarget(&marker, patch);
    Check(!CStaticFunctionDefinitions::GetMarkerTarget(marker, returned) && returned == update, "RPC clear did not remove the target");
}

void CheckTargets()
{
    CClientMarker     marker;
    CClientCheckpoint checkpoint(&marker);
    marker.checkpoint = &checkpoint;
    CClientMarkerCommon& stream = checkpoint;
    auto&                sink = game.checkpoints;
    const CVector        origin, start(10, 0, 0), second(0, -10, 0), plane(0, 0, 7), sentinel(4, 2, 1);
    CVector              observed = sentinel;
    Check(!CStaticFunctionDefinitions::GetMarkerTarget(marker, observed) && observed == sentinel, "absent target changed output");
    Check(!checkpoint.GetAtomic() && game.markers.finds == 0, "render lookup before creation");
    checkpoint.SetPosition(start);
    Check(CStaticFunctionDefinitions::SetMarkerTarget(marker, &origin), "Lua target setter rejected origin");
    Check(checkpoint.HasTarget() && CStaticFunctionDefinitions::GetMarkerTarget(marker, observed) && observed == origin && !sink.creates,
          "target unavailable before first stream-in");
    stream.StreamIn();
    const DWORD id = sink.id;
    Check(id && sink.type == CHECKPOINT_TUBE && sink.direction == CVector(-1, 0, 0) && sink.value.arrows == 1, "origin arrow was not created");
    checkpoint.SetPosition(second);
    Check(sink.id == id && sink.direction == CVector(0, 1, 0) && sink.position == second, "moving a checkpoint lost its origin target");
    const auto creates = sink.creates;
    checkpoint.SetTarget(&origin);
    Check(sink.creates == creates, "unchanged target recreated the game checkpoint");
    checkpoint.SetCheckpointType(CClientCheckpoint::TYPE_RING);
    checkpoint.SetTarget(&plane);
    checkpoint.SetPosition(origin);
    Check(sink.direction == CVector(0, 0, 1) && sink.type == CHECKPOINT_TORUS, "ring lost its target on a coordinate plane");
    Check(CStaticFunctionDefinitions::SetMarkerTarget(marker, nullptr) && !checkpoint.HasTarget() && sink.direction == CVector(1, 0, 0),
          "clearing a ring target retained its direction");
    checkpoint.SetPosition(plane);
    Check(sink.direction == CVector(1, 0, 0), "cleared target reappeared when moved");
    checkpoint.SetTarget(&plane);
    Check(sink.direction == CVector(), "coincident target is not a zero direction");
    checkpoint.SetPosition(start);
    CVector towardPlane = plane - start;
    towardPlane.Normalize();
    Check(sink.direction == towardPlane, "coincident target did not recover on movement");
    stream.StreamOut();
    Check(!checkpoint.GetAtomic() && !game.markers.finds, "stream-out queried a released game marker");
    sink.available = false;
    stream.StreamIn();
    Check(!checkpoint.GetAtomic() && !game.markers.finds, "failed creation retained a game marker");
}

void CheckVisibility()
{
    CClientMarker marker;
    marker.pos = CVector(0, 8, 0);
    auto& camera = game.camera;
    auto& sprite = game.coronas.corona;
    camera.sphere = false;
    Check(!marker.IsOnScreen() && camera.radius == 5 && camera.calls == 1, "offscreen outer sphere");
    camera.sphere = true;
    game.coronas.available = false;
    Check(!marker.IsOnScreen(), "unregistered corona visible");
    game.coronas.available = true;
#ifdef _WIN64
    marker.manager.camera.available = false;
    Check(!marker.IsOnScreen(), "missing client pose published visibility");
    marker.manager.camera.available = true;
    Check(marker.IsOnScreen() && camera.radius == 0 && camera.center == CVector(0, 6, 0), "incorrect offset or point radius");
    camera.point = false;
    Check(!marker.IsOnScreen(), "size test overrode rejected sprite point");
    camera.point = true;
    sprite.range = 7;
    Check(!marker.IsOnScreen() && camera.radius == 5, "corona beyond registration range");
    sprite.range = 8;
    Check(marker.IsOnScreen() && camera.radius == 0, "visible range endpoint");
    sprite.pull = std::numeric_limits<float>::infinity();
    Check(!marker.IsOnScreen(), "invalid corona offset published visibility");
#endif
    marker.kind = CClientMarker::MARKER_RING;
    Check(marker.IsOnScreen() && camera.radius == 2, "non-corona sphere fallback");
    auto oldCalls = camera.calls;
    marker.corona.atomic = reinterpret_cast<RpAtomic*>(&marker);
    game.visibility.visible = true;
    Check(marker.IsOnScreen() && camera.calls == oldCalls, "visible atomic required a second camera observation");
}

int main()
{
    try
    {
        CheckTargets();
        CheckTargetPackets();
        CheckVisibility();
        std::puts("Checkpoint targets, initial entity target, RPC update/clear and marker camera visibility passed");
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
