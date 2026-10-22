/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Common SDK registry identity, mutation and adjusted-base destruction
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main PoolSdkLinksHistoricalRegistryMain
#include "PedRegistryPolicyTests.cpp"
#undef main
#pragma pop_macro("main")
#include <windows.h>
#include <string>
#include <vector>
#include <utility>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include <game/CPed.h>
#include <game/CPedIK.h>
#include <game/CPools.h>
namespace pool_fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::abort();
    }
    inline unsigned anchorsDestroyed = 0;
    struct Anchor
    {
        virtual ~Anchor() { ++anchorsDestroyed; }
        std::array<unsigned char, 17> padding{};
    };
}
#include "PoolSdkPedFixture.generated.h"
int main()
{
    // The unchanged actual policy cases still exercise sparse and duplicate
    // registration, resolver-before-slot ordering, count-only reset, client
    // reassociation and caller-owned destruction before count retirement.
    Check(PoolSdkLinksHistoricalRegistryMain() == EXIT_SUCCESS, "existing real shared registry policy remains intact");
    using PedLink = SClientEntity<CPed>;
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetPed(static_cast<DWORD*>(nullptr))), PedLink*>);
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetPed(std::size_t{})), PedLink*>);
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetVehicle(static_cast<DWORD*>(nullptr))), SClientEntity<CVehicle>*>);
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetVehicle(std::size_t{})), SClientEntity<CVehicle>*>);
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetObject(static_cast<DWORD*>(nullptr))), SClientEntity<CObject>*>);
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetEntity(static_cast<DWORD*>(nullptr))), CEntity*>);
    static_assert(std::is_same_v<decltype(std::declval<CPools&>().GetClientEntity(static_cast<DWORD*>(nullptr))), CClientEntity*>);
    static_assert(std::is_standard_layout_v<PedLink> && std::is_trivially_copyable_v<PedLink>);
    static_assert(sizeof(PedLink) == 2 * sizeof(void*) && offsetof(PedLink, pEntity) == 0 && offsetof(PedLink, pClientEntity) == sizeof(void*));
    static_assert(std::has_virtual_destructor_v<CPed>);
    std::array<PedLink, 4> entries{};
    unsigned long          count = 0;
    unsigned               destroyed = 0;
    auto                   owned = std::make_unique<pool_fixture::Ped>(
        [&]
        {
            ++destroyed;
            Check(!entries[3].pEntity && !entries[3].pClientEntity && count == 1,
                                    "actual SDK virtual destructor observes retired canonical pair before caller count retirement");
            Check(!Policy::FindFromRef(entries, 3u << 8, [](std::uint32_t) { return true; }),
                                    "destructor reentry cannot rediscover the removed actual SDK wrapper");
        });
    CPed* const sdkPed = owned.get();
    Check(static_cast<void*>(sdkPed) != static_cast<void*>(owned.get()), "the actual SDK fixture exercises a nonzero multiple-inheritance base adjustment");
    Check(Policy::Register<CPed>(entries, count, 3, sdkPed, nullptr) && count == 1, "actual common SDK pointer registers in the same canonical pair array");
    auto* const pair = Policy::Find(entries, 3);
    Check(pair == &entries[3] && pair->pEntity == sdkPed && dynamic_cast<pool_fixture::Ped*>(pair->pEntity) == owned.get(),
          "ordinary C++ conversion preserves complete identity without reinterpreting pair specializations");
    CEntity* const genericEntity = pair->pEntity;
    Check(genericEntity->GetEntityType() == ENTITY_TYPE_PED && genericEntity->GetModelIndex() == 123 && pair->pEntity->GetType() == CIVILIAN_PED,
          "real SDK virtual dispatch through the stored adjusted base remains available to generic entity/ped consumers");
    pair->pEntity = nullptr;
    pair->pClientEntity = nullptr;
    Check(!entries[3].pEntity && !Policy::Find(entries, 3) && count == 1 && !destroyed,
          "client destructor callbacks null the actual mutable pair, not a detached converted view");
    *pair = {sdkPed, nullptr};
    Check(Policy::Find(entries, 3) == pair && entries[3].pEntity == sdkPed && count == 1,
          "restoring a borrowed pair changes no separately maintained registration count");
    unsigned resolutions = 0;
    Check(!Policy::FindFromRef(entries, (3u << 8) | 7,
                               [&](std::uint32_t)
                               {
                                   ++resolutions;
                                   return false;
                               }) &&
              resolutions == 1 && entries[3].pEntity == sdkPed,
          "generic SDK registry still requires current native resolution before returning an existing pointer");
    auto* const detached = Policy::Clear(entries, 3);
    auto* const complete = owned.release();
    Check(detached == sdkPed && dynamic_cast<pool_fixture::Ped*>(detached) == complete && !destroyed && count == 1,
          "retirement hands the same adjusted SDK pointer to its actual caller without destroying it early");
    delete detached;
    Policy::FinishRemoval(count);
    Check(destroyed == 1 && pool_fixture::anchorsDestroyed == 1 && count == 0,
          "deleting through the actual virtual SDK base reaches complete backend destruction exactly once");
    std::puts("Production MTA backend-neutral pool SDK links fixture completed");
    std::printf("Passed %u actual MTA pool SDK link checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
