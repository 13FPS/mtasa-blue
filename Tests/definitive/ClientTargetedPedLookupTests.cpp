/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual client target routing over common SDK and controlled pools
 ****************************************************************************/
// Reuse the strict existing native-owner/ped fixture definitions. Its historical
// main is compiled under a different name and no historical case is invoked.
#pragma push_macro("main")
#undef main
#define main HistoricalTargetedLookupRegistryDefinitionsMain
#include "PoolsPedRegistryDETests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/mods/deathmatch/logic/CPedTargetClientLookup.h"
#include <initializer_list>
#include <string>

namespace client_targeted_ped_fixture
{
    struct RoutingFailure final : std::runtime_error
    {
        explicit RoutingFailure(const char* stage) : std::runtime_error(stage) {}
    };
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified client target fixture call: %s\n", method);
        std::abort();
    }
    struct Trace
    {
        std::array<unsigned, 16> events{};
        std::size_t              count = 0;
        void                     Record(unsigned event) noexcept
        {
            if (count == events.size())
                std::abort();
            events[count++] = event;
        }
        void Reset() noexcept { count = 0; }
    };
    struct EntityState
    {
        Trace&              trace;
        CEntitySAInterface* identity = nullptr;
        unsigned            calls = 0;
        bool                fail = false;
    };
    struct FallbackState
    {
        Trace&         trace;
        DWORD*         received = nullptr;
        CClientEntity* result = nullptr;
        unsigned       calls = 0;
        bool           fail = false;
    };
}
#include "ClientTargetedPedFixture.generated.h"

namespace
{
    void ExpectTrace(const client_targeted_ped_fixture::Trace& trace, std::initializer_list<unsigned> expected)
    {
        Check(trace.count == expected.size(), "client helper retained the expected call prefix length");
        std::size_t index = 0;
        for (const auto event : expected)
            Check(trace.events[index++] == event, "client helper preserved actual service/interface/lookup evaluation order");
    }
    template <class Operation>
    bool RoutingRefused(Operation operation, const char* stage)
    {
        try
        {
            operation();
        }
        catch (const client_targeted_ped_fixture::RoutingFailure& error)
        {
            return std::string(error.what()) == stage;
        }
        return false;
    }
}

