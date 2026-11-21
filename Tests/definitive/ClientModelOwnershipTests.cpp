// Exercise the normal model/TXD manager and Lua request code with controlled
// native slots. Entity-event dispatch is represented by a reentrant callback.
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
using ushort = unsigned short;
using uint = unsigned int;
constexpr int INVALID_MODEL_ID = -1;
#define dassert(x) assert(x)
class CModelInfo;
#include "CClientModelManager.h"

class CBaseModelInfoSAInterface
{
};
struct CModelInfo
{
    uint                       id = 0, parent = 0, txd = 0;
    bool                       valid = false, damageable = true, refuse = false, failAfterPublication = false;
    int                        deallocated = 0;
    CBaseModelInfoSAInterface  first, second;
    CBaseModelInfoSAInterface* metadata = nullptr;
    bool                       IsValid() const { return valid; }
    auto                       GetInterface() const { return metadata; }
    uint                       GetParentID() const { return parent; }
    uint                       GetModel() const { return id; }
    bool                       IsDamageableAtomic() const { return damageable; }
    void                       Make()
    {
        if (refuse)
            return;
        valid = true;
        metadata = &first;
        if (failAfterPublication)
            throw std::runtime_error("streaming refusal after model publication");
    }
    void MakePedModel(const char*) { Make(); }
    void MakeObjectModel(ushort) { Make(); }
    void MakeObjectDamageableModel(ushort) { Make(); }
    void MakeClumpModel(ushort) { Make(); }
    void MakeTimedObjectModel(ushort) { Make(); }
    void MakeVehicleAutomobile(ushort) { Make(); }
    void DeallocateModel()
    {
        valid = false;
        metadata = nullptr;
        ++deallocated;
    }
    uint GetTextureDictionaryID() const { return txd; }
    void SetTextureDictionaryID(ushort id) { txd = id; }
};
struct Game
{
    std::vector<CModelInfo> info = std::vector<CModelInfo>(MAX_MODEL_DFF_ID + 4);
    int                     removes = 0, resets = 0, allocations = 0, reply = 0, nextSlot = 0;
    Game()
    {
        for (uint i = 0; i < info.size(); ++i)
            info[i].id = i;
    }
    uint        GetBaseIDforCOL() const { return static_cast<uint>(info.size()); }
    CModelInfo* GetModelInfo(uint id, bool = false) { return id < info.size() ? &info[id] : nullptr; }
    Game*       GetPools() { return this; }
    Game&       GetTxdPool() { return *this; }
    Game*       GetStreaming() { return this; }
    bool        IsFreeTextureDictonarySlot(uint id) { return !info[MAX_MODEL_DFF_ID + id].valid; }
    uint        GetFreeTextureDictonarySlot() const { return static_cast<uint>(nextSlot); }
    uint        AllocateTextureDictonarySlot(uint id, std::string&)
    {
        ++allocations;
        if (reply == static_cast<int>(id))
            info[MAX_MODEL_DFF_ID + id].valid = true;
        return static_cast<uint>(reply);
    }
    void RemoveTextureDictonarySlot(uint id)
    {
        assert(!IsFreeTextureDictonarySlot(id));
        info[MAX_MODEL_DFF_ID + id].valid = false;
        ++removes;
    }
    void SetStreamingInfo(uint, int, int, int, int) { ++resets; }
} game;
auto* g_pGame = &game;
struct CResource
{
    int        releases = 0;
    CResource* GetResourceModelStreamer() { return this; }
    void       FullyReleaseModel(std::uint16_t) { ++releases; }
};
class CClientManager
{
public:
    bool                 validParent = true;
    CClientModelManager  models;
    CClientModelManager* GetModelManager() { return &models; }
    CClientManager*      GetObjectManager() { return this; }
    CClientManager*      GetVehicleManager() { return this; }
    bool                 IsValidModel(ushort) const { return validParent; }
};
CClientManager* g_pClientGame;
struct lua_State
{
    CResource               resource;
    eClientModelType        type = eClientModelType::OBJECT;
    ushort                  parent = 7;
    std::variant<bool, int> returned = false;
};
void lua_pushinteger(lua_State* vm, int value)
{
    vm->returned = value;
}
void lua_pushboolean(lua_State* vm, bool value)
{
    vm->returned = value;
}
enum class VehicleType
{
    VT_LANDSTAL = 400
};
struct CScriptArgReader
{
    lua_State* vm;
    bool       error = false;
    explicit CScriptArgReader(lua_State* vm) : vm(vm) {}
    void        ReadEnumString(eClientModelType& type) { type = vm->type; }
    bool        NextIsNumber() const { return true; }
    void        ReadNumber(ushort& value) { value = vm->parent; }
    bool        HasErrors() const { return error; }
    void        SetCustomError(const char*) { error = true; }
    const char* GetFullErrorMessage() const { return "refused"; }
};
struct Lua
{
    CResource* resource = nullptr;
    Lua*       GetVirtualMachine(lua_State* vm)
    {
        resource = &vm->resource;
        return this;
    }
    CResource* GetResource() { return resource; }
} lua;
using CLuaMain = Lua;
struct Debug
{
    void LogCustom(lua_State*, const char*) {}
} debug;
struct CLuaEngineDefs
{
    static inline CClientManager*   m_pManager;
    static inline Lua*              m_pLuaManager = &lua;
    static inline Debug*            m_pScriptDebugging = &debug;
    static int                      EngineRequestModel(lua_State*);
    static std::variant<bool, uint> EngineRequestTXD(lua_State*, std::string);
    static bool                     EngineFreeTXD(uint);
};
std::function<void()> onRestore;
int                   restores = 0;
#include "ModelOwnership.generated.inl"
void CClientModel::RestoreDFF(CModelInfo*)
{
    ++restores;
    if (onRestore)
        onRestore();
}

