/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise deathmatch upgrade decisions across streaming and Lua calls
 *****************************************************************************/
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "VehicleUpgradeCache.generated.h"

using ushort = unsigned short;
template <typename T, std::size_t Count>
using SFixedArray = std::array<T, Count>;
struct CElapsedTime
{
    unsigned Get() const { return 0; }
    void     Reset() {}
};
#include "CVehicleUpgrades.h"

enum eClientVehicleType
{
    CLIENTVEHICLE_CAR,
    CLIENTVEHICLE_TRAIN,
    CLIENTVEHICLE_BOAT,
    CLIENTVEHICLE_BIKE,
    CLIENTVEHICLE_BMX,
    CLIENTVEHICLE_HELI,
    CLIENTVEHICLE_PLANE
};
enum
{
    BLOCKING
};
struct CModelInfo
{
    unsigned                  parent = 0, reads = 0, requests = 0;
    SVehicleSupportedUpgrades support;
    bool                      loaded = true, loadSucceeds = true;
    unsigned                  GetParentID() { return parent; }
    SVehicleSupportedUpgrades GetVehicleSupportedUpgrades()
    {
        ++reads;
        return support;
    }
    bool IsLoaded() { return loaded; }
    void Request(int, const char*)
    {
        ++requests;
        loaded = loadSucceeds;
    }
};
struct CVehicle
{
    unsigned adds = 0, removes = 0;
    void     AddVehicleUpgrade(unsigned) { ++adds; }
    void     RemoveVehicleUpgrade(unsigned) { ++removes; }
};
struct Game
{
    std::array<CModelInfo*, 2048> models{};
    bool                          asyncLoading = true;
    CModelInfo*                   GetModelInfo(unsigned id) { return models.at(id); }
    bool                          IsASyncLoadingEnabled() { return asyncLoading; }
};
Game* g_pGame;
class CClientEntity
{
public:
    bool isVehicle = true;
};
class CClientVehicle : public CClientEntity
{
public:
    CVehicle*          native = nullptr;
    CModelInfo*        info = nullptr;
    CVehicleUpgrades   upgrades{this};
    unsigned           wheelResets = 0;
    ushort             id = 400;
    eClientVehicleType GetVehicleType() { return CLIENTVEHICLE_CAR; }
    ushort             GetModel() { return id; }
    CModelInfo*        GetModelInfo() { return info; }
    CVehicle*          GetGameVehicle() { return native; }
    CVehicleUpgrades*  GetUpgrades() { return &upgrades; }
    void               ResetWheelScale() { ++wheelResets; }
};

// The single-vehicle scripting wrappers run on the same upgrade object as the
// packet receiver. Child entity enumeration belongs to the entity subsystem.
#define RUN_CHILDREN(call)
#define IS_VEHICLE(entity) ((entity)->isVehicle)
struct CStaticFunctionDefinitions
{
    static bool AddVehicleUpgrade(CClientEntity& Entity, ushort usUpgrade);
    static bool RemoveVehicleUpgrade(CClientEntity& Entity, ushort usUpgrade);
};
#include "CVehicleUpgrades.cpp"
#include "VehicleUpgradeWrappers.generated.inl"

