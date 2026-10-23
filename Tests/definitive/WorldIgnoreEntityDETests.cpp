/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual world exclusion uses one existing controlled ped registry
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include "../../Client/game_de/CWorldIgnoreEntityDE.h"
#include "../../Client/game_de/CPoolsDE.h"
#include "../../Client/game_de/CPedVitalsDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace world_ignore_entity_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified IgnoreEntity SDK fixture: %s\n", method);
        std::abort();
    }
}
#include "WorldIgnoreEntityFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    bool Same(const NativePedReferenceToken& first, const NativePedReferenceToken& second) noexcept
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    struct Owner final : NativeGameTimingAccess
    {
        struct Row
        {
            NativePedReferenceToken    token;
            NativePedPoolReader::Value value;
        };
        std::array<Row, 2>                                                rows{};
        std::array<unsigned char, 9 * NativePedPoolProfile::ObjectStride> ownedObjects{};
        std::array<unsigned char, NativePedPoolProfile::NativeCapacity>   ownedFlags{};
        ClockResult                                                       access = ClockResult::Ok, reference = ClockResult::Ok, recheck = ClockResult::Ok;
        unsigned                                                          ownerChecks = 0, references = 0, rechecks = 0, acquisitions = 0, releases = 0;
        std::function<void()>                                             afterReference;
        Owner()
        {
            rows[0].token = {0x123456789abcdef0ull, 0xfedcba9876543210ull, 37};
            rows[1].token = {0x1111222233334444ull, 0xfedcba9876543210ull, 38};
            for (std::size_t index = 0; index != rows.size(); ++index)
            {
                auto& value = rows[index].value;
                value.handle = index ? 0x87f : 1;
                value.slot = index ? 8 : 0;
                value.generation = static_cast<unsigned char>(value.handle);
                value.pool = reinterpret_cast<std::uintptr_t>(this);
                value.objects = reinterpret_cast<std::uintptr_t>(ownedObjects.data());
                value.flags = reinterpret_cast<std::uintptr_t>(ownedFlags.data());
                value.nativePed = value.objects + value.slot * NativePedPoolProfile::ObjectStride;
                value.capacity = static_cast<std::int32_t>(NativePedPoolProfile::NativeCapacity);
                value.cursor = -1;
                value.ownsAllocations = 1;
            }
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult CheckPedPoolAccess() noexcept override
        {
            ++ownerChecks;
            return access;
        }
        ClockResult ResolvePedReference(const NativePedReferenceToken& token, NativePedPoolReader::Value& output) noexcept override
        {
            ++references;
            for (const auto& row : rows)
                if (Same(token, row.token))
                {
                    output = row.value;
                    if (afterReference)
                    {
                        auto action = std::move(afterReference);
                        afterReference = {};
                        action();
                    }
                    return access != ClockResult::Ok ? access : reference;
                }
            return ClockResult::OwnerConflict;
        }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& value) noexcept override
        {
            ++rechecks;
            if (access != ClockResult::Ok)
                return access;
            if (recheck != ClockResult::Ok)
                return recheck;
            for (const auto& row : rows)
                if (row.value.slot == value.slot && row.value.handle == value.handle && row.value.nativePed == value.nativePed)
                    return ClockResult::Ok;
            return ClockResult::BindingChanged;
        }
        ClockResult AcquirePedReference(std::uint32_t, NativePedReferenceToken&) noexcept override
        {
            ++acquisitions;
            return ClockResult::NotInitialized;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken&) noexcept override
        {
            ++releases;
            return ClockResult::NotInitialized;
        }
        void ResetCounters() noexcept { ownerChecks = references = rechecks = 0; }
    };
    struct WorldOwners : NativeWorldGroundAccess, NativeWorldLineAccess, NativeWorldAreaAccess, NativeWorldJetpackHeightAccess
    {
    };
    struct Ignore final : NativeWorldIgnoreEntityAccess
    {
        unsigned                sets = 0, clears = 0;
        ClockResult             result = ClockResult::Ok;
        NativePedReferenceToken received;
        bool                    acceptedPrefix = false;
        std::function<void()>   afterSet;
        ClockResult             SetWorldIgnoredPed(const NativePedReferenceToken& token) noexcept override
        {
            ++sets;
            received = token;
            acceptedPrefix = true;
            if (afterSet)
            {
                auto action = std::move(afterSet);
                afterSet = {};
                action();
            }
            return result;
        }
        ClockResult ClearWorldIgnoredEntity() noexcept override
        {
            ++clears;
            acceptedPrefix = false;
            return result;
        }
    };
    struct Unavailable final : NativeWorldIgnoreEntityAccess
    {
    };
    struct ClientIdentity
    {
        unsigned value;
    };
    CClientEntity* Client(ClientIdentity& identity) noexcept
    {
        // Only the canonical nullable association identity is used here.
        // No complete client or engine object is created or dereferenced.
        return reinterpret_cast<CClientEntity*>(&identity);
    }
    bool Refused(CWorld& world, CEntity* entity, ClockResult expected)
    {
        try
        {
            world.IgnoreEntity(entity);
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            return text.find("IgnoreEntity") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(expected))) != std::string::npos;
        }
        return false;
    }
}

