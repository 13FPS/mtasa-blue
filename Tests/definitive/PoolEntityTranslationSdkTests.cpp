/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual supported pool entity/client dispatch and complete identity anchors
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CVector.h>
#include "../../Client/game_de/CPoolsEntityDE.h"
#include "../../Client/game_de/CPedBorrowedFactoryDE.h"
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include "PoolEntityTranslationFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    bool SameReference(const NativePedReferenceToken& a, const NativePedReferenceToken& b) noexcept
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    struct Owner final : NativeGameTimingAccess
    {
        NativePedReferenceToken       issued{0, 77, 37};
        NativePedPoolReader::Value    value;
        std::array<unsigned char, 64> bytes{};
        ClockResult                   status = ClockResult::Ok;
        unsigned                      poolChecks = 0, resolutions = 0, acquires = 0, releases = 0, hookAt = 0;
        bool                          live = false;
        std::function<void()>         afterCheck;
        Owner()
        {
            value.handle = 1;
            value.slot = 0;
            value.pool = reinterpret_cast<std::uintptr_t>(this);
            value.objects = reinterpret_cast<std::uintptr_t>(bytes.data());
            value.flags = value.objects + 1;
            value.nativePed = value.objects;
            value.vtable = value.objects + 8;
            value.firstMethod = value.objects + 16;
            value.capacity = static_cast<std::int32_t>(NativePedPoolProfile::NativeCapacity);
            value.cursor = -1;
            value.generation = 1;
            value.ownsAllocations = 1;
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult CheckPedPoolAccess() noexcept override
        {
            ++poolChecks;
            if (afterCheck && poolChecks == hookAt)
            {
                auto action = std::move(afterCheck);
                afterCheck = {};
                action();
            }
            return status;
        }
        ClockResult ProbePedPoolHandle(std::uint32_t handle, bool& present, NativePedPoolReader::Value& output) noexcept override
        {
            if (status != ClockResult::Ok)
                return status;
            present = handle == value.handle;
            if (present)
                output = value;
            return ClockResult::Ok;
        }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& selected) noexcept override
        {
            return status != ClockResult::Ok                                                  ? status
                   : selected.nativePed == value.nativePed && selected.handle == value.handle ? ClockResult::Ok
                                                                                              : ClockResult::BindingChanged;
        }
        ClockResult ResolvePedReference(const NativePedReferenceToken& token, NativePedPoolReader::Value& output) noexcept override
        {
            ++resolutions;
            if (status != ClockResult::Ok)
                return status;
            if (!live || !SameReference(token, issued))
                return ClockResult::OwnerConflict;
            output = value;
            return ClockResult::Ok;
        }
        ClockResult AcquirePedReference(std::uint32_t handle, NativePedReferenceToken& output) noexcept override
        {
            ++acquires;
            if (status != ClockResult::Ok)
                return status;
            if (live)
                return ClockResult::Busy;
            if (handle != value.handle)
                return ClockResult::InvalidArgument;
            ++issued.version;
            live = true;
            output = issued;
            return ClockResult::Ok;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken& token) noexcept override
        {
            ++releases;
            if (!live || !SameReference(token, issued))
                return ClockResult::OwnerConflict;
            live = false;
            return ClockResult::Ok;
        }
    };
    class StablePed final : public pool_entity_fixture::Ped
    {
    public:
        using Ped::Ped;
        static void* operator new(std::size_t);
        static void  operator delete(void*) noexcept;
    };
    alignas(StablePed) std::array<unsigned char, sizeof(StablePed)> storage{};
    bool  storageUsed = false;
    void* StablePed::operator new(std::size_t size)
    {
        if (size != storage.size() || storageUsed)
            throw std::bad_alloc();
        storageUsed = true;
        return storage.data();
    }
    void StablePed::operator delete(void* pointer) noexcept
    {
        if (pointer != storage.data() || !storageUsed)
            std::abort();
        storageUsed = false;
    }
    using Factory = CPedBorrowedFactoryDE<StablePed>;
    struct ClientIdentity
    {
        unsigned value;
    };
    CClientEntity* Client(ClientIdentity& identity) noexcept
    {
        // Opaque association identity only, never a guessed client base cast
        // used to create a production client or invoke any client operation.
        return reinterpret_cast<CClientEntity*>(&identity);
    }
    DWORD* Raw(const Owner& owner) noexcept
    {
        return reinterpret_cast<DWORD*>(owner.value.nativePed);
    }
    template <class Action>
    bool Refused(Action action, ClockResult status, const char* operation)
    {
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message(error.what());
            return message.find(operation) != std::string::npos && message.find(": " + std::to_string(static_cast<unsigned>(status))) != std::string::npos;
        }
        return false;
    }
}