void Check(bool condition, const char* reason)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %s\n", reason);
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    Game game;
    g_pGame = &game;
    CClientVehicle client;
    CVehicle       native;
    CModelInfo     custom, parent, upgrade;
    client.id = 700;
    custom.parent = 400;
    custom.support.m_bInitialised = custom.support.m_bSpoiler = true;
    client.info = game.models[700] = &custom;
    game.models[400] = &parent;
    game.models[1000] = &upgrade;
    auto& upgrades = client.upgrades;
    using Script = CStaticFunctionDefinitions;

    parent.support.m_bInitialised = true;
    Check(upgrades.IsUpgradeCompatible(1000), "client must preserve the server's published spoiler eligibility");
    client.native = &native;
    Check(!Script::AddVehicleUpgrade(client, 1000) && !upgrades.HasUpgrade(1000) && !native.adds,
          "scripting must report failure when the live spoiler frame is missing");
    Check(!custom.reads && parent.reads == 1, "live application must inspect the parent model's discovery result");
    client.native = nullptr;
    Check(Script::AddVehicleUpgrade(client, 1000) && upgrades.HasUpgrade(1000), "a streamed-out vehicle retains server-compatible spoiler requests");
    parent.support.m_bInitialised = false;
    Check(upgrades.IsUpgradeCompatible(1000) && Script::AddVehicleUpgrade(client, 1000), "undiscovered streamed-out model must accept a deferred spoiler");
    Check(upgrades.HasUpgrade(1000) && !native.adds && !upgrade.requests, "deferred slot must not touch native vehicle or streaming");
    client.native = &native;
    upgrades.ReAddAll();
    Check(upgrades.HasUpgrade(1000) && !native.adds, "stream-in during discovery must preserve pending slot without native dispatch");
    Check(!Script::AddVehicleUpgrade(client, 1001) && upgrades.HasUpgrade(1000), "scripting refusal must preserve existing slot while geometry is unknown");
    parent.support.m_bInitialised = parent.support.m_bSpoiler = true;
    upgrade.loaded = false;
    upgrades.ReAddAll();
    Check(native.adds == 1 && upgrade.requests == 1 && upgrades.GetSlotState(2) == 1000, "stream-in with parent spoiler frame must request and apply once");
    Check(!Script::AddVehicleUpgrade(client, 1001) && upgrades.HasUpgrade(1000) && native.adds == 1,
          "missing live upgrade model must preserve prior slot and return failure");
    game.models[400] = nullptr;
    Check(!Script::AddVehicleUpgrade(client, 1000) && native.adds == 1, "missing parent cannot borrow custom model's spoiler frame for live dispatch");
    game.models[400] = &parent;
    parent.support.m_bSpoiler = false;
    upgrades.ReAddAll();
    Check(upgrades.HasUpgrade(1000) && native.adds == 1, "temporarily incompatible live geometry must preserve the queued network spoiler");
    Check(!upgrades.HasUpgrade(0) && !upgrades.RemoveUpgrade(0) && !native.removes, "empty slots must never dispatch native removal of upgrade zero");
    Check(!Script::AddVehicleUpgrade(client, 1073) && !client.wheelResets, "missing wheel receiver must not publish or reset wheel scale");
    game.models[1073] = &upgrade;
    Check(Script::AddVehicleUpgrade(client, 1073) && client.wheelResets == 1 && native.adds == 2, "valid wheel request dispatches once and resets scale");
    Check(Script::RemoveVehicleUpgrade(client, 1073) && client.wheelResets == 2 && native.removes == 1, "installed wheel is removed and scale restored");
    Check(!Script::RemoveVehicleUpgrade(client, 1073) && client.wheelResets == 2 && native.removes == 1, "duplicate removal has no side effects");
    game.models[1008] = &upgrade;
    Check(Script::AddVehicleUpgrade(client, 1008) && upgrades.AddUpgrade(1008, false) && native.adds == 3, "server echo of local nitro must not apply twice");

    upgrades.RemoveAll(false);
    Check(Script::AddVehicleUpgrade(client, 1073), "install a wheel before attempting replacement");
    CModelInfo otherWheel;
    game.models[1074] = &otherWheel;
    otherWheel.loaded = otherWheel.loadSucceeds = false;
    const auto adds = native.adds, resets = client.wheelResets;
    for (bool async : {true, false})
    {
        game.asyncLoading = async;
        Check(!Script::AddVehicleUpgrade(client, 1074), "a blocking request that leaves the wheel unloaded must report failure");
        Check(upgrades.GetSlotState(12) == 1073 && native.adds == adds && client.wheelResets == resets,
              "failed wheel replacement must preserve the live slot and wheel scale");
    }
    Check(otherWheel.requests == 2, "each loading policy tried its normal blocking request");
    game.asyncLoading = true;
    client.native = nullptr;
    Check(upgrades.AddUpgrade(1074, false) && otherWheel.requests == 2, "streamed-out network wheel waits for the next stream-in");
    client.native = &native;
    const auto queuedResets = client.wheelResets;
    upgrades.ReAddAll();
    Check(upgrades.HasUpgrade(1074) && otherWheel.requests == 3 && native.adds == adds && client.wheelResets == queuedResets,
          "a failed stream-in load retains its queued wheel and skips scale changes");
    otherWheel.loadSucceeds = true;
    upgrades.RestreamVehicleUpgrades(1074);
    Check(otherWheel.requests == 4 && native.adds == adds + 1 && client.wheelResets == queuedResets + 1,
          "restreaming applies the queued wheel once after the model becomes available");
    std::puts("PASS vehicle upgrade client application, streaming and script results");
}
