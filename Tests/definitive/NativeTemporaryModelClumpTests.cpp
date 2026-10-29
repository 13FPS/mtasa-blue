/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Causal temporary-clump receipt and one-callback itinerary checks
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main HistoricalTemporaryClumpInitializerDefinitionsMain
#include "NativeStaticAssociationInitializeCallsTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeTemporaryModelClump.h"
#include "../../Client/game_de/NativeTemporaryModelClumpProfile.h"
#include <type_traits>
#include <thread>
namespace TemporaryClumpGateHost
{
    thread_local bool     holdBeforeGate = false;
    std::atomic<unsigned> phase{0};
}
namespace mta::game_de::NativeStreamingCompletionContext
{
    bool TemporaryClumpFixtureTryEnter() noexcept;
}
#define GetModuleHandleW  PoolTestHost::Module
#define ReadProcessMemory PoolTestHost::Read
#define TryEnter          TemporaryClumpFixtureTryEnter
#include "../../Client/game_de/NativeTemporaryModelClump.cpp"
#undef TryEnter
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace mta::game_de::NativeStreamingCompletionContext
{
    bool TemporaryClumpFixtureTryEnter() noexcept
    {
        if (TemporaryClumpGateHost::holdBeforeGate)
        {
            TemporaryClumpGateHost::phase.store(1, std::memory_order_release);
            while (TemporaryClumpGateHost::phase.load(std::memory_order_acquire) < 2)
                SwitchToThread();
            return false;
        }
        return TryEnter();
    }
}
namespace TemporaryClumpFixture
{
    namespace T = NativeTemporaryModelClumpCalls;
    // Keep the main's imported alias distinct from the reused pool fixture.
    namespace TempProfile = NativeTemporaryModelClumpProfile;
    using Record = NativeTemporaryModelClump;
    using State = Record::State;
    static_assert(!std::is_copy_constructible_v<Record> && !std::is_move_constructible_v<Record>);
    static_assert(!std::is_default_constructible_v<NativeTemporaryModelClumpBorrow> && !std::is_copy_constructible_v<NativeTemporaryModelClumpBorrow>);
    struct Fixture;
    Fixture*    active = nullptr;
    ClockResult Create(T::CreateFunction, std::uintptr_t, std::uintptr_t, std::uint32_t, std::int32_t, std::uintptr_t, std::uintptr_t&,
                       std::uint32_t&) noexcept;
    ClockResult Setup(T::SetupFunction, std::uintptr_t, std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&) noexcept;
    ClockResult Prepare(T::PrepareFunction, std::uintptr_t, std::uintptr_t, std::uint32_t&) noexcept;
    ClockResult Retire(T::RetireFunction, std::uintptr_t, bool, bool&, std::uint32_t&) noexcept;
    ClockResult Row(const NativeTemporaryModelClumpBorrow&, void*) noexcept;
    struct Fixture : InitializerFixture
    {
        NativeModelBankReader::Value model;
        T::Invocations               invocations;
        Record*                      issued = nullptr;
        unsigned                     mode = 0, createCalls = 0, setupCalls = 0, prepareCalls = 0, rowCalls = 0, retireCalls = 0;
        std::array<unsigned, 5>      order{};
        std::size_t                  orderCount = 0;
        ClockResult                  nested{ClockResult::Ok};
        std::array<unsigned char, 4> tail{};
        explicit Fixture(const std::filesystem::path& path) : InitializerFixture(path) { Reset(); }
        void Reset()
        {
            InitializerFixture::Reset();
            active = this;
            const auto image = transform.image.image;
            for (const auto& pin : TempProfile::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const unsigned char ret = 0xc3, opaque = 0x5a;
            transform.image.Patch(TempProfile::SetupRva, &ret, 1, PAGE_EXECUTE_READ);
            transform.image.Patch(TempProfile::OpaqueDefaultRva, &opaque, 1, PAGE_READONLY);
            const auto creator = image + TempProfile::CreateRva;
            transform.image.Patch(0x4301000 + TempProfile::CreateSlot, &creator, sizeof(creator), PAGE_READONLY);
            Global(TempProfile::SetupClassSlotRva, Address(native.actorClass.data()));
            F::Store(metadata, TempProfile::FlagsOffset, std::uint16_t{0});
            F::Store(native.container.data(), 0x20, std::uintptr_t{0});
            model = {0, image + NativeModelBankProfile::BankRva, Address(metadata)};
            invocations = {&Create, &Setup, &Prepare, &Retire};
            issued = nullptr;
            mode = createCalls = setupCalls = prepareCalls = rowCalls = retireCalls = 0;
            order.fill(0);
            orderCount = 0;
            nested = ClockResult::Ok;
        }
        ClockResult Run(Record& record, T::Receipt& receipt)
        {
            issued = &record;
            return T::WithTemporary(transform.image.image, record, 17, model, &PoolTestHost::Owner, &TemporaryClumpFixture::Row, this, &receipt, &invocations);
        }
        void Mark(unsigned value) noexcept
        {
            Check(orderCount < order.size(), "temporary itinerary has no hidden extra native boundary");
            order[orderCount++] = value;
        }
    };
    ClockResult Create(T::CreateFunction entry, std::uintptr_t model, std::uintptr_t opaque, std::uint32_t zero, std::int32_t minusOne,
                       std::uintptr_t stackZero, std::uintptr_t& output, std::uint32_t& exception) noexcept
    {
        auto& f = *active;
        ++f.createCalls;
        f.Mark(1);
        Check(reinterpret_cast<std::uintptr_t>(entry) == f.transform.image.image + TempProfile::CreateRva && model == f.Address(f.metadata) &&
                  opaque == f.transform.image.image + TempProfile::OpaqueDefaultRva && zero == 0 && minusOne == -1 && stackZero == 0,
              "current original model vslot receives exact five-argument source tuple, including signed minus one");
        Check(f.issued->GetState() == State::CreateEntered && f.issued->HasOutstandingResource(),
              "issued record owns an unknown original create outcome before entry");
        if (f.mode == 3)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        output = (f.mode == 1 || f.mode == 2) ? 0 : f.native.Clump();
        if (f.mode == 2 || f.mode == 4)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (f.mode == 15)
        {
            // Simulate only the opaque wrapper's already-created container.
            // Production still calls that whole wrapper then explicit FFBDF0;
            // no fixture pretends to execute its internal blend/publication.
            Check(F::Load<std::uint16_t>(f.metadata, TempProfile::FlagsOffset) == 0x100, "original wrapper sees its unchanged current model flag branch");
        }
        if (f.mode == 19)
        {
            f.model = {};
            f.invocations.setup = nullptr;
            f.invocations.prepare = nullptr;
            f.invocations.retire = nullptr;
        }
        return ClockResult::Ok;
    }
    ClockResult Setup(T::SetupFunction entry, std::uintptr_t clump, std::uintptr_t opaque, std::uint32_t zero, std::uint32_t secondZero,
                      std::uint32_t& exception) noexcept
    {
        auto& f = *active;
        ++f.setupCalls;
        f.Mark(2);
        Check(reinterpret_cast<std::uintptr_t>(entry) == f.transform.image.image + TempProfile::SetupRva && clump == f.native.Clump() &&
                  opaque == f.transform.image.image + TempProfile::OpaqueDefaultRva && !zero && !secondZero,
              "conditional original class setup keeps its opaque source tuple");
        if (f.mode == 5)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
    ClockResult Prepare(T::PrepareFunction entry, std::uintptr_t clump, std::uintptr_t model, std::uint32_t& exception) noexcept
    {
        auto& f = *active;
        ++f.prepareCalls;
        f.Mark(3);
        Check(reinterpret_cast<std::uintptr_t>(entry) == f.transform.image.image + TempProfile::PrepareRva && clump == f.native.Clump() &&
                  model == f.Address(f.metadata),
              "explicit original FFBDF0 receives same causal actor and captured current model");
        if (f.mode == 6)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        if (f.mode == 7)
            F::Store(f.native.container.data(), 0x20, f.Address(f.transform.pool.objects) + 0x6e4);
        if (f.mode == 8)
            F::Store(f.native.container.data(), 0x18, clump + 8);
        if (f.mode == 9)
            F::Store(f.native.RenderItem(), 0x10, std::uint32_t{9000});
        if (f.mode == 17)
        {
            F::Store(f.native.container.data(), 0x10, std::int32_t{0});
            F::Store(f.native.container.data(), 0x988, std::int32_t{0});
        }
        return ClockResult::Ok;
    }
    ClockResult Row(const NativeTemporaryModelClumpBorrow&, void* context) noexcept
    {
        auto& f = *static_cast<Fixture*>(context);
        ++f.rowCalls;
        f.Mark(4);
        Check(f.issued->GetState() == State::RowEntered && f.issued->HasOutstandingResource(),
              "typed row callback receives only a current synchronous issued borrow");
        if (f.mode == 10)
            return ClockResult::BindingChanged;
        if (f.mode == 11)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (f.mode == 18)
        {
            Record     other;
            T::Receipt nested;
            f.nested =
                T::WithTemporary(f.transform.image.image, other, 17, f.model, &PoolTestHost::Owner, &TemporaryClumpFixture::Row, &f, &nested, &f.invocations);
            Check(f.nested == ClockResult::Busy && other.CanDiscard() && !nested.createEntered,
                  "row callback cannot reenter another itinerary through the shared existing gate");
            Check(T::WithTemporary(0, *f.issued, 0, f.model, nullptr, nullptr, nullptr, &nested) == ClockResult::Busy &&
                      f.issued->GetState() == State::RowEntered,
                  "invalid nested same-record calls do not clear the active private borrow or debt phase");
        }
        if (f.mode == 22)
        {
            TemporaryClumpGateHost::phase.store(2, std::memory_order_release);
            while (TemporaryClumpGateHost::phase.load(std::memory_order_acquire) < 3)
                SwitchToThread();
            Check(f.issued->GetState() == State::RowEntered && f.issued->HasOutstandingResource(),
                  "losing pre-gate caller cannot mutate the winning issued record or clear its live private borrow");
        }
        return ClockResult::Ok;
    }
    ClockResult Retire(T::RetireFunction entry, std::uintptr_t clump, bool flag, bool& accepted, std::uint32_t& exception) noexcept
    {
        auto& f = *active;
        ++f.retireCalls;
        f.Mark(5);
        Check(reinterpret_cast<std::uintptr_t>(entry) == f.transform.image.image + TempProfile::RetireRva && clump == f.native.Clump() && !flag,
              "retirement uses original deferred actor routine with source false argument, never a native heap free");
        if (f.mode == 12)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return ClockResult::OriginalException;
        }
        accepted = f.mode != 13;
        if (f.mode == 14)
            PoolTestHost::ownerResult = ClockResult::Expired;
        // A genuine accepted deferred result can leave allocation/registry in
        // place. The test intentionally does not destroy either fixture object.
        return ClockResult::Ok;
    }
}
int main(int argc, char** argv)
{
    using namespace TemporaryClumpFixture;
    Check(argc == 3 && std::filesystem::path(argv[1]).is_absolute(), "temporary-clump fixture uses task-owned evidence and one selected phase");
    const std::string phase = argv[2];
    Fixture           fixture{std::filesystem::path(argv[1])};
    const auto        firstCheck = checks;
    if (phase == "happy")
    {
        for (unsigned mode : {0u, 1u, 15u, 16u, 17u, 18u, 19u, 20u, 21u})
        {
            fixture.Reset();
            fixture.mode = mode;
            if (mode == 15)
                F::Store(fixture.metadata, TempProfile::FlagsOffset, std::uint16_t{0x100});
            if (mode == 16)
                F::Store(fixture.native.actorClass.data(), 0x38, std::int32_t{2});
            if (mode == 20)
            {
                const std::array<unsigned char, 3> otherType{0xb0, 2, 0xc3};
                fixture.transform.image.Patch(F::ModelTypeRva, otherType.data(), otherType.size(), PAGE_EXECUTE_READ);
            }
            if (mode == 21)
                fixture.Global(NativeModelLoadedProfile::ArrayRva + NativeModelLoadedProfile::FlagsOffset + 1, static_cast<unsigned char>(0));
            Record     record;
            T::Receipt receipt;
            Check(fixture.Run(record, receipt) == ClockResult::Ok && receipt.verified && record.CanDiscard() && !receipt.debt,
                  "one synchronous source itinerary completes or acknowledges a null original return");
            if (mode == 1)
                Check(receipt.returnedNull && record.GetState() == State::ReturnedNull && fixture.orderCount == 1 && !receipt.setupEntered &&
                          !receipt.prepareEntered && !receipt.rowEntered && !receipt.retireEntered,
                      "acknowledged original null result is not adopted, prepared, initialized or retired");
            else
            {
                Check(receipt.containerObserved && receipt.retireAccepted && record.GetState() == State::RetiredDeferred && fixture.createCalls == 1 &&
                          fixture.prepareCalls == 1 && fixture.rowCalls == 1 && fixture.retireCalls == 1 && fixture.setupCalls == (mode == 16 ? 0u : 1u) &&
                          fixture.native.Clump() == receipt.clump && receipt.frameCount == (mode == 17 ? 0 : 2),
                      "original setup condition, explicit prepare, temporary owner-zero container and deferred acknowledgement survive intact");
                Check(fixture.order[0] == 1 && fixture.order[fixture.orderCount - 3] == 3 && fixture.order[fixture.orderCount - 2] == 4 &&
                          fixture.order[fixture.orderCount - 1] == 5,
                      "source create/setup/prepare/row/retire order remains exact");
                Check(F::Load<std::uintptr_t>(fixture.native.RenderItem(), 0) == receipt.clump &&
                          F::Load<std::uintptr_t>(fixture.native.container.data(), 0x18) == receipt.clump,
                      "accepted retirement is not misreported as allocation or registry disappearance");
            }
            const auto calls = fixture.createCalls;
            Check(fixture.Run(record, receipt) == ClockResult::Busy && fixture.createCalls == calls,
                  "completed issued records cannot be reused to recreate or replay original native entries");
        }
    }
    else if (phase == "race")
    {
        fixture.Reset();
        fixture.mode = 22;
        Record      record;
        T::Receipt  winnerReceipt, loserReceipt;
        ClockResult losingResult{ClockResult::Ok};
        TemporaryClumpGateHost::phase.store(0, std::memory_order_release);
        std::thread loser(
            [&]
            {
                TemporaryClumpGateHost::holdBeforeGate = true;
                losingResult = T::WithTemporary(fixture.transform.image.image, record, 17, fixture.model, &PoolTestHost::Owner, &TemporaryClumpFixture::Row,
                                                &fixture, &loserReceipt, &fixture.invocations);
                TemporaryClumpGateHost::phase.store(3, std::memory_order_release);
            });
        while (TemporaryClumpGateHost::phase.load(std::memory_order_acquire) < 1)
            SwitchToThread();
        const auto result = fixture.Run(record, winnerReceipt);
        // A pre-row winner refusal must still release/join the owned worker.
        TemporaryClumpGateHost::phase.store(2, std::memory_order_release);
        loser.join();
        Check(result == ClockResult::Ok && winnerReceipt.verified && winnerReceipt.retireAccepted && record.CanDiscard(),
              "winner completes original itinerary after deterministic losing caller reaches pre-gate state");
        Check(losingResult == ClockResult::Busy && !loserReceipt.createEntered && !loserReceipt.debt && fixture.createCalls == 1,
              "nonowning Busy acknowledgement does not copy native resource debt or invoke native entries");
    }
    else if (phase == "preentry")
    {
        for (unsigned fault = 0; fault != 8; ++fault)
        {
            fixture.Reset();
            if (fault == 0)
                PoolTestHost::ownerResult = ClockResult::Expired;
            if (fault == 1)
                fixture.Global(TempProfile::SetupClassSlotRva, std::uintptr_t{0});
            if (fault == 2)
            {
                const unsigned char wrong = 0x90;
                fixture.transform.image.Patch(TempProfile::CreateRva, &wrong, 1, PAGE_EXECUTE_READ);
            }
            if (fault == 3)
            {
                const auto wrong = fixture.transform.image.image + TempProfile::CreateRva + 1;
                fixture.transform.image.Patch(0x4301000 + TempProfile::CreateSlot, &wrong, sizeof(wrong), PAGE_READONLY);
            }
            if (fault == 4)
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
            if (fault == 5)
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
            if (fault == 6)
                fixture.model.slotAddress += 8;
            if (fault == 7)
            {
                fixture.model.nativeModelInfo = 0;
                fixture.Global(NativeModelBankProfile::BankRva, std::uintptr_t{0});
            }
            Record     record;
            T::Receipt receipt;
            Check(fixture.Run(record, receipt) != ClockResult::Ok && !receipt.createEntered && record.CanDiscard() && record.GetState() == State::Empty &&
                      !fixture.createCalls,
                  "owner/cache/code/slot/world/GC failures refuse before any causal actor is issued");
        }
    }
    else if (phase == "unknown")
    {
        for (unsigned mode : {2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u})
        {
            fixture.Reset();
            fixture.mode = mode;
            Record     record;
            T::Receipt receipt;
            Check(fixture.Run(record, receipt) != ClockResult::Ok && receipt.createEntered && !receipt.verified,
                  "entered refusal retains exact progress instead of inventing a complete temporary-clump operation");
            const bool discharged = mode == 2 || mode == 14;
            Check(record.CanDiscard() == discharged && receipt.debt == !discharged &&
                      record.GetState() == (mode == 2    ? State::ReturnedNull
                                            : mode == 14 ? State::RetiredDeferred
                                                         : State::FaultDebt),
                  "only original null or accepted deferred retirement discharges actual resource debt, including post-ack owner loss");
            if (mode >= 4)
                Check(receipt.createReturned && receipt.clump == fixture.native.Clump(),
                      "known nonnull original return is journaled before every later fallible check");
            if (mode == 7 || mode == 8 || mode == 9)
                Check(!receipt.rowEntered && !fixture.rowCalls && !fixture.retireCalls,
                      "forged ped-owner flavor, wrong container backpointer and reused actor identity cannot reach a row initializer");
            const auto calls = fixture.createCalls;
            Check(fixture.Run(record, receipt) == ClockResult::Busy && fixture.createCalls == calls,
                  "entered unknown/deferred outcomes never retry create, row init, retirement or arbitrary free");
        }
    }
    else
        Check(false, "unknown temporary-clump phase refused");
    std::puts("Production synchronous temporary model clump fixture completed");
    std::printf("Passed %u native temporary-clump checks (64-bit; no native engine code executed)\n", checks - firstCheck);
    return 0;
}
