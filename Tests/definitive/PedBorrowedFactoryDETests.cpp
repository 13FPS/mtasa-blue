/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed SDK retirement composed with the real retained-cell controller
 ****************************************************************************/
#define main PedBorrowedFactoryHistoricalReferenceMain
#include "NativePedReferenceTests.cpp"
#undef main
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include "../../Client/game_de/CPedBorrowedFactoryDE.h"
#include <game/CPedIK.h>
#include <functional>
#include <memory>
#include <string>
#include "PedBorrowedFactoryFixture.generated.h"

namespace
{
    struct LifecycleOwner final : NativeGameTimingAccess
    {
        NativePedReferenceMemory                      memory;
        std::unique_ptr<NativePedReferenceController> controller = std::make_unique<NativePedReferenceController>();
        ClockResult                                   admission = ClockResult::Ok;
        NativePedReferenceMemory::Receipt             lastNative;
        unsigned                                      acquisitions = 0, releases = 0;
        std::function<void()>                         beforeRelease;

        explicit LifecycleOwner(NativePedReferenceMemory::Binding binding)
            : memory(binding, &PoolTestHost::Owner, &ReferenceTestHost::Recover, 42), binding(binding)
        {
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult CheckPedPoolAccess() noexcept override
        {
            if (admission != ClockResult::Ok)
                return admission;
            const auto checked = PoolTestHost::Owner();
            return checked != ClockResult::Ok ? checked : NativePedPoolReader::Revalidate(binding) ? ClockResult::Ok : ClockResult::UnsupportedBuild;
        }
        ClockResult ProbePedPoolHandle(std::uint32_t handle, bool& present, NativePedPoolReader::Value& value) noexcept override
        {
            const auto checked = CheckPedPoolAccess();
            return checked == ClockResult::Ok ? NativePedPoolReader(binding, &PoolTestHost::Owner).Probe(handle, present, value) : checked;
        }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& value) noexcept override
        {
            const auto checked = CheckPedPoolAccess();
            return checked == ClockResult::Ok ? NativePedPoolReader(binding, &PoolTestHost::Owner).Recheck(value) : checked;
        }
        ClockResult AcquirePedReference(std::uint32_t handle, NativePedReferenceToken& token) noexcept override
        {
            ++acquisitions;
            const auto checked = CheckPedPoolAccess();
            return checked == ClockResult::Ok ? controller->Acquire(memory, 42, handle, token, &lastNative) : checked;
        }
        ClockResult ResolvePedReference(const NativePedReferenceToken& token, NativePedPoolReader::Value& value) noexcept override
        {
            const auto checked = CheckPedPoolAccess();
            return checked == ClockResult::Ok ? controller->Resolve(memory, 42, token, value) : checked;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken& token) noexcept override
        {
            ++releases;
            if (beforeRelease)
                beforeRelease();
            // The real Runtime also uses its recovery admission here: loss of
            // ordinary borrowing does not authorize dropping retained cells.
            return controller->Release(memory, 42, token, &lastNative);
        }

    private:
        NativePedReferenceMemory::Binding binding;
    };
    struct ConstructionFailure
    {
        unsigned identity;
    };
    struct Construction
    {
        NativeGameTimingAccess* otherOwner = nullptr;
        bool                    changeToken = false, throwDuringConstruction = false;
        unsigned                entered = 0, constructed = 0, derivedDestroyed = 0, baseDestroyed = 0;
        std::function<void()>   afterConstruction, beforeDestruction;
    };
    class CompleteTestPed final : public pool_registry_fixture::Ped
    {
    public:
        CompleteTestPed(NativeGameTimingAccess& owner, const NativePedReferenceToken& token, Construction& hooks)
            : Ped(hooks.otherOwner ? *hooks.otherOwner : owner, SelectedToken(token, hooks), [&hooks] { ++hooks.baseDestroyed; }), hooks(hooks)
        {
            ++hooks.entered;
            if (hooks.afterConstruction)
                hooks.afterConstruction();
            if (hooks.throwDuringConstruction)
                throw ConstructionFailure{0xa531};
            ++hooks.constructed;
        }
        ~CompleteTestPed() override
        {
            if (hooks.beforeDestruction)
                hooks.beforeDestruction();
            ++hooks.derivedDestroyed;
        }