int main()
{
    try
    {
        using namespace client_targeted_ped_fixture;
        Owner                             owner, foreignOwner;
        pool_registry_fixture::Ped        ped(owner, owner.rows[0].token);
        pool_registry_fixture::Ped        foreignPed(foreignOwner, foreignOwner.rows[0].token);
        Trace                             trace;
        FallbackState                     fallback{trace}, foreignFallback{trace};
        Pools                             pools(owner, fallback), foreignPools(foreignOwner, foreignFallback);
        CPoolsDE::BorrowedPedRegistration registration, foreignRegistration;
        ClientIdentity                    clientA{1}, clientB{2};
        Check(pools.BindBorrowedPed(ped, Client(clientA), registration) == ClockResult::Ok &&
                  foreignPools.BindBorrowedPed(foreignPed, Client(clientB), foreignRegistration) == ClockResult::Ok,
              "actual controlled pools bind their own genuine adjusted common ped wrappers");
        CPed* const    pedSdk = &ped;
        CEntity* const target = pedSdk;
        Check(static_cast<void*>(pedSdk) != static_cast<void*>(&ped),
              "the target uses an actual adjusted common CPed identity rather than assuming the complete object address");
        unsigned poolRequests = 0;
        auto     selectPools = [&]() -> CPools*
        {
            ++poolRequests;
            trace.Record(1);
            return &pools;
        };
        // Keep failure a runtime input: an unconditional-throwing supplier
        // makes the helper's successful branches unreachable under /WX.
        volatile bool failPools = true;
        const auto    nullRequests = poolRequests;
        Check(MtaClient::ResolveTargetClient(nullptr,
                                             [&]() -> CPools*
                                             {
                                                 ++poolRequests;
                                                 if (failPools)
                                                     throw RoutingFailure("unused-pools");
                                                 return &pools;
                                             }) == nullptr &&
                  poolRequests == nullRequests && trace.count == 0,
              "a null target does not request pools, query identity or run a lookup");

        owner.afterReference = [&] { trace.Record(4); };
        const auto references = owner.references;
        const auto requests = poolRequests;
        Check(MtaClient::ResolveTargetClient(target, selectPools) == Client(clientA) && poolRequests == requests + 1 && owner.references == references + 2 &&
                  !fallback.calls && !foreignFallback.calls,
              "actual common ped target dispatch reaches the existing checked CPoolsDE association, never its rejecting GetInterface or native fallback");
        ExpectTrace(trace, {1, 4});
        trace.Reset();

        Check(registration.Reset() == ClockResult::Ok && pools.BindBorrowedPed(ped, Client(clientB), registration) == ClockResult::Ok,
              "reassociation uses the actual existing controlled registry and guard lifecycle");
        Check(MtaClient::ResolveTargetClient(target, selectPools) == Client(clientB) && !fallback.calls,
              "a later helper call observes the current association instead of caching the construction client");
        trace.Reset();
        pools.InvalidateLocalPlayerClientEntity();
        Check(MtaClient::ResolveTargetClient(target, selectPools) == nullptr && !fallback.calls,
              "qualified null client association remains null without a StoredPointer/native-interface fallback");
        trace.Reset();
        Check(registration.Reset() == ClockResult::Ok && pools.BindBorrowedPed(ped, Client(clientA), registration) == ClockResult::Ok,
              "the current association is restored only through the actual registry owner");

        Check(MtaClient::ResolveTargetClient(static_cast<CEntity*>(static_cast<CPed*>(&foreignPed)), selectPools) == nullptr && !fallback.calls,
              "a foreign wrapper cannot borrow another pool's client association or trigger a classic interface round-trip");
        trace.Reset();
        unsigned foreignRequests = 0;
        Check(MtaClient::ResolveTargetClient(target,
                                             [&]() -> CPools*
                                             {
                                                 ++foreignRequests;
                                                 return &foreignPools;
                                             }) == nullptr &&
                  foreignRequests == 1 && !foreignFallback.calls,
              "the supplied pool identity controls lookup; a ped does not select an unrelated global registry");

        owner.referenceStatus = ClockResult::ReadFailure;
        const auto failedReferences = owner.references;
        const auto failedRequests = poolRequests;
        bool       ownerRefused = false;
        try
        {
            (void)MtaClient::ResolveTargetClient(target, selectPools);
        }
        catch (const std::runtime_error& error)
        {
            ownerRefused = std::string(error.what()).find("GetPedClientEntity") != std::string::npos;
        }
        Check(ownerRefused && owner.references == failedReferences + 1 && poolRequests == failedRequests + 1 && !fallback.calls,
              "actual owner refusal propagates after one lookup attempt, with no retry, native fallback or fabricated absence");
        owner.referenceStatus = ClockResult::Ok;
        trace.Reset();

        std::array<DWORD, 8> nativeIdentity{};
        EntityState          entityState{trace, reinterpret_cast<CEntitySAInterface*>(nativeIdentity.data())};
        Entity               entity(entityState);
        fallback.result = Client(clientB);
        const auto fallbackBefore = fallback.calls;
        Check(MtaClient::ResolveTargetClient(&entity, selectPools) == Client(clientB) && fallback.calls == fallbackBefore + 1 &&
                  fallback.received == nativeIdentity.data() && entityState.calls == 1,
              "a non-ped common entity keeps its original native identity and nullable client lookup virtuals exactly once");
        ExpectTrace(trace, {1, 2, 3});
        trace.Reset();
        fallback.result = nullptr;
        entityState.identity = nullptr;
        const auto nullFallback = fallback.calls;
        Check(MtaClient::ResolveTargetClient(&entity, selectPools) == nullptr && fallback.calls == nullFallback + 1 && !fallback.received,
              "the original non-ped path still calls the lookup with a null native identity and preserves its null result");
        ExpectTrace(trace, {1, 2, 3});
        trace.Reset();

        const auto beforeInterface = entityState.calls;
        const auto beforeFallback = fallback.calls;
        Check(RoutingRefused(
                  [&]
                  {
                      (void)MtaClient::ResolveTargetClient(&entity,
                                                           [&]() -> CPools*
                                                           {
                                                               trace.Record(1);
                                                               if (failPools)
                                                                   throw RoutingFailure("pools-service");
                                                               return &pools;
                                                           });
                  },
                  "pools-service") &&
                  entityState.calls == beforeInterface && fallback.calls == beforeFallback,
              "a pools-service exception keeps its type and stops before non-ped native identity access");
        ExpectTrace(trace, {1});
        trace.Reset();
        entityState.fail = true;
        Check(RoutingRefused([&] { (void)MtaClient::ResolveTargetClient(&entity, selectPools); }, "entity-interface") &&
                  entityState.calls == beforeInterface + 1 && fallback.calls == beforeFallback,
              "non-ped interface refusal preserves the original prefix and adds no lookup/retry");
        ExpectTrace(trace, {1, 2});
        trace.Reset();
        entityState.fail = false;
        entityState.identity = reinterpret_cast<CEntitySAInterface*>(nativeIdentity.data());
        fallback.fail = true;
        Check(RoutingRefused([&] { (void)MtaClient::ResolveTargetClient(&entity, selectPools); }, "entity-lookup") && fallback.calls == beforeFallback + 1 &&
                  fallback.received == nativeIdentity.data(),
              "non-ped lookup exceptions remain explicit after one original-identity attempt");
        ExpectTrace(trace, {1, 2, 3});
        fallback.fail = false;
        trace.Reset();
        Check(registration.Reset() == ClockResult::Ok && foreignRegistration.Reset() == ClockResult::Ok && !owner.acquisitions && !owner.releases &&
                  !foreignOwner.acquisitions && !foreignOwner.releases,
              "the helper acquires/releases no native references and leaves controlled registry retirement to existing owners");
        std::puts("Actual client targeted ped lookup fixture completed");
        std::printf("Passed %u actual client targeted ped lookup checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
