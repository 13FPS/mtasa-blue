/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve current client associations through actual SDK registries
 *****************************************************************************/
// Reuse only the strict SDK leaf/owner definitions. The old registry cases
// are not invoked or reported as current tests by this focused entrypoint.
#pragma push_macro("main")
#undef main
#define main HistoricalPedRegistryDefinitionMain
#include "PoolsPedRegistryDETests.cpp"
#undef main
#pragma pop_macro("main")

namespace
{
    template <class Operation>
    bool RefusesClientLookup(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        return false;
    }
}

int main()
{
    try
    {
        using Policy = MtaGame::CPedRegistryPolicy;
        Owner                      owner, foreignOwner;
        pool_registry_fixture::Ped local(owner, owner.rows[0].token);
        pool_registry_fixture::Ped remote(owner, owner.rows[1].token);
        pool_registry_fixture::Ped foreign(foreignOwner, foreignOwner.rows[0].token);
        CPed*                      localSdk = &local;
        CPed*                      remoteSdk = &remote;
        ClientIdentity             clientA{1}, clientB{2};
        Check(static_cast<void*>(localSdk) != static_cast<void*>(&local), "new lookup compares the actual adjusted common SDK identity");

        // The actual classic implementation uses this same canonical-array
        // policy. Its native-bootstrap CPoolsSA object is not fabricated here;
        // the real classic/client call sites have a separate normal compile.
        std::array<SClientEntity<CPed>, 4> classicRows{};
        unsigned long                      classicCount = 0;
        Check(!Policy::FindByWrapper(classicRows, localSdk) && !Policy::FindByWrapper(classicRows, static_cast<CPed*>(nullptr)),
              "shared classic policy requires an existing row and rejects null wrapper identity");
        Check(Policy::Register<CPed>(classicRows, classicCount, 3, localSdk, Client(clientA)),
              "shared classic policy registers in its one existing canonical pair array");
        auto* classicPair = Policy::FindByWrapper(classicRows, localSdk);
        Check(classicPair == &classicRows[3] && classicPair->pClientEntity == Client(clientA) && classicCount == 1,
              "wrapper lookup scans actual capacity rather than treating count as an occupied index bound");
        Check(Policy::AssociateIfPresent(classicRows, 3, Client(clientB)) && Policy::FindByWrapper(classicRows, localSdk)->pClientEntity == Client(clientB) &&
                  classicCount == 1,
              "classic reassociation reads the current canonical client instead of a stored construction pointer");
        Policy::ResetCount(classicCount);
        Check(Policy::FindByWrapper(classicRows, localSdk) == classicPair && classicCount == 0,
              "classic count reset does not hide a still-existing wrapper row");
        classicPair->pEntity = remoteSdk;
        Check(!Policy::FindByWrapper(classicRows, localSdk) && Policy::FindByWrapper(classicRows, remoteSdk) == classicPair,
              "replacement wrapper is compared by current exact SDK identity without dereferencing foreign objects");
        Policy::Clear(classicRows, 3);
        Check(!Policy::FindByWrapper(classicRows, remoteSdk), "cleared canonical row cannot retain a client lookup result");

        pool_registry_fixture::Pools      pool(owner);
        CPools&                           sdk = pool;
        CPoolsDE::BorrowedPedRegistration localRegistration, remoteRegistration;
        const auto                        referencesBefore = owner.references;
        Check(!sdk.GetPedClientEntity(localSdk) && !sdk.GetPedClientEntity(&foreign) && !sdk.GetPedClientEntity(nullptr) &&
                  owner.references == referencesBefore && !pool.GetPedCount(),
              "qualified DE absence requires an existing registered row and never adopts an arbitrary wrapper token");
        Check(pool.BindBorrowedPed(local, Client(clientA), localRegistration) == ClockResult::Ok &&
                  pool.BindBorrowedPed(remote, Client(clientB), remoteRegistration) == ClockResult::Ok,
              "focused DE setup admits existing immutable-token wrappers without acquiring new native reference cells");
        auto* localPair = sdk.GetPed(std::size_t{0});
        auto* remotePair = sdk.GetPed(std::size_t{8});
        Check(sdk.GetPedClientEntity(localSdk) == Client(clientA) && sdk.GetPedClientEntity(remoteSdk) == Client(clientB) && pool.GetPedCount() == 2 &&
                  owner.probes == 0,
              "actual common SDK lookup resolves low and sparse high rows directly from registered wrapper identity");
        sdk.InvalidateLocalPlayerClientEntity();
        Check(!sdk.GetPedClientEntity(localSdk) && sdk.GetPedClientEntity(remoteSdk) == Client(clientB) && localPair->pEntity == localSdk &&
                  !localPair->pClientEntity && remotePair->pEntity == remoteSdk,
              "local invalidation returns current null association without hiding its existing row or changing another client");
        Check(localRegistration.Reset() == ClockResult::Ok && pool.BindBorrowedPed(local, Client(clientB), localRegistration) == ClockResult::Ok &&
                  sdk.GetPedClientEntity(localSdk) == Client(clientB),
              "controlled DE retirement and reassociation publish the newly admitted canonical client");
        localPair = sdk.GetPed(std::size_t{0});
        CClientEntity* preserved = Client(clientA);
        for (auto refusal : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::OwnerConflict, ClockResult::Expired})
        {
            owner.ownerStatus = refusal;
            Check(pool.ReadPedClientEntity(localSdk, preserved) == refusal && preserved == Client(clientA),
                  "missing DE owner authority preserves caller output rather than fabricating client absence");
            Check(RefusesClientLookup([&] { sdk.GetPedClientEntity(localSdk); }) && RefusesClientLookup([&] { sdk.GetPedClientEntity(nullptr); }),
                  "actual pointer-returning SDK query propagates owner refusal even for a null lookup input");
        }
        owner.ownerStatus = ClockResult::Ok;
        const auto selectedToken = owner.rows[0].token;
        ++owner.rows[0].token.version;
        Check(pool.ReadPedClientEntity(localSdk, preserved) == ClockResult::OwnerConflict && preserved == Client(clientA) &&
                  RefusesClientLookup([&] { sdk.GetPedClientEntity(localSdk); }),
              "stale registered token cannot be replaced by the wrapper address or a current native pool slot");
        owner.rows[0].token = selectedToken;
        localPair->pClientEntity = Client(clientA);
        Check(pool.ReadPedClientEntity(localSdk, preserved) == ClockResult::BindingChanged && preserved == Client(clientA),
              "uncontrolled foreign mutation of the canonical client association refuses instead of silently adopting it");
        localPair->pClientEntity = Client(clientB);
        owner.afterReference = [&] { localPair->pClientEntity = Client(clientA); };
        Check(pool.ReadPedClientEntity(localSdk, preserved) == ClockResult::BindingChanged && preserved == Client(clientA),
              "association changed during current token qualification preserves output and refuses a mixed relation");
        localPair->pClientEntity = Client(clientB);
        localPair->pEntity = &foreign;
        Check(pool.ReadPedClientEntity(&foreign, preserved) == ClockResult::BindingChanged && preserved == Client(clientA),
              "a caller-replaced foreign wrapper row cannot inherit its old controlled native token");
        localPair->pEntity = localSdk;
        owner.afterReference = [&] { owner.ownerStatus = ClockResult::Expired; };
        Check(pool.ReadPedClientEntity(localSdk, preserved) == ClockResult::Expired && preserved == Client(clientA),
              "late readonly owner loss preserves caller output without returning a previously matched client");
        owner.ownerStatus = ClockResult::Ok;
        Check(sdk.GetPedClientEntity(localSdk) == Client(clientB), "restored current owner can observe its unchanged valid association");
        Check(localRegistration.Reset() == ClockResult::Ok && remoteRegistration.Reset() == ClockResult::Ok && !pool.GetPedCount() &&
                  !sdk.GetPedClientEntity(localSdk) && !sdk.GetPedClientEntity(remoteSdk),
              "retired canonical rows remove both lookup results before their borrowed SDK wrappers are destroyed");
        Check(!owner.acquisitions && !owner.releases && !owner.probes,
              "semantic wrapper lookup never creates a registry, acquires/releases a native cell or reconstructs a raw-handle roundtrip");
        std::puts("Actual shared ped wrapper client lookup policy fixture completed");
        std::puts("Actual DE ped wrapper client lookup SDK fixture completed");
        std::printf("Passed %u actual ped client lookup checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped client lookup failed after %u checks: %s\n", checks, error.what());
        return EXIT_FAILURE;
    }
}
