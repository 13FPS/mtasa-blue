/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise pending client model registrations through the request manager
 *****************************************************************************/
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <list>
#include <memory>
#include <string_view>

enum EModelRequestType
{
    BLOCKING,
    NON_BLOCKING
};
struct CElapsedTime
{
    void     SetMaxIncrement(unsigned) {}
    void     Reset() {}
    unsigned Get() const { return 0; }
};
template <class T>
bool ListContains(const std::list<T>& list, T value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}
#define OutputDebugLine(message) ((void)0)

class CClientModel
{
};
struct CModelInfo
{
    std::weak_ptr<CClientModel> registration;
    int                         references = 0, removals = 0, applied = 0;
    bool                        loaded = false, releasedWithRegistration = true;
    bool                        IsLoaded() const { return loaded; }
    void                        Request(EModelRequestType, const char*) {}
    void                        ModelAddRef(EModelRequestType, const char*) { ++references; }
    void                        MakeCustomModel() { ++applied; }
    void                        RemoveRef()
    {
        // The SDK receiver stays alive in every test, so even the historical
        // implementation can safely report whether the registration survived.
        releasedWithRegistration &= !registration.expired();
        --references;
        ++removals;
    }
};
struct CClientEntity
{
    std::function<void(CModelInfo*)> callback;
    void                             ModelRequestCallback(CModelInfo* model) { callback(model); }
};
#include "CClientModelRequestManager.h"
struct CClientModelManager
{
    std::array<std::shared_ptr<CClientModel>, 2> registrations;
    std::shared_ptr<CClientModel>                FindModelByID(int id) { return registrations.at(id); }
};
class CClientManager
{
public:
    CClientModelManager  registry;
    CClientModelManager* GetModelManager() { return &registry; }
    void                 Pulse(CClientModelRequestManager& requests) { requests.DoPulse(); }
};
struct ClientGame
{
    CClientManager  manager;
    CClientManager* GetManager() { return &manager; }
};
struct Game
{
    std::array<CModelInfo, 2> models;
    CModelInfo*               GetModelInfo(unsigned id) { return &models.at(id); }
    bool                      IsASyncLoadingEnabled() const { return true; }
};
Game*       g_pGame;
ClientGame* g_pClientGame;
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4456)
#endif
#include "CClientModelRequestManager.cpp"
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

void Check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    Game       game;
    ClientGame client;
    g_pGame = &game;
    g_pClientGame = &client;
    auto& registrations = client.manager.registry.registrations;
    auto& [oldModel, newModel] = game.models;
    for (unsigned i = 0; i < registrations.size(); ++i)
    {
        registrations[i] = std::make_shared<CClientModel>();
        game.models[i].registration = registrations[i];
    }
    const std::string_view scenario = argc == 2 ? argv[1] : "";
    CClientEntity          entity, other;
    {
        CClientModelRequestManager requests;
        Check(!requests.Request(0, &entity) && oldModel.references == 1, "unloaded model is queued and referenced");
        if (scenario == "cancel")
        {
            Check(!requests.Request(0, &other) && oldModel.references == 2, "another entity queues the same registration");
            registrations[0].reset();
            requests.Cancel(&entity, false);
            Check(oldModel.releasedWithRegistration && oldModel.references == 1 && !oldModel.registration.expired() && requests.HasRequested(&other),
                  "cancelling one entity releases its reference while both registrations are still held");
            requests.Cancel(&other, false);
        }
        else if (scenario == "replace" || scenario == "replace-loaded")
        {
            registrations[0].reset();
            newModel.loaded = scenario == "replace-loaded";
            Check(requests.Request(1, &entity) == newModel.loaded, "replacement respects the new model's loaded state");
            Check(oldModel.removals == 1 && oldModel.releasedWithRegistration && oldModel.registration.expired(),
                  "replacement releases the old SDK reference before dropping its registration");
            if (newModel.loaded)
                Check(newModel.applied == 1 && newModel.references == 0 && !requests.HasRequested(&entity), "loaded replacement skips the queue");
            else
            {
                registrations[1].reset();
                requests.Cancel(&entity, false);
                Check(newModel.removals == 1 && newModel.releasedWithRegistration && newModel.registration.expired(),
                      "the pending replacement retains its own registration through cancellation");
            }
        }
        else if (scenario == "complete")
        {
            bool callback = false;
            entity.callback = [&](CModelInfo* model)
            {
                Check(model == &oldModel && model->applied == 1 && !requests.HasRequested(&entity), "callback sees applied model and a detached request");
                registrations[0].reset();
                callback = true;
            };
            oldModel.loaded = true;
            client.manager.Pulse(requests);
            Check(callback && oldModel.removals == 1 && oldModel.releasedWithRegistration, "post-callback release retains the unregistered model");
        }
        else if (scenario == "replace-after-cancel")
        {
            Check(!requests.Request(0, &other), "second entity queues its original model");
            bool received = false;
            entity.callback = [&](CModelInfo*)
            {
                requests.Cancel(&other, true);
                Check(!requests.Request(1, &other), "unloaded replacement must wait");
            };
            other.callback = [&](CModelInfo* model) { received = model == &newModel; };
            oldModel.loaded = true;
            client.manager.Pulse(requests);
            Check(!received && oldModel.removals == 2 && oldModel.references == 0 && newModel.references == 1 &&
                      requests.GetRequestedModelInfo(&other) == &newModel,
                  "deferred cancellation of the old model must not consume the new pending request");
            newModel.loaded = true;
            client.manager.Pulse(requests);
            Check(received && newModel.references == 0 && newModel.removals == 1 && !requests.HasRequested(&other),
                  "only the new model is delivered on a later pulse and its reference is released exactly once");
            registrations[0].reset();
        }
        else if (scenario == "destroy")
            registrations[0].reset();
        else
            return 2;
    }
    const int expectedRemovals = scenario == "cancel" || scenario == "replace-after-cancel" ? 2 : 1;
    Check(oldModel.references == 0 && oldModel.removals == expectedRemovals && oldModel.releasedWithRegistration && oldModel.registration.expired(),
          "the last pending request balances its SDK reference and then releases the registration");
    std::puts("PASS");
}
