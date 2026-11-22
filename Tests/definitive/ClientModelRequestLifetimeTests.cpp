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
#include <exception>
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
enum class Failure
{
    LoadCheck,
    Acquire,
    Apply,
    Callback,
    Release
};
struct CModelInfo
{
    std::weak_ptr<CClientModel> registration;
    int                         references = 0, removals = 0, applied = 0;
    bool                        loaded = false, releasedWithRegistration = true;
    bool                        failLoadCheck = false, failAcquire = false, failApply = false, failRelease = false;
    bool                        IsLoaded() const
    {
        if (failLoadCheck)
            throw Failure::LoadCheck;
        return loaded;
    }
    void Request(EModelRequestType, const char*) {}
    void ModelAddRef(EModelRequestType, const char*)
    {
        if (failAcquire)
            throw Failure::Acquire;
        ++references;
    }
    void MakeCustomModel()
    {
        ++applied;
        if (failApply)
            throw Failure::Apply;
    }
    void RemoveRef()
    {
        // The SDK receiver stays alive in every test, so even the historical
        // implementation can safely report whether the registration survived.
        releasedWithRegistration &= !registration.expired();
        --references;
        ++removals;
        if (failRelease)
            throw Failure::Release;
    }
};
struct CClientEntity
{
    std::function<void(CModelInfo*)> callback;
    void                             ModelRequestCallback(CModelInfo* model) { callback(model); }
};
class CClientStreamElement : public CClientEntity
{
public:
    bool IsAttemptingToStreamIn() const { return true; }
    bool IsModelStreamInWanted() const { return true; }
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

template <class Operation>
void ExpectFailure(Failure expected, Operation operation)
{
    try
    {
        operation();
    }
    catch (Failure failure)
    {
        Check(failure == expected, "propagate the original SDK or callback failure");
        return;
    }
    Check(false, "operation must propagate its failure");
}

int main(int argc, char** argv)
{
    // A historical destructor throwing from noexcept should fail this subprocess promptly.
    std::set_terminate([] { std::_Exit(91); });
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
        else if (scenario == "callback-failure" || scenario == "apply-failure" || scenario == "completion-release-failure")
        {
            bool called = false;
            oldModel.loaded = true;
            oldModel.failApply = scenario == "apply-failure";
            oldModel.failRelease = scenario != "apply-failure";
            Check(!requests.Request(1, &other), "queue an unrelated request alongside completion");
            entity.callback = [&](CModelInfo*)
            {
                called = true;
                if (scenario == "callback-failure")
                {
                    requests.Cancel(&other, true);
                    newModel.failRelease = true;
                    throw Failure::Callback;
                }
            };
            registrations[0].reset();
            const auto expected = oldModel.failApply ? Failure::Apply : (scenario == "callback-failure" ? Failure::Callback : Failure::Release);
            ExpectFailure(expected, [&] { client.manager.Pulse(requests); });
            Check(called == !oldModel.failApply && !requests.HasRequested(&entity) && oldModel.removals == 1 && oldModel.releasedWithRegistration,
                  "completion detaches and releases its registration on every exception");
            Check(requests.HasRequested(&other) == (scenario != "callback-failure"), "the failed pulse drains only its deferred cancellations");
            newModel.failRelease = false;
            requests.Cancel(&other, true);
            Check(!requests.HasRequested(&other) && newModel.removals == 1, "cancel works immediately after a failed pulse");
            Check(!requests.Request(1, &other), "a fresh request can be made after a failed pulse");
            bool delivered = false;
            other.callback = [&](CModelInfo* model) { delivered = model == &newModel; };
            newModel.loaded = true;
            client.manager.Pulse(requests);
            Check(delivered && newModel.removals == 2 && newModel.references == 0, "the next pulse delivers the fresh request normally");
        }
        else if (scenario == "poll-failure")
        {
            oldModel.failLoadCheck = true;
            ExpectFailure(Failure::LoadCheck, [&] { client.manager.Pulse(requests); });
            Check(requests.HasRequested(&entity) && !oldModel.removals, "a refused poll preserves the pending request");
            requests.Cancel(&entity, true);
            Check(!requests.HasRequested(&entity) && oldModel.removals == 1, "a failed poll leaves immediate cancellation available");
            registrations[0].reset();
        }
        else if (scenario == "replace-query-failure" || scenario == "replace-acquire-failure" || scenario == "replace-release-failure")
        {
            oldModel.failRelease = scenario == "replace-release-failure";
            newModel.failAcquire = scenario == "replace-acquire-failure";
            newModel.failLoadCheck = scenario == "replace-query-failure";
            registrations[0].reset();
            const auto expected = oldModel.failRelease ? Failure::Release : (newModel.failAcquire ? Failure::Acquire : Failure::LoadCheck);
            ExpectFailure(expected, [&] { requests.Request(1, &entity); });
            Check(!requests.HasRequested(&entity) && oldModel.removals == 1 && oldModel.releasedWithRegistration && !newModel.removals,
                  "failed replacement cannot leave the retired old request or a speculative new reference");
            requests.Cancel(&entity, false);
            newModel.failAcquire = newModel.failLoadCheck = false;
            Check(!requests.Request(1, &entity), "a replacement may be requested after a refused transition");
            requests.Cancel(&entity, false);
            Check(newModel.removals == 1 && newModel.references == 0, "only the successful acquire is released");
        }
        else if (scenario == "cancel-release-failure")
        {
            registrations[0].reset();
            oldModel.failRelease = true;
            ExpectFailure(Failure::Release, [&] { requests.Cancel(&entity, false); });
            Check(!requests.HasRequested(&entity), "failed release detaches the canceled request");
            requests.Cancel(&entity, false);
        }
        else if (scenario == "teardown-failure")
        {
            Check(!requests.Request(1, &other), "queue a second request for shutdown");
            registrations[0].reset();
            registrations[1].reset();
            oldModel.failRelease = newModel.failRelease = true;
        }
        else if (scenario == "destroy")
            registrations[0].reset();
        else
            return 2;
    }
    if (scenario == "teardown-failure")
        Check(newModel.removals == 1 && !newModel.references && newModel.releasedWithRegistration && newModel.registration.expired(),
              "shutdown attempts each release once even when a previous release failed");
    const int expectedRemovals = scenario == "cancel" || scenario == "replace-after-cancel" ? 2 : 1;
    Check(oldModel.references == 0 && oldModel.removals == expectedRemovals && oldModel.releasedWithRegistration && oldModel.registration.expired(),
          "the last pending request balances its SDK reference and then releases the registration");
    std::puts("PASS");
}