int main()
{
    CClientManager client;
    g_pClientGame = CLuaEngineDefs::m_pManager = &client;
    auto&     manager = client.models;
    auto&     slot = game.info[12];
    CResource resource;
    lua_State vm;
    auto      request = [&] { return manager.Request(&client, 12, eClientModelType::OBJECT); };
    client.validParent = false;
    assert(CLuaEngineDefs::EngineRequestModel(&vm) == 1 && !std::get<bool>(vm.returned) && !manager.FindModelByID(0) && !restores);
    client.validParent = true;
    assert(CLuaEngineDefs::EngineRequestModel(&vm) == 1 && std::get<int>(vm.returned) == 0);
    assert(manager.FindModelByID(0)->GetParentResource() == &vm.resource);
    assert(manager.Remove(manager.FindModelByID(0)));
    assert(!game.info[0].valid);
    restores = 0;
    auto first = request();
    slot.valid = true;
    slot.metadata = &slot.second;
    assert(!first->Allocate(7));
    manager.Add(first);
    assert(!manager.FindModelByID(12));
    first->RestoreEntitiesUsingThisModel();
    first.reset();
    assert(restores == 0 && slot.valid && slot.deallocated == 0);

    slot.valid = false;
    slot.metadata = nullptr;
    auto failed = request();
    game.info[7].parent = 5;
    assert(!failed->Allocate(7));
    game.info[7].parent = 0;
    slot.refuse = true;
    assert(!failed->Allocate(7));
    slot.refuse = false;
    slot.failAfterPublication = true;
    try
    {
        failed->Allocate(7);
        assert(false);
    }
    catch (const std::runtime_error&)
    {
    }
    failed.reset();
    assert(slot.valid && slot.deallocated == 0);
    slot.failAfterPublication = false;
    slot.DeallocateModel();  // upstream owner settles its own partial publication

    auto old = request();
    assert(old->Allocate(7));
    old->SetParentResource(&resource);
    manager.Add(old);
    assert(!manager.Request(&client, 12, eClientModelType::PED));
    onRestore = [&] { assert(!manager.Remove(old) && !manager.FindModelByID(12)); };
    assert(manager.Remove(old));
    onRestore = {};
    assert(restores == 1 && resource.releases == 1 && old->GetParentResource() == nullptr && slot.valid);
    assert(old->Deallocate() && !old->Deallocate());
    auto replacement = request();
    assert(replacement->Allocate(7));
    manager.Add(replacement);
    assert(!manager.Remove(old) && manager.FindModelByID(12) == replacement);
    old.reset();
    assert(slot.valid && slot.deallocated == 2);
    slot.metadata = &slot.second;  // independent replacement with the old wrapper still alive
    assert(manager.Remove(replacement));
    replacement.reset();
    assert(restores == 1 && slot.deallocated == 2 && slot.valid);

    game.reply = -1;
    auto result = CLuaEngineDefs::EngineRequestTXD(&vm, "refused");
    assert(std::holds_alternative<bool>(result) && !std::get<bool>(result) && !manager.FindModelByID(MAX_MODEL_DFF_ID));
    game.reply = 2;
    result = CLuaEngineDefs::EngineRequestTXD(&vm, "wrong_slot");
    assert(std::holds_alternative<bool>(result) && !std::get<bool>(result) && !manager.FindModelByID(MAX_MODEL_DFF_ID));
    game.reply = 0;
    result = CLuaEngineDefs::EngineRequestTXD(&vm, "zero");
    assert(std::get<uint>(result) == 0);
    auto held = manager.FindModelByID(MAX_MODEL_DFF_ID);
    assert(held && held->GetParentResource() == &vm.resource);
    assert(CLuaEngineDefs::EngineFreeTXD(0) && !CLuaEngineDefs::EngineFreeTXD(0));
    assert(game.removes == 1 && game.resets == 1);
    assert(std::get<uint>(CLuaEngineDefs::EngineRequestTXD(&vm, "again")) == 0);
    held.reset();
    assert(game.removes == 1 && !game.IsFreeTextureDictonarySlot(0));
    manager.DeallocateModelsAllocatedByResource(&vm.resource);
    assert(game.removes == 2 && game.resets == 2);

    game.nextSlot = game.reply = 1;
    game.info[48].txd = 1;
    assert(std::get<uint>(CLuaEngineDefs::EngineRequestTXD(&vm, "bulk")) == 1);
    held = manager.FindModelByID(MAX_MODEL_DFF_ID + 1);
    manager.RemoveAll();
    assert(game.info[48].txd == 0 && game.removes == 3 && game.resets == 3);
    held.reset();
    assert(game.removes == 3);
    game.info[MAX_MODEL_DFF_ID + 1].valid = true;  // race between free-slot lookup and allocation
    const auto calls = game.allocations;
    assert(!std::get<bool>(CLuaEngineDefs::EngineRequestTXD(&vm, "occupied")));
    assert(game.allocations == calls && game.info[MAX_MODEL_DFF_ID + 1].valid);
    std::cout << "Client model allocation, identity, callbacks, TXD reuse and Lua results passed\n";
}