int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPoolsEntityDE> && !std::is_abstract_v<pool_entity_fixture::Pools>);
        static_assert(std::is_same_v<decltype(&CPoolsEntityDE::GetEntity), CEntity* (CPoolsEntityDE::*)(DWORD*)>);
        static_assert(std::is_same_v<decltype(&CPoolsEntityDE::GetClientEntity), CClientEntity* (CPoolsEntityDE::*)(DWORD*)>);
        {
            Owner                      owner;
            pool_entity_fixture::Pools pools(owner);
            CPools&                    sdk = pools;
            Check(!owner.poolChecks && !owner.acquires && !owner.releases, "pool construction is only a same-owner CPU association");
            owner.status = ClockResult::Stopped;
            Check(sdk.GetEntity(nullptr) == nullptr && sdk.GetClientEntity(nullptr) == nullptr && !owner.poolChecks && !owner.resolutions,
                  "classic raw null is CPU-only even when the native owner has stopped");
            owner.status = ClockResult::Ok;
            Factory        factory(pools);
            ClientIdentity clientA{1}, clientB{2};
            Check(factory.Create(1, Client(clientA)) == ClockResult::Ok && owner.acquires == 1 && pools.GetPedCount() == 1,
                  "the real typed factory supplies the one registered complete test ped");
            CPed* ped = nullptr;
            Check(factory.Read(ped) == ClockResult::Ok && ped, "the actual controlled pool returns its qualified SDK ped");
            Check(static_cast<void*>(ped) != static_cast<void*>(storage.data()), "the fixture exercises genuine virtual SDK base adjustment");
            Check(sdk.GetEntity(Raw(owner)) == static_cast<CEntity*>(ped), "GetEntity returns the actual typed SDK entity rather than native address bits");
            Check(sdk.GetClientEntity(Raw(owner)) == Client(clientA) && sdk.GetPedClientEntity(ped) == Client(clientA),
                  "new raw-address and existing wrapper client lookups return the same controlled association");
            Check(owner.acquires == 1 && !owner.releases, "entity and client translation neither acquire nor release another reference");
            auto* unknown = reinterpret_cast<DWORD*>(owner.value.nativePed + 47);
            Check(Refused([&] { (void)sdk.GetEntity(unknown); }, ClockResult::NotInitialized, "GetEntity") &&
                      Refused([&] { (void)sdk.GetClientEntity(unknown); }, ClockResult::NotInitialized, "GetClientEntity"),
                  "unknown nonzero addresses refuse instead of claiming absent vehicle/object/building coverage");
            for (const auto status : {ClockResult::WrongThread, ClockResult::Stopped, ClockResult::ReadFailure})
            {
                owner.status = status;
                Check(Refused([&] { (void)sdk.GetEntity(Raw(owner)); }, status, "GetEntity") &&
                          Refused([&] { (void)sdk.GetClientEntity(Raw(owner)); }, status, "GetClientEntity"),
                      "both nonzero translations preserve the current native owner's exact refusal");
            }
            owner.status = ClockResult::Ok;
            auto* pair = sdk.GetPed(std::size_t{0});
            pair->pClientEntity = Client(clientB);
            Check(Refused([&] { (void)sdk.GetEntity(Raw(owner)); }, ClockResult::BindingChanged, "GetEntity") &&
                      Refused([&] { (void)sdk.GetClientEntity(Raw(owner)); }, ClockResult::BindingChanged, "GetClientEntity"),
                  "mutable canonical SDK rows cannot replace controlled client provenance");
            pair->pClientEntity = Client(clientA);
            sdk.InvalidateLocalPlayerClientEntity();
            Check(
                sdk.GetClientEntity(Raw(owner)) == nullptr && sdk.GetEntity(Raw(owner)) == static_cast<CEntity*>(ped) && sdk.GetPedClientEntity(ped) == nullptr,
                "a controlled registered-null client is successful absence independently of the still-present SDK entity");
            CClientEntity*             output = Client(clientB);
            NativePedPoolReader::Value native;
            native.nativePed = 123;
            NativePedReferenceToken reference{88, 99, 7};
            owner.status = ClockResult::Expired;
            Check(pools.ReadPedClientBinding(ped, output, native, reference) == ClockResult::Expired && output == Client(clientB) && native.nativePed == 123 &&
                      reference.version == 88 && reference.ownerGeneration == 99 && reference.slot == 7,
                  "status-bearing client binding refusal preserves all caller outputs");
            const auto beforeNull = owner.poolChecks;
            Check(sdk.GetEntity(nullptr) == nullptr && sdk.GetClientEntity(nullptr) == nullptr && owner.poolChecks == beforeNull,
                  "null policy still bypasses access after a previously live owner refuses");
            owner.status = ClockResult::Ok;
            Check(factory.Retire() == ClockResult::Ok && !storageUsed && owner.releases == 1 && !pools.GetPedCount(),
                  "the existing typed factory withdraws and retires its wrapper once");
        }
        std::puts("Actual pool entity translation null and supported policy fixture completed");
        std::puts("Actual pool entity translation typed SDK identity fixture completed");
        {
            Owner                      owner;
            pool_entity_fixture::Pools pools(owner);
            Factory                    factory(pools);
            ClientIdentity             client{3};
            Check(factory.Create(1, Client(client)) == ClockResult::Ok, "seed the full-native-anchor refusal case with actual controlled registration");
            const auto before = owner.poolChecks;
            Check(pools.GetClientEntity(Raw(owner)) == Client(client),
                  "measure the complete successful client itinerary without replaying historical matrices");
            const auto accesses = owner.poolChecks - before;
            const auto original = owner.value;
            owner.hookAt = owner.poolChecks + accesses - 1;
            owner.afterCheck = [&] { ++owner.value.flags; };
            Check(Refused([&] { (void)pools.GetClientEntity(Raw(owner)); }, ClockResult::BindingChanged, "GetClientEntity") && !owner.afterCheck &&
                      owner.value.nativePed == original.nativePed,
                  "a changed full native identity at the final client seam refuses even when address and CPU token stay the same");
            owner.value = original;
            Check(factory.Retire() == ClockResult::Ok && !storageUsed, "native-anchor refusal adds no adoption or new lifetime debt");
        }
        {
            Owner                      owner;
            pool_entity_fixture::Pools pools(owner);
            Factory                    first(pools), second(pools);
            ClientIdentity             client{4};
            Check(first.Create(1, Client(client)) == ClockResult::Ok, "seed same-storage CPU reference ABA through the actual typed issuer");
            CPed* originalPed = nullptr;
            Check(first.Read(originalPed) == ClockResult::Ok, "retain only the original SDK pointer identity for the ABA comparison");
            const auto originalReference = first.ReadReceipt().reference;
            const auto before = owner.poolChecks;
            Check(pools.GetClientEntity(Raw(owner)) == Client(client), "measure the final client-binding entry seam on this controlled owner");
            const auto accesses = owner.poolChecks - before;
            CPed*      replacement = nullptr;
            bool       replaced = false;
            owner.hookAt = owner.poolChecks + accesses - 1;
            owner.afterCheck = [&]
            {
                replaced =
                    first.Retire() == ClockResult::Ok && second.Create(1, Client(client)) == ClockResult::Ok && second.Read(replacement) == ClockResult::Ok;
            };
            Check(Refused([&] { (void)pools.GetClientEntity(Raw(owner)); }, ClockResult::BindingChanged, "GetClientEntity") && replaced &&
                      replacement == originalPed && second.ReadReceipt().reference.version != originalReference.version && owner.acquires == 2 &&
                      owner.releases == 1,
                  "identical SDK/native addresses and client value cannot hide a new issued registration token across the complete lookup");
            Check(pools.GetEntity(Raw(owner)) == static_cast<CEntity*>(replacement) && pools.GetClientEntity(Raw(owner)) == Client(client),
                  "a fresh call can qualify the replacement after the interrupted original lookup refused");
            Check(second.Retire() == ClockResult::Ok && !storageUsed && owner.releases == 2 && !pools.GetPedCount(),
                  "both real typed lifetimes retire explicitly without adopting native or client objects");
        }
        std::puts("Actual pool entity translation full native and CPU token anchors fixture completed");
        Check(std::is_abstract_v<CPoolsEntityDE>, "all allocation/removal and unsupported pool contracts remain mandatory");
        std::puts("All 25 remaining pool SDK methods remain abstract");
        std::puts("Actual pool entity translation SDK fixture completed");
        std::printf("Passed %u pool entity translation SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