    private:
        static NativePedReferenceToken SelectedToken(NativePedReferenceToken token, const Construction& hooks)
        {
            if (hooks.changeToken)
                ++token.version;
            return token;
        }
        Construction& hooks;
    };
    using Factory = CPedBorrowedFactoryDE<CompleteTestPed>;
    using Phase = Factory::Phase;
    struct ClientIdentity
    {
        unsigned value;
    };
    CClientEntity* Client(ClientIdentity& identity)
    {
        // Opaque SDK association only; it is never invoked or dereferenced.
        return reinterpret_cast<CClientEntity*>(&identity);
    }
    NativePedReferenceCell::State CellSnapshot(const NativePedReferenceMemory::Receipt& native)
    {
        NativePedReferenceCell::State result;
        result.nativePed = native.nativePed;
        result.cellAddress = native.cell;
        result.node = native.node;
        return result;
    }
    void AcknowledgeExistingDebt(ReferenceFixture& fixture, LifecycleOwner& owner, const NativePedReferenceCell::State& cell)
    {
        const auto calls = ReferenceTestHost::unregisters;
        ReferenceTestHost::result = ClockResult::Ok;
        NativePedReferenceController::RetireProgress progress;
        Check(owner.controller->Retire(owner.memory, 42, 1, progress) != ClockResult::Ok && progress.retained == 1 && ReferenceTestHost::unregisters == calls,
              "existing End retains unresolved native bank debt without replaying registration or cleanup");
        fixture.Resolve(cell);
        Check(owner.controller->Retire(owner.memory, 42, 1, progress) == ClockResult::Ok && !owner.controller->IsRetained() &&
                  !owner.controller->RecoveryPending() && ReferenceTestHost::unregisters == calls,
              "only genuine full-bank cell-address absence acknowledges existing controller debt");
        CellFreed(cell.cellAddress);
    }
}