static_assert(std::is_abstract_v<mta::game_de::CWorldIgnoreEntityDE> && std::is_abstract_v<mta::game_de::CPoolsDE>);
static_assert(std::is_base_of_v<mta::game_de::CWorldJetpackHeightDE, mta::game_de::CWorldIgnoreEntityDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CWorldIgnoreEntityDE::IgnoreEntity), void (mta::game_de::CWorldIgnoreEntityDE::*)(CEntity*)>);

int main()
{
    try
    {
        Owner                              owner, differentOwner;
        world_ignore_entity_fixture::Pools pools(owner), anotherPool(owner);
        auto                               callerToken = owner.rows[0].token;
        const auto                         expected = callerToken;
        world_ignore_entity_fixture::Ped   ped(owner, callerToken), anotherPed(owner, owner.rows[1].token),
            foreignPed(differentOwner, differentOwner.rows[0].token);
        world_ignore_entity_fixture::ForeignEntity entity;
        CPoolsDE::BorrowedPedRegistration          registration, anotherRegistration;
        ClientIdentity                             firstClient{1}, replacementClient{2};
        Check(pools.BindBorrowedPed(ped, Client(firstClient), registration) == ClockResult::Ok &&
                  anotherPool.BindBorrowedPed(anotherPed, nullptr, anotherRegistration) == ClockResult::Ok,
              "test setup binds genuine same-owner wrappers in two distinct existing controlled registries");
        callerToken = {};
        WorldOwners                    otherWorld;
        Ignore                         first, second;
        NativeWorldIgnoreEntityAccess* selected = &first;
        owner.ResetCounters();
        world_ignore_entity_fixture::World world(otherWorld, otherWorld, otherWorld, otherWorld, pools, *selected);
        selected = &second;
        CWorld& sdk = world;
        Check(!owner.ownerChecks && !owner.references && !first.sets && !first.clears,
              "world construction borrows pool/access without native calls, private registry or registration creation");
        sdk.IgnoreEntity(nullptr);
        Check(first.clears == 1 && !first.sets && !second.clears && !owner.ownerChecks && !owner.references,
              "actual null SDK call clears once through its borrowed access and bypasses every pool lookup");
        sdk.IgnoreEntity(&ped);
        Check(
            first.sets == 1 && first.clears == 1 && !second.sets && Same(first.received, expected) && owner.references && owner.rechecks &&
                pools.GetPedCount() == 1 && !owner.acquisitions && !owner.releases,
            "actual CWorld dispatch resolves the existing controlled wrapper token and forwards it once without native pointer/factory/lifetime substitution");
        Check(static_cast<void*>(static_cast<CPed*>(&ped)) != static_cast<void*>(&ped),
              "the actual multiple-inheritance CPed base identity is used instead of complete-object address reinterpretation");
        const NativePedReferenceToken sentinel{91, 92, 93};
        auto                          output = sentinel;
        Check(pools.ReadBorrowedPedReference(&ped, output) == ClockResult::Ok && Same(output, expected),
              "direct status lookup publishes the exact copied token only after its controlled row/native bookends");

        owner.ResetCounters();
        const auto setsBeforeForeign = first.sets, clearsBeforeForeign = first.clears;
        Check(Refused(sdk, &entity, ClockResult::InvalidArgument) && !owner.ownerChecks && !owner.references && first.sets == setsBeforeForeign &&
                  first.clears == clearsBeforeForeign,
              "a genuine non-ped SDK object refuses before pool/native access without querying its interface or stored pointer");
        for (CPed* candidate : {static_cast<CPed*>(&foreignPed), static_cast<CPed*>(&anotherPed)})
        {
            output = sentinel;
            Check(pools.ReadBorrowedPedReference(candidate, output) == ClockResult::NotInitialized && Same(output, sentinel) &&
                      Refused(sdk, candidate, ClockResult::NotInitialized) && first.sets == setsBeforeForeign,
                  "foreign-owner or other-pool wrappers are absent from this exact registry and cannot publish a token or native ignore target");
        }
        auto* pair = pools.GetPed(std::size_t{0});
        Check(pair && pair->pEntity == &ped && pair->pClientEntity == Client(firstClient), "test obtained the one actual mutable canonical SDK row");
        pair->pClientEntity = Client(replacementClient);
        output = sentinel;
        const auto beforeTamper = first.sets;
        Check(pools.ReadBorrowedPedReference(&ped, output) == ClockResult::BindingChanged && Same(output, sentinel) &&
                  Refused(sdk, &ped, ClockResult::BindingChanged) && first.sets == beforeTamper,
              "caller client replacement invalidates the controlled pair and cannot leak a token or invoke native set");
        pair->pClientEntity = Client(firstClient);
        pair->pEntity = &foreignPed;
        output = sentinel;
        Check(pools.ReadBorrowedPedReference(&foreignPed, output) == ClockResult::BindingChanged && Same(output, sentinel) &&
                  Refused(sdk, &foreignPed, ClockResult::BindingChanged) && first.sets == beforeTamper,
              "a foreign wrapper inserted into a mutable canonical row never adopts the original controlled token");
        pair->pEntity = &ped;

        for (const auto status : {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                                  ClockResult::Busy, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.access = status;
            output = sentinel;
            Check(pools.ReadBorrowedPedReference(&ped, output) == status && Same(output, sentinel) && Refused(sdk, &ped, status) && first.sets == beforeTamper,
                  "controlled lookup owner refusal leaves token output unchanged and never becomes a native set or fabricated absence");
        }
        owner.access = ClockResult::Ok;
        owner.reference = ClockResult::ReadFailure;
        output = sentinel;
        Check(pools.ReadBorrowedPedReference(&ped, output) == ClockResult::ReadFailure && Same(output, sentinel) &&
                  Refused(sdk, &ped, ClockResult::ReadFailure) && first.sets == beforeTamper,
              "staged native reference data on refusal cannot publish the controlled token");
        owner.reference = ClockResult::Ok;
        owner.recheck = ClockResult::BindingChanged;
        Check(Refused(sdk, &ped, ClockResult::BindingChanged) && first.sets == beforeTamper,
              "native observation recheck failure stops before set without retry");
        owner.recheck = ClockResult::Ok;
        owner.afterReference = [&] { pair->pClientEntity = Client(replacementClient); };
        output = sentinel;
        Check(pools.ReadBorrowedPedReference(&ped, output) == ClockResult::BindingChanged && Same(output, sentinel) && first.sets == beforeTamper,
              "row mutation during native reference callback is caught before token publication");
        pair->pClientEntity = Client(firstClient);
        owner.afterReference = [&] { owner.access = ClockResult::WrongThread; };
        Check(Refused(sdk, &ped, ClockResult::WrongThread) && first.sets == beforeTamper,
              "owner change during lookup refuses before native ignore without replaying the earlier accepted read");
        owner.access = ClockResult::Ok;

        for (const auto status :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = status;
            const auto sets = first.sets, clears = first.clears;
            Check(Refused(sdk, &ped, status) && first.sets == sets + 1 && first.clears == clears && first.acceptedPrefix && Same(first.received, expected),
                  "each native set refusal is surfaced once with its accepted prefix retained; SDK never retries or invents a compensating clear");
            owner.ResetCounters();
            Check(Refused(sdk, nullptr, status) && first.clears == clears + 1 && first.sets == sets + 1 && !first.acceptedPrefix && !owner.ownerChecks &&
                      !owner.references,
                  "each native clear refusal is surfaced once and does not resolve, set or manufacture a fallback target");
        }
        first.result = ClockResult::Ok;
        first.afterSet = [&] { first.result = ClockResult::BindingChanged; };
        const auto beforeCallbackSets = first.sets, beforeCallbackClears = first.clears;
        Check(Refused(sdk, &ped, ClockResult::BindingChanged) && first.sets == beforeCallbackSets + 1 && first.clears == beforeCallbackClears,
              "callback refusal after accepted set propagates without native replay or world cleanup");
        first.result = ClockResult::Ok;
        sdk.IgnoreEntity(nullptr);
        Unavailable                        unavailable;
        world_ignore_entity_fixture::World missing(otherWorld, otherWorld, otherWorld, otherWorld, pools, unavailable);
        Check(Refused(missing, nullptr, ClockResult::NotInitialized) && Refused(missing, &ped, ClockResult::NotInitialized),
              "default access refuses clear and registered set rather than publishing a no-op world implementation");

        ClockResult withdrawn = ClockResult::NotInitialized;
        owner.afterReference = [&] { withdrawn = registration.Reset(); };
        output = sentinel;
        Check(pools.ReadBorrowedPedReference(&ped, output) == ClockResult::BindingChanged && Same(output, sentinel) && withdrawn == ClockResult::Ok &&
                  !registration.IsBound() && !pools.GetPedCount(),
              "withdrawal during lookup invalidates the snapshot before output without owning native reference release");
        output = sentinel;
        Check(pools.ReadBorrowedPedReference(&ped, output) == ClockResult::NotInitialized && Same(output, sentinel) &&
                  Refused(sdk, &ped, ClockResult::NotInitialized) && !owner.acquisitions && !owner.releases,
              "withdrawn CPU registration stays absent; the SDK cannot reacquire, release or fabricate native lifetime");
        Check(anotherRegistration.Reset() == ClockResult::Ok && !anotherPool.GetPedCount(), "separate test registry guard retired before its wrapper and pool");
        std::puts("Actual world IgnoreEntity SDK fixture completed");
        std::printf("Passed %u actual world IgnoreEntity SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "World IgnoreEntity SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
