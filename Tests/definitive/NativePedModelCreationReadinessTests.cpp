/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Qualify creation readiness without entering native creation effects
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main PedReadinessHistoricalCreationDefinitionsMain
#include "NativePedModelCreationTests.cpp"
#undef main
#pragma pop_macro("main")
#include <game/CGameModelPreparationPolicy.h>

namespace PedReadinessTests
{
    namespace Creation = NativePedModelCreation;
    namespace Heap = NativeModelReferenceTransactionProfile;
    namespace Allocator = NativeCustomAnimSequenceStorageProfile;
    using Fixture = PedCreationTests::Fixture;
    Fixture* fixture = nullptr;
    void     ChangeDefault()
    {
        const unsigned char changed = 0x64;
        fixture->transform.image.Patch(NativePedModelMetadataProfile::DefaultCollisionRva + 0x29, &changed, 1, PAGE_READWRITE);
    }
    void ChangeHeap()
    {
        const std::uintptr_t absent = 0;
        fixture->transform.image.Patch(Heap::HeapRva, &absent, sizeof(absent), PAGE_READWRITE);
    }
}

int main(int argc, char** argv)
{
    namespace T = PedReadinessTests;
    namespace Creation = T::Creation;
    using Signature = ClockResult (*)(std::uintptr_t, Creation::CheckOwner) noexcept;
    static_assert(std::is_same_v<decltype(&Creation::CheckReady), Signature>);
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "readiness fixture evidence stays in its owned workspace directory");
    T::Fixture fixture(argv[1]);
    T::fixture = &fixture;
    PedCreationTests::fixture = &fixture;
    PedCreationTests::HeapHost::State host;
    PedCreationTests::HeapHost::active = &host;
    const auto image = fixture.Image();
    const auto reset = [&]
    {
        fixture.ResetDefaultCollision();
        host.Reset(image, &SpecialFixture::Patch, &fixture);
        PedCreationTests::SeedVtable();
        // The factory must be able to create these rows. Make every future
        // startup slot absent rather than supplying44 preexisting ped objects.
        MtaGame::CGameModelPreparationPolicy::SetupSpecialCharacters([&](std::uint32_t key, const char*) { fixture.Slot(key, 0); });
        PedCreationTests::writes = PedCreationTests::exchanges = 0;
    };
    const auto noEffects = [&]
    {
        Check(!host.mallocCalls && !host.freeCalls && !host.allocationCount && !PedCreationTests::writes && !PedCreationTests::exchanges,
              "readiness never enters allocator/free/default-writer/publication or creates issued allocation debt");
        MtaGame::CGameModelPreparationPolicy::SetupSpecialCharacters(
            [&](std::uint32_t key, const char*)
            {
                std::uintptr_t value = 1;
                std::memcpy(&value, reinterpret_cast<const void*>(image + NativeModelBankProfile::BankRva + key * NativeModelBankProfile::SlotBytes),
                            sizeof(value));
                Check(!value, "each of44 future bank rows remains absent and untouched by readiness");
            });
    };
    const auto gateReleased = [&]
    {
        Check(NativeStreamingCompletionContext::TryEnter(), "success and refusal both release only readiness's transient shared CPU gate");
        NativeStreamingCompletionContext::Leave();
    };
    const auto ready = [&] { return Creation::CheckReady(image, &PoolTestHost::Owner); };
    reset();
    Check(ready() == ClockResult::Ok, "qualified current world/heap/default accepts readiness even with all future model rows absent");
    const auto ownerChecks = PoolTestHost::ownerCalls;
    const auto memoryReads = PoolTestHost::reads;
    Check(ownerChecks > 2 && memoryReads > 2, "readiness executes real repeated owner/profile/world/default/heap qualification");
    Check(ready() == ClockResult::Ok, "a repeated explicit readiness observation has no attempt or once state");
    noEffects();
    gateReleased();
    Check(Creation::CheckReady(0, &PoolTestHost::Owner) == ClockResult::NotInitialized && Creation::CheckReady(image, nullptr) == ClockResult::NotInitialized,
          "missing image/owner cannot claim readiness or enter an effect");
    Check(Creation::CheckReady((std::numeric_limits<std::uintptr_t>::max)(), &PoolTestHost::Owner) == ClockResult::BindingChanged,
          "overflowing image extent refuses before native observation");
    noEffects();
    for (const auto status : {ClockResult::WrongThread, ClockResult::Expired, ClockResult::OwnerConflict, ClockResult::PatchFailure})
    {
        reset();
        PoolTestHost::ownerResult = status;
        Check(ready() == status, "the exact missing current owner status propagates before qualification");
        noEffects();
        gateReleased();
    }
    reset();
    Check(NativeStreamingCompletionContext::TryEnter(), "fixture explicitly owns the competing CPU completion gate");
    Check(ready() == ClockResult::Busy, "readiness cannot borrow another operation's active shared scope");
    NativeStreamingCompletionContext::Leave();
    noEffects();
    gateReleased();

    // These are mutations of owned test snapshots, never calls into copied
    // game instructions or probes that prepare real native dependencies.
    reset();
    const unsigned char changedCode = 0xcc;
    fixture.transform.image.Patch(T::Allocator::AuthorityMallocRva, &changedCode, 1, PAGE_EXECUTE_READ);
    Check(ready() == ClockResult::BindingChanged, "allocator entry bytes are required in addition to transaction heap slots");
    noEffects();
    gateReleased();
    reset();
    const std::uintptr_t zero = 0;
    fixture.transform.image.Patch(T::Heap::HeapAuthorityRva, &zero, sizeof(zero), PAGE_READWRITE);
    Check(ready() == ClockResult::NotInitialized, "missing constructor-published heap authority cannot be prepared by readiness");
    noEffects();
    gateReleased();
    reset();
    fixture.transform.image.Patch(NativePedModelMetadataProfile::DefaultCollisionRva + 0x30, &zero, sizeof(zero), PAGE_READWRITE);
    Check(ready() != ClockResult::Ok, "missing completed canonical collision cannot become a null default success");
    noEffects();
    gateReleased();
    reset();
    fixture.transform.image.Patch(NativePedModelMetadataProfile::VtableRva + 0x20, &zero, sizeof(zero), PAGE_READONLY);
    Check(ready() == ClockResult::BindingChanged, "the exact future installed ped vtable must already be qualified");
    noEffects();
    gateReleased();
    reset();
    PoolTestHost::watched = fixture.Canonical();
    PoolTestHost::after = &T::ChangeDefault;
    Check(ready() != ClockResult::Ok, "canonical collision mutation between guarded snapshots refuses readiness");
    noEffects();
    gateReleased();
    reset();
    PoolTestHost::watched = image + T::Heap::HeapRva;
    PoolTestHost::after = &T::ChangeHeap;
    Check(ready() != ClockResult::Ok, "current heap identity changing during qualification is caught by its bookend");
    noEffects();
    gateReleased();
    for (const auto at : {1u, ownerChecks / 2u, ownerChecks})
    {
        reset();
        PoolTestHost::revokeAt = at;
        Check(ready() != ClockResult::Ok, "owner revocation at early/middle/final admission refuses without reserving a creation record");
        noEffects();
        gateReleased();
    }
    for (const auto at : {1u, memoryReads / 2u, memoryReads})
    {
        reset();
        PoolTestHost::failAt = at;
        Check(ready() != ClockResult::Ok, "early/middle/final guarded observation faults cannot be mistaken for readiness");
        noEffects();
        gateReleased();
    }
    PedCreationTests::HeapHost::active = nullptr;
    std::puts("Production native ped model creation readiness fixture completed");
    std::printf("Passed %u native ped model creation readiness checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