int main(int argc, char** argv)
{
    static_assert(!std::is_abstract_v<CompleteTestPed> && std::is_abstract_v<CPedVitalsDE> && std::is_abstract_v<CPoolsDE>);
    static_assert(!std::is_copy_constructible_v<Factory> && !std::is_move_constructible_v<Factory>);
    Check(argc >= 2 && std::filesystem::path(argv[1]).is_absolute(), "factory evidence uses an explicit workspace output folder");
    ReferenceFixture fixture(argv[1]);
    PoolTestHost::image = ReferenceTestHost::image = fixture.transform.image.image;
    const NativePedReferenceMemory::Binding binding{fixture.transform.image.image};
    ClientIdentity                          clientA{1}, clientB{2};
    if (argc == 3 && std::string(argv[2]) == "--unterminated")
    {
        std::set_terminate([] { std::_Exit(91); });
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        Factory                      factory(pool);
        Check(factory.Create(1, Client(clientA), hooks) == ClockResult::Ok, "seed exactly one explicit retirement contract violation in this owned child");
        std::puts("Intentional unretired borrowed factory contract check");
        std::fflush(stdout);
        return 0;  // Factory destructor must use the owned terminate handler.
    }
    {
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        Factory                      factory(pool);
        CPed*                        borrowed = reinterpret_cast<CPed*>(std::uintptr_t{1});
        Check(
            factory.Read(borrowed) == ClockResult::NotInitialized && borrowed == reinterpret_cast<CPed*>(std::uintptr_t{1}) && !owner.controller->IsRetained(),
            "a fresh CPU factory neither publishes a wrapper nor allocates another native reference registry");
        Check(factory.Create(1, Client(clientA), hooks) == ClockResult::Ok && owner.acquisitions == 1 && ReferenceTestHost::registers == 1 &&
                  owner.controller->IsRetained() && pool.GetPedCount() == 1 && hooks.constructed == 1,
              "one real acquired cell precedes typed construction and controlled registration in the canonical SDK pool");
        const auto state = factory.ReadReceipt();
        Check(state.phase == Phase::Live && state.acquired && state.constructed && state.registered && !state.releaseAttempted &&
                  factory.Read(borrowed) == ClockResult::Ok && pool.GetPedFromRef(1) == borrowed,
              "only the exact newly constructed same-owner token wrapper is published after actual native bookends");
        auto* pair = pool.GetPed(std::size_t{0});
        Check(pair->pEntity == borrowed && pair->pClientEntity == Client(clientA), "borrowed factory and CPools share one actual mutable SDK pair");
        Factory      competing(pool);
        Construction unused;
        Check(competing.Create(1, Client(clientB), unused) == ClockResult::Busy && owner.acquisitions == 1 && !unused.entered,
              "a controlled existing SDK registration refuses before acquiring a redundant cell or constructing a second wrapper");
        hooks.beforeDestruction = [&]
        {
            Check(!pair->pEntity && !pair->pClientEntity && pool.GetPedCount() == 0 && !owner.releases,
                  "canonical CPU registration is withdrawn before exact concrete SDK destruction");
            CPed* output = borrowed;
            Check(factory.Read(output) == ClockResult::Busy && factory.Retire() == ClockResult::Busy && output == borrowed,
                  "typed destruction cannot recursively republish or retire the same in-flight factory");
        };
        owner.beforeRelease = [&]
        {
            Check(hooks.derivedDestroyed == 1 && hooks.baseDestroyed == 1 && !pair->pEntity && factory.Retire() == ClockResult::Busy,
                  "native release follows complete typed destruction and recursive release admission is refused");
        };
        Check(factory.Retire() == ClockResult::Ok && owner.releases == 1 && ReferenceTestHost::unregisters == 1 && !owner.controller->IsRetained(),
              "explicit retirement performs one original-token release through the sole existing native controller");
        const auto retired = factory.ReadReceipt();
        Check(retired.phase == Phase::Retired && retired.wrapperDestroyed && retired.releaseAttempted && retired.release == ClockResult::Ok &&
                  factory.Retire() == ClockResult::Ok && owner.releases == 1 && ReferenceTestHost::unregisters == 1 &&
                  factory.Create(1, Client(clientA), hooks) == ClockResult::Busy,
              "completed retirement is observable and never replays creation, native registration or native release");
    }
    fixture.Reset();
    for (const auto refused : {ClockResult::WrongThread, ClockResult::Stopped, ClockResult::Expired, ClockResult::ReadFailure})
    {
        LifecycleOwner owner(binding);
        owner.admission = refused;
        pool_registry_fixture::Pools pool(owner);
        Factory                      factory(pool);
        Construction                 hooks;
        Check(factory.Create(1, nullptr, hooks) == refused && !owner.acquisitions && !owner.releases && !hooks.entered &&
                  factory.ReadReceipt().phase == Phase::Retired && !factory.ReadReceipt().acquired,
              "ordinary callback refusal never becomes a constructed empty wrapper or a native acquisition");
    }
    for (const auto handle : {std::uint32_t{2}, std::uint32_t{0xffffffff}, std::uint32_t{0x80}, std::uint32_t{1000 << 8}})
    {
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Factory                      factory(pool);
        Construction                 hooks;
        const auto                   expected = handle == 2 || handle == UINT32_MAX ? ClockResult::NotInitialized : ClockResult::InvalidArgument;
        Check(factory.Create(handle, nullptr, hooks) == expected && !owner.acquisitions && !owner.releases && !hooks.entered,
              "qualified native absence and unsupported crafted/upper-capacity handles stay distinct before acquisition");
    }
    for (const bool wrongOwner : {false, true})
    {
        fixture.Reset();
        LifecycleOwner               owner(binding), otherOwner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        hooks.changeToken = !wrongOwner;
        hooks.otherOwner = wrongOwner ? &otherOwner : nullptr;
        Factory factory(pool);
        Check(factory.Create(1, nullptr, hooks) == ClockResult::OwnerConflict && hooks.constructed == 1 && hooks.derivedDestroyed == 1 &&
                  hooks.baseDestroyed == 1 && pool.GetPedCount() == 0 && owner.acquisitions == 1 && owner.releases == 1 && !otherOwner.acquisitions &&
                  !otherOwner.releases && !owner.controller->IsRetained(),
              "even a complete concrete caller cannot substitute another token version or owner for the factory's acquired identity");
    }
    fixture.Reset();
    {
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        hooks.throwDuringConstruction = true;
        Factory factory(pool);
        bool    sameException = false;
        try
        {
            factory.Create(1, nullptr, hooks);
        }
        catch (const ConstructionFailure& error)
        {
            sameException = error.identity == 0xa531;
        }
        Check(sameException && hooks.entered == 1 && !hooks.constructed && !hooks.derivedDestroyed && hooks.baseDestroyed == 1 &&
                  factory.ReadReceipt().constructionThrew && !factory.ReadReceipt().constructed && factory.ReadReceipt().releaseAttempted &&
                  factory.ReadReceipt().release == ClockResult::Ok && owner.releases == 1 && !owner.controller->IsRetained() && !pool.GetPedCount(),
              "C++ construction failure preserves its exception identity and performs one cleanup attempt without publishing a partial wrapper");
    }
    fixture.Reset();
    {
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        Factory                      factory(pool);
        hooks.afterConstruction = [&]
        {
            CPed* output = nullptr;
            Check(factory.Create(1, nullptr, hooks) == ClockResult::Busy && factory.Retire() == ClockResult::Busy &&
                      factory.Read(output) == ClockResult::Busy && !owner.releases && owner.acquisitions == 1,
                  "recursive constructor admission cannot acquire or release the in-flight identity");
            owner.admission = ClockResult::WrongThread;
        };
        Check(factory.Create(1, nullptr, hooks) == ClockResult::WrongThread && hooks.derivedDestroyed == 1 && !pool.GetPedCount() && owner.releases == 1 &&
                  factory.ReadReceipt().release == ClockResult::Ok && !owner.controller->IsRetained(),
              "registration refusal after construction destroys only the typed original and uses the existing recovery release route");
    }
    fixture.Reset();
    {
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        Factory                      factory(pool);
        Check(factory.Create(1, Client(clientA), hooks) == ClockResult::Ok, "seed mutable SDK association lifetime boundary");
        auto* pair = pool.GetPed(std::size_t{0});
        CPed* original = pair->pEntity;
        pair->pClientEntity = Client(clientB);
        Check(factory.Retire() == ClockResult::BindingChanged && factory.ReadReceipt().phase == Phase::Withdrawing && pair->pEntity == original &&
                  pair->pClientEntity == Client(clientB) && !hooks.derivedDestroyed && !owner.releases && !factory.ReadReceipt().releaseAttempted &&
                  pool.GetPedCount() == 1,
              "a caller-mutated foreign client association retains its referenced typed wrapper and prevents premature native release");
        pair->pEntity = nullptr;
        pair->pClientEntity = nullptr;
        auto* copied = pool.GetPed(std::size_t{8});
        copied->pEntity = original;
        copied->pClientEntity = Client(clientB);
        Check(factory.Retire() == ClockResult::BindingChanged && !hooks.derivedDestroyed && !owner.releases && copied->pEntity == original &&
                  !factory.ReadReceipt().releaseAttempted,
              "withdrawal checks every canonical mutable row instead of overlooking a copied association at another native index");
        copied->pEntity = nullptr;
        copied->pClientEntity = nullptr;
        Check(factory.Retire() == ClockResult::BindingChanged && factory.ReadReceipt().phase == Phase::Retired && hooks.derivedDestroyed == 1 &&
                  owner.releases == 1 && !owner.controller->IsRetained() && pool.GetPedCount() == 1,
              "after explicit caller withdrawal, CPU retirement may finish once while preserving the first failure and untouched shared count semantics");
        pool.ResetPedPoolCount();
    }
    fixture.Reset();
    {
        LifecycleOwner               owner(binding);
        pool_registry_fixture::Pools pool(owner);
        Construction                 hooks;
        pool_registry_fixture::Ped   foreign(owner, NativePedReferenceToken{});
        Factory                      factory(pool);
        Check(factory.Create(1, Client(clientA), hooks) == ClockResult::Ok, "seed an independently replaced SDK pair");
        auto* pair = pool.GetPed(std::size_t{0});
        pair->pEntity = &foreign;
        pair->pClientEntity = Client(clientB);
        Check(factory.Retire() == ClockResult::BindingChanged && hooks.derivedDestroyed == 1 && owner.releases == 1 && pair->pEntity == &foreign &&
                  pair->pClientEntity == Client(clientB) && !owner.controller->IsRetained(),
              "typed retirement never clears, destroys or adopts a foreign replacement SDK pair");
        pair->pEntity = nullptr;
        pair->pClientEntity = nullptr;
        pool.ResetPedPoolCount();
    }
    fixture.Reset();
    {
        LifecycleOwner                owner(binding);
        pool_registry_fixture::Pools  pool(owner);
        Construction                  hooks;
        NativePedReferenceCell::State cell;
        {
            Factory factory(pool);
            Check(factory.Create(1, nullptr, hooks) == ClockResult::Ok, "seed a cleanup call with an uncertain accepted native prefix");
            ReferenceTestHost::result = ClockResult::OriginalException;
            ReferenceTestHost::leaveCellAddress = true;
            Check(factory.Retire() == ClockResult::OriginalException && factory.ReadReceipt().phase == Phase::Retired &&
                      factory.ReadReceipt().releaseAttempted && hooks.derivedDestroyed == 1 && !pool.GetPedCount() && owner.controller->IsRetained() &&
                      owner.controller->RecoveryPending() && owner.lastNative.entered,
                  "an entered release failure retains real controller debt after registry withdrawal and exact typed destruction");
            cell = CellSnapshot(owner.lastNative);
            const auto calls = ReferenceTestHost::unregisters;
            Check(factory.Retire() == ClockResult::OriginalException && owner.releases == 1 && ReferenceTestHost::unregisters == calls,
                  "a failed explicit release is never retried by ordinary factory retirement");
        }
        Check(owner.releases == 1 && ReferenceTestHost::unregisters == 1 && owner.controller->IsRetained(),
              "factory destruction performs no native cleanup replay and does not discard retained controller metadata");
        AcknowledgeExistingDebt(fixture, owner, cell);
    }
    fixture.Reset();
    {
        LifecycleOwner                owner(binding);
        pool_registry_fixture::Pools  pool(owner);
        Construction                  hooks;
        NativePedReferenceCell::State cell;
        ReferenceTestHost::result = ClockResult::OriginalException;
        {
            Factory factory(pool);
            Check(factory.Create(1, nullptr, hooks) == ClockResult::OriginalException && !factory.ReadReceipt().acquired &&
                      !factory.ReadReceipt().constructed && factory.ReadReceipt().phase == Phase::Retired && !hooks.entered && !owner.releases &&
                      owner.controller->IsRetained() && owner.controller->RecoveryPending(),
                  "uncertain acquisition leaves unpublished cell debt only in the sole original controller, never a partial wrapper/token");
            cell = CellSnapshot(owner.lastNative);
        }
        Check(!owner.releases && !ReferenceTestHost::unregisters, "a factory with no issued token cannot manufacture a destructor release");
        AcknowledgeExistingDebt(fixture, owner, cell);
    }
    fixture.Reset();
    PoolTestHost::image = ReferenceTestHost::image = 0;
    std::puts("Production typed borrowed ped factory lifetime fixture completed");
    std::puts("Production borrowed ped factory existing native controller fixture completed");
    std::printf("Passed %u actual borrowed ped factory checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
