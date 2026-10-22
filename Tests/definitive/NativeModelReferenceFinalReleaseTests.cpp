/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Selected-key final decrement and actual removal continuation intent
 ****************************************************************************/
#pragma push_macro("main")
#undef main
#define main NativeFinalReleaseHistoricalRemoveMain
#include "NativeStreamingRemoveTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativeModelReferenceRelease.h"
namespace
{
    ClockResult ExchangeFinal(std::uintptr_t, std::uint32_t, std::uint32_t, std::uint32_t&, std::uint32_t&) noexcept;
}
#define GetModuleHandleW                        PoolTestHost::Module
#define ReadProcessMemory                       PoolTestHost::Read
#define MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE ::ExchangeFinal
#include "../../Client/game_de/NativeModelReferenceRelease.cpp"
#undef MTA_DE_MODEL_REFERENCE_RELEASE_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace FR = NativeModelReferenceRelease;
    RemoveFixture* finalFixture = nullptr;
    unsigned       finalCalls = 0, finalMode = 0;
    std::int32_t   finalIndex = 1;
    ClockResult    ExchangeFinal(std::uintptr_t at, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++finalCalls;
        auto& f = *finalFixture;
        Check(at == f.Address(f.references) + static_cast<std::uintptr_t>(finalIndex) * RP::RowBytes + 4 && expected == 1 && target == 0,
              "final exchange receives exactly the current occupied selected DWORD, expected1 and target0");
        if (finalMode == 1)
        {
            exception = 0xe05c0001;
            return ClockResult::OriginalException;
        }
        if (finalMode == 2)
        {
            observed = 9;
            std::memcpy(reinterpret_cast<void*>(at), &observed, 4);
            return ClockResult::Ok;
        }
        LONG e = 0, t = 0;
        std::memcpy(&e, &expected, 4);
        std::memcpy(&t, &target, 4);
        const auto previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), t, e);
        std::memcpy(&observed, &previous, 4);
        if (finalMode == 3)
            PoolTestHost::ownerResult = ClockResult::Expired;
        if (finalMode == 4)
            f.Global(SP::RootRva, std::uintptr_t{0});
        if (finalMode == 5)
            f.Row(finalIndex, 4, std::uint32_t{7});
        if (finalMode == 6)
            f.Header(0x10, std::uint32_t{5});
        if (finalMode == 7)
            f.Stream(f.selectedKey, 0x40, 2);
        if (finalMode == 8)
            f.Global(AP::HeapRva, std::uintptr_t{0});
        if (finalMode == 9)
            f.Put(f.metadata, 0x58, std::uintptr_t{0});
        if (finalMode == 10)
            f.Put(f.assetRows, 8, std::uint16_t{0});
        if (finalMode == 11)
        {
            exception = 0xe05c0002;
            return ClockResult::OriginalException;
        }
        return ClockResult::Ok;
    }
    std::vector<unsigned char> FinalMapBytes(const RemoveFixture& f)
    {
        std::vector<unsigned char> bytes(RP::HeaderBytes + 4 * RP::RowBytes);
        std::memcpy(bytes.data(), f.Map(), RP::HeaderBytes);
        std::memcpy(bytes.data() + RP::HeaderBytes, f.references, 4 * RP::RowBytes);
        return bytes;
    }
    void FinalMapOnlyZero(const RemoveFixture& f, std::vector<unsigned char> before)
    {
        const std::uint32_t zero = 0;
        std::memcpy(before.data() + RP::HeaderBytes + static_cast<std::size_t>(finalIndex) * RP::RowBytes + 4, &zero, 4);
        Check(FinalMapBytes(f) == before, "accepted final decrement changes only the DWORD; occupied key, hash and free-list remain intact");
    }
}
int main(int argc, char** argv)
{
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "final-release evidence stays in its workspace-owned directory");
    RemoveFixture f(argv[1]);
    finalFixture = activeRemove = &f;
    activeAcquire = &f;
    activeReference = &f;
    const auto image = f.transform.image.image;
    PoolTestHost::image = image;
    FR::Receipt receipt;
    const auto  reset = [&](std::uint32_t key = 17)
    {
        f.Reset(key);
        // Reference rows and asset rows are distinct native sparse maps. Keep
        // both on independent inline bits/hash storage in this focused host.
        f.ReferenceFixture::Shape(4, 1, false, 1, key == 0 || key == B::ModelCount - 1 ? 17 : key);
        finalIndex = key == 0 ? 0 : key == B::ModelCount - 1 ? 2 : 1;
        f.Row(finalIndex, 4, std::uint32_t{1});
        finalCalls = finalMode = removeCalls = removeMode = 0;
        passedRemove = nullptr;
    };
    const auto run = [&](FR::ZeroPolicy policy, std::uint32_t key = 17) { return FR::ReleaseFinal(image, key, policy, &PoolTestHost::Owner, &receipt); };
    for (const auto key : {std::uint32_t{0}, std::uint32_t{17}, std::uint32_t{1000}, std::uint32_t{1193}, B::ModelCount - 1})
        for (const auto policy : {FR::ZeroPolicy::KeepResident, FR::ZeroPolicy::RemoveAtZero})
        {
            reset(key);
            const auto before = FinalMapBytes(f);
            const auto stream = f.Stream(key);
            const auto result = run(policy, key);
            Check(result == ClockResult::Ok && receipt.reference.entered && receipt.reference.accepted && finalCalls == 1 && receipt.reference.before == 1 &&
                      receipt.reference.target == 0 && f.Count(finalIndex) == 0,
                  "one selected-key final DWORD decrement includes upgrades and leaves CJ gating to actual MTA policy");
            FinalMapOnlyZero(f, before);
            if (policy == FR::ZeroPolicy::KeepResident)
                Check(!receipt.removalPrepared && !receipt.removal.entered && !removeCalls && f.Stream(key) == stream,
                      "explicit custom-resource keep intent accepts zero without entering native removal");
            else
                Check(receipt.removalPrepared && receipt.removal.entered && receipt.removal.returned && removeCalls == 1 && f.Stream(key)[1] == 0,
                      "explicit eligible final-release intent calls genuine qualified native removal exactly once");
        }
    reset();
    f.Global(AP::HeapRva, std::uintptr_t{0});
    Check(run(FR::ZeroPolicy::KeepResident) == ClockResult::Ok && finalCalls == 1 && !removeCalls,
          "KeepResident does not acquire irrelevant native removal heap prerequisites");
    reset();
    f.Global(AP::HeapRva, std::uintptr_t{0});
    auto before = FinalMapBytes(f);
    Check(run(FR::ZeroPolicy::RemoveAtZero) != ClockResult::Ok && !finalCalls && !removeCalls && FinalMapBytes(f) == before,
          "complete removal heap admission precedes accepting zero when caller requests removal");
    for (const auto count : {std::uint32_t{0}, std::uint32_t{2}, std::uint32_t{0x7fffffff}, std::uint32_t{0x80000000}, std::uint32_t{0xffffffff}})
    {
        reset();
        f.Row(1, 4, count);
        before = FinalMapBytes(f);
        Check(run(FR::ZeroPolicy::KeepResident) == ClockResult::InvalidArgument && !receipt.reference.entered && !finalCalls && !removeCalls &&
                  FinalMapBytes(f) == before,
              "final-only route refuses zero underflow and every nonfinal count rather than changing interior semantics");
    }
    reset();
    before = FinalMapBytes(f);
    Check(FR::ReleaseFinal(image, B::ModelCount, FR::ZeroPolicy::KeepResident, &PoolTestHost::Owner, &receipt) == ClockResult::InvalidArgument &&
              !receipt.reference.entered,
          "final route keeps full DWORD model bounds");
    Check(run(static_cast<FR::ZeroPolicy>(2)) == ClockResult::InvalidArgument && !receipt.reference.entered,
          "unknown custom-policy intent cannot authorize removal");
    Check(FR::ReleaseFinal(image, 17, FR::ZeroPolicy::KeepResident, nullptr, &receipt) == ClockResult::NotInitialized && !receipt.reference.entered &&
              FinalMapBytes(f) == before,
          "callable reader alone grants no final-release authority");
    for (const auto state : {static_cast<unsigned char>(0), static_cast<unsigned char>(2), static_cast<unsigned char>(3), static_cast<unsigned char>(4)})
    {
        reset();
        f.Stream(17, 0x40, state);
        before = FinalMapBytes(f);
        Check(run(FR::ZeroPolicy::RemoveAtZero) != ClockResult::Ok && !finalCalls && !removeCalls && FinalMapBytes(f) == before,
              "pending or absent stream state remains outside this final loaded-model transaction");
    }
    for (unsigned mode = 1; mode <= 11; ++mode)
    {
        reset();
        finalMode = mode;
        const auto result = run(FR::ZeroPolicy::RemoveAtZero);
        Check(result != ClockResult::Ok && receipt.reference.entered && finalCalls == 1 && !removeCalls,
              "fault, count competition, owner/root/map/state/heap/bundle/asset loss never replay or reach an unsafe removal");
        const auto expected = mode == 1 ? 1u : mode == 2 ? 9u : mode == 5 ? 7u : 0u;
        Check(f.Count(finalIndex) == expected && receipt.reference.accepted == (mode >= 3 && mode <= 10),
              "accepted zero and foreign count effects persist on later refusal without synthetic rollback");
        if (mode == 1)
            Check(receipt.reference.exceptionCode == 0xe05c0001, "final exchange preserves its exact native exception receipt");
        if (mode == 11)
            Check(receipt.reference.exceptionCode == 0xe05c0002 && f.Count(finalIndex) == 0,
                  "fault returned after an actual zero store preserves unknown partial effects despite no success acknowledgement");
    }
    for (unsigned mode = 1; mode <= 7; ++mode)
    {
        reset();
        removeMode = mode;
        const auto result = run(FR::ZeroPolicy::RemoveAtZero);
        Check(result != ClockResult::Ok && receipt.reference.accepted && receipt.removal.entered && finalCalls == 1 && removeCalls == 1 && f.Count(1) == 0,
              "entered removal failure preserves final zero and native partial effects, with exactly one entry of each operation");
        if (mode <= 2)
            Check(receipt.removal.exceptionCode == (mode == 1 ? 0xe05a0001u : 0xe05a0002u), "removal exception is distinct from final exchange status");
    }
    reset();
    Check(run(FR::ZeroPolicy::RemoveAtZero) == ClockResult::Ok, "measure actual current-owner final-release path");
    const auto owners = PoolTestHost::ownerCalls;
    Check(owners > 0, "final route used real current-owner admission");
    unsigned acceptedExpiries = 0, enteredRemovals = 0;
    for (unsigned step = 1; step <= owners; ++step)
    {
        reset();
        PoolTestHost::revokeAt = step;
        const auto result = run(FR::ZeroPolicy::RemoveAtZero);
        Check(result != ClockResult::Ok && finalCalls <= 1 && removeCalls <= 1, "every owner bookend refuses revoked final intent without a replay");
        Check(f.Count(1) == (receipt.reference.accepted ? 0u : 1u), "owner expiry retains exactly the accepted counter effect");
        acceptedExpiries += receipt.reference.accepted ? 1u : 0u;
        enteredRemovals += receipt.removal.entered ? 1u : 0u;
    }
    Check(acceptedExpiries && enteredRemovals, "owner coverage includes accepted-zero and entered-removal failure boundaries");
    reset();
    before = FinalMapBytes(f);
    ClockResult foreignResult = ClockResult::Ok;
    std::thread foreign([&] { foreignResult = run(FR::ZeroPolicy::RemoveAtZero); });
    foreign.join();
    Check(foreignResult == ClockResult::WrongThread && !finalCalls && !removeCalls && FinalMapBytes(f) == before,
          "final release preserves actual game-thread ownership before every write");
    reset();
    removeMode = 1;
    Check(run(FR::ZeroPolicy::RemoveAtZero) == ClockResult::OriginalException && f.Count(1) == 0 && finalCalls == 1 && removeCalls == 1,
          "prepare persistent zero with failed removal acknowledgement");
    Check(run(FR::ZeroPolicy::RemoveAtZero) == ClockResult::InvalidArgument && finalCalls == 1 && removeCalls == 1,
          "repeating a failed final transaction cannot replay its accepted decrement or removal entry");
    reset();
    FR::Receipt unused;
    Check(RC::RemoveGuarded(image, 17, &PoolTestHost::Owner, nullptr, nullptr, &unused.removal) == ClockResult::InvalidArgument && !removeCalls,
          "guarded internal continuation cannot silently omit its explicit final-entry guard");
    before = FinalMapBytes(f);
    Check(RC::RemoveGuarded(
              image, 17, &PoolTestHost::Owner, +[](void*) noexcept { return ClockResult::Expired; }, nullptr, &unused.removal) == ClockResult::Expired &&
              !unused.removal.entered && !removeCalls && FinalMapBytes(f) == before,
          "explicit guard refusal after removal admission reaches no native entry and changes no reference ownership");
    std::printf("Production native final model reference release fixture completed\n");
    std::printf("Passed %u native final model-reference checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
