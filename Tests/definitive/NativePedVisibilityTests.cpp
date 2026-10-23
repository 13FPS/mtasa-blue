/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual visibility bit reads and one-shot masked writes on owned peds
 ****************************************************************************/
// Reuse the real occupied-ped fixture and its production pool/metadata code.
// Its historical main is not executed. Only new visibility cases run here.
#pragma push_macro("main")
#undef main
#define main NativePedVisibilityHistoricalDuckingMain
#include "NativePedDuckingTests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/NativePedVisibilityMemory.h"
#include "../../Client/game_de/NativePedVisibilityProfile.h"
#include <functional>
#include <intrin.h>
namespace VisibilityHost
{
    std::uintptr_t        flagsAddress = 0;
    unsigned              flagsReads = 0, failFlagRead = 0, ownerCalls = 0, mutateAt = 0;
    std::function<void()> mutateOwner;
    ClockResult           Owner() noexcept
    {
        if (++ownerCalls == mutateAt && mutateOwner)
            mutateOwner();
        return PoolTestHost::Owner();
    }
    BOOL WINAPI Read(HANDLE process, LPCVOID input, LPVOID output, SIZE_T bytes, SIZE_T* copied)
    {
        if (reinterpret_cast<std::uintptr_t>(input) == flagsAddress && bytes == sizeof(std::uint32_t) && ++flagsReads == failFlagRead)
        {
            if (copied)
                *copied = 0;
            return FALSE;
        }
        return PoolTestHost::Read(process, input, output, bytes, copied);
    }
    unsigned              attempts = 0, accepted = 0;
    bool                  failBefore = false, failAfter = false;
    std::function<void()> before, after;
    bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        ++attempts;
        if (failBefore)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        if (before)
            before();
        LONG old = 0, desired = 0;
        std::memcpy(&old, &expected, sizeof(old));
        std::memcpy(&desired, &target, sizeof(desired));
        const auto result = _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), desired, old);
        std::memcpy(&observed, &result, sizeof(observed));
        if (observed == expected)
            ++accepted;
        if (after)
            after();
        if (failAfter)
        {
            exception = EXCEPTION_ACCESS_VIOLATION;
            return false;
        }
        return true;
    }
    void Reset()
    {
        attempts = accepted = flagsReads = failFlagRead = ownerCalls = mutateAt = 0;
        mutateOwner = {};
        failBefore = failAfter = false;
        before = after = {};
    }
}

#define GetModuleHandleW               PoolTestHost::Module
#define ReadProcessMemory              VisibilityHost::Read
#define MTA_DE_PED_VISIBILITY_EXCHANGE VisibilityHost::Exchange
#include "../../Client/game_de/NativePedVisibilityMemory.cpp"
#undef MTA_DE_PED_VISIBILITY_EXCHANGE
#undef ReadProcessMemory
#undef GetModuleHandleW
namespace
{
    namespace VP = mta::game_de::NativePedVisibilityProfile;
    struct VisibilityFixture : DuckingFixture
    {
        explicit VisibilityFixture(const std::filesystem::path& path) : DuckingFixture(path)
        {
            for (const auto& pin : VP::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            const auto group = transform.image.image + VP::GroupRva;
            const auto branch = static_cast<std::uint32_t>(VP::CaseRva);
            const auto selector = static_cast<unsigned char>(VP::Selector);
            transform.image.Patch(VP::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
            transform.image.Patch(VP::RemapRva + VP::CaseIndex, &selector, sizeof(selector), PAGE_EXECUTE_READ);
            transform.image.Patch(VP::TableRva + VP::Selector * 4, &branch, sizeof(branch), PAGE_EXECUTE_READ);
            Reset();
        }
        std::uint32_t Flags(std::size_t slot = 0) const
        {
            std::uint32_t value = 0;
            std::memcpy(&value, transform.pool.objects + slot * NativePedPoolProfile::ObjectStride + VP::FlagsOffset, sizeof(value));
            return value;
        }
        void Reset()
        {
            DuckingFixture::Reset();
            for (const auto slot : {std::size_t{0}, std::size_t{8}})
                Store(slot, VP::FlagsOffset, std::uint32_t{0});
            Global(0x50315a1, static_cast<unsigned char>(0x5a));
            VisibilityHost::Reset();
            VisibilityHost::flagsAddress = reinterpret_cast<std::uintptr_t>(transform.pool.objects) + VP::FlagsOffset;
        }
    };
}
int main(int argc, char** argv)
{
    using Memory = mta::game_de::NativePedVisibilityMemory;
    namespace Host = VisibilityHost;
    Check(argc == 2 && std::filesystem::path(argv[1]).is_absolute(), "new visibility property uses explicit workspace evidence");
    VisibilityFixture fixture(argv[1]);
    PoolTestHost::image = fixture.transform.image.image;
    const Memory::Binding binding{fixture.transform.image.image};
    Memory                memory(binding, &VisibilityHost::Owner), unavailable(binding, nullptr);
    Memory::Receipt       receipt;
    const auto            reset = [&](std::size_t slot = 0)
    {
        fixture.Reset();
        Host::Reset();
        return fixture.Ped(slot == 0 ? 1 : (8u << 8) | 127u);
    };
    Check(Memory::Revalidate(binding), "exact named visibility dispatch, complete bool branch, return and common constructor qualify");
    struct DefaultAccess final : mta::game_de::NativePedVisibilityAccess
    {
    } access;
    for (bool prior : {false, true})
    {
        bool value = prior;
        Check(
            access.ReadPedVisible({}, value) == ClockResult::NotInitialized && value == prior && access.SetPedVisible({}, prior) == ClockResult::NotInitialized,
            "default visibility access refuses both methods without writing output");
    }
    for (unsigned guard = 0; guard != 10; ++guard)
    {
        auto selected = reset();
        switch (guard)
        {
            case 0:
                PoolTestHost::ownerResult = ClockResult::Expired;
                break;
            case 1:
                fixture.Global(NativeStartGameProfile::ThreadInitializedRva, static_cast<unsigned char>(0));
                break;
            case 2:
                fixture.Global(NativeStartGameProfile::ThreadIdRva, GetCurrentThreadId() + 1);
                break;
            case 3:
                fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1});
                break;
            case 4:
                fixture.Global(NativeGameStateProfile::stateRva, std::uint32_t{7});
                break;
            case 5:
                fixture.Global(NativePedPoolProfile::RootRva, std::uintptr_t{0});
                break;
            case 6:
                fixture.transform.pool.flags[0] = 0x81;
                break;
            case 7:
                fixture.transform.pool.flags[0] = 2;
                break;
            case 8:
                fixture.Store(0, 0x6a, static_cast<unsigned char>(0xfa));
                break;
            case 9:
                fixture.Store(0, 0, fixture.transform.image.image + std::uintptr_t{0x1300010});
                selected = fixture.Ped();
                break;
        }
        for (bool prior : {false, true})
        {
            bool value = prior;
            Check(memory.ReadVisible(selected, value) != ClockResult::Ok && value == prior,
                  "visibility read preserves output on native identity/owner/thread/state/GC refusal");
            Check(memory.SetVisible(selected, true, &receipt) != ClockResult::Ok && !receipt.entered && !Host::attempts,
                  "the same fresh admission refuses visibility writes before entry");
        }
    }
    for (unsigned failure = 1; failure <= 2; ++failure)
    {
        auto selected = reset();
        Host::failFlagRead = failure;
        bool value = true;
        Check(memory.ReadVisible(selected, value) == ClockResult::ReadFailure && value && !Host::attempts,
              "either uncopied visibility word preserves read output");
    }
    for (unsigned mode = 0; mode != 3; ++mode)
    {
        auto selected = reset();
        fixture.Store(0, VP::FlagsOffset, VP::VisibleMask);
        PoolTestHost::watched = selected.nativePed + VP::FlagsOffset;
        static VisibilityFixture* changingVisibilityFixture = nullptr;
        static unsigned           change = 0;
        changingVisibilityFixture = &fixture;
        change = mode;
        PoolTestHost::after = +[]
        {
            if (change == 0)
                changingVisibilityFixture->Store(0, VP::FlagsOffset, changingVisibilityFixture->Flags() ^ VP::VisibleMask);
            if (change == 1)
                changingVisibilityFixture->Store(0, VP::FlagsOffset, changingVisibilityFixture->Flags() ^ std::uint32_t{0x80000001});
            if (change == 2)
                changingVisibilityFixture->transform.pool.flags[0] = 2;
        };
        bool value = false;
        auto status = memory.ReadVisible(selected, value);
        Check(mode == 1 ? status == ClockResult::Ok && value : status != ClockResult::Ok && !value,
              "a changing visible bit or slot refuses; unrelated neighbors do not become getter authority");
    }
    for (bool writing : {false, true})
    {
        auto       selected = reset();
        const auto original = selected;
        fixture.Store(0, VP::FlagsOffset, VP::VisibleMask);
        Host::mutateAt = 1;
        Host::mutateOwner = [&]
        {
            selected.handle = 0xffffffffu;
            selected.nativePed = 0;
        };
        if (writing)
            Check(memory.SetVisible(selected, false, &receipt) == ClockResult::Ok && receipt.handle == original.handle &&
                      receipt.nativePed == original.nativePed && receipt.verified && fixture.Flags() == 0 && Host::attempts == 1,
                  "owner callback cannot retarget a captured visibility write through the caller observation");
        else
        {
            bool value = false;
            Check(memory.ReadVisible(selected, value) == ClockResult::Ok && value && !Host::attempts,
                  "read uses its immutable local observation across arbitrary owner callbacks");
        }
    }
    for (const auto slot : {std::size_t{0}, std::size_t{8}})
        for (const auto original : {std::uint32_t{0}, 0xffffffffu, 0xa1b2c3d4u, 0x80u, 0x80000000u})
            for (bool enabled : {false, true})
            {
                const auto ped = reset(slot);
                fixture.Store(slot, VP::FlagsOffset, original);
                const auto target = (original & ~VP::VisibleMask) | (enabled ? VP::VisibleMask : 0);
                Check(memory.SetVisible(ped, enabled, &receipt) == ClockResult::Ok && receipt.entered && receipt.accepted && receipt.verified &&
                          receipt.attempts == 1 && Host::attempts == 1 && Host::accepted == 1 && fixture.Flags(slot) == target,
                      "actual generic/player visibility bool preserves every neighboring DWORD bit with one CAS");
                bool observed = !enabled;
                Check(memory.ReadVisible(ped, observed) == ClockResult::Ok && observed == enabled && Host::attempts == 1,
                      "focused existing read sanity observes the accepted bit without a second write");
            }
    Check(*reinterpret_cast<const unsigned char*>(fixture.transform.image.image + 0x50315a1) == 0x5a,
          "actual SDK visibility property leaves the script-only local-player global untouched");
    auto ped = reset();
    Check(unavailable.SetVisible(ped, true, &receipt) == ClockResult::NotInitialized && !receipt.entered && !Host::attempts,
          "readable private ped memory never grants callback write authority");
    for (const auto racing : {std::uint32_t{1}, VP::VisibleMask})
    {
        ped = reset();
        Host::before = [&] { fixture.Store(0, VP::FlagsOffset, fixture.Flags() ^ racing); };
        Check(memory.SetVisible(ped, true, &receipt) == ClockResult::OwnerConflict && receipt.entered && !receipt.accepted && receipt.attempts == 1 &&
                  Host::attempts == 1 && !Host::accepted && fixture.Flags() == racing,
              "neighbor or owned-bit races refuse once, retaining the engine word without retry");
    }
    ped = reset();
    Host::after = [&] { fixture.Store(0, VP::FlagsOffset, fixture.Flags() ^ std::uint32_t{0x80000001}); };
    Check(memory.SetVisible(ped, true, &receipt) == ClockResult::Ok && receipt.accepted && receipt.verified &&
              fixture.Flags() == (VP::VisibleMask | 0x80000001) && Host::attempts == 1,
          "accepted visibility change does not own or repair subsequently changed neighboring flags");
    for (unsigned mode = 0; mode != 6; ++mode)
    {
        ped = reset();
        if (mode == 0)
            Host::failBefore = true;
        if (mode == 1)
            Host::failAfter = true;
        if (mode == 2)
            Host::after = [&] { PoolTestHost::ownerResult = ClockResult::Expired; };
        if (mode == 3)
            Host::after = [&] { fixture.transform.pool.flags[0] = 2; };
        if (mode == 4)
            Host::after = [&] { fixture.Global(NativeBorrowedGcProfile::markerRva, LONG{1}); };
        if (mode == 5)
            Host::after = [&] { fixture.Store(0, VP::FlagsOffset, fixture.Flags() ^ VP::VisibleMask); };
        const auto result = memory.SetVisible(ped, true, &receipt);
        Check(result != ClockResult::Ok && receipt.entered && !receipt.verified && receipt.attempts == 1 && Host::attempts == 1 &&
                  Host::accepted == (mode == 0 ? 0u : 1u) && fixture.Flags() == (mode == 0 || mode == 5 ? 0u : VP::VisibleMask),
              "entered failure retains accepted or unknown effects without rollback, replay or a second CAS");
    }
    ped = reset();
    VisibilityHost::failFlagRead = 1;
    Check(memory.SetVisible(ped, true, &receipt) == ClockResult::ReadFailure && !receipt.entered && !Host::attempts,
          "uncopied source flags refuse before entry");
    ped = reset();
    Host::before = [&]
    {
        Memory::Receipt nested;
        Check(memory.SetVisible(ped, false, &nested) == ClockResult::Busy && !nested.entered, "same-operation recursion refuses before nested entry");
    };
    Check(memory.SetVisible(ped, true, &receipt) == ClockResult::Ok && Host::attempts == 1, "outer accepted write survives refused recursion");
    for (const auto& pin : VP::Code)
    {
        ped = reset();
        const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
        fixture.transform.image.Patch(pin.rva, &bad, 1, PAGE_EXECUTE_READ);
        Check(memory.SetVisible(ped, true, &receipt) != ClockResult::Ok && !receipt.entered && !Host::attempts,
              "genuine named setter and common constructor remain mandatory setter identity proofs");
        fixture.transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
    }
    ped = reset();
    ClockResult foreign = ClockResult::Ok;
    std::thread thread([&] { foreign = memory.SetVisible(ped, true); });
    thread.join();
    Check(foreign != ClockResult::Ok && !Host::attempts, "foreign thread cannot acquire native visibility write authority");
    for (unsigned mode = 0; mode != 3; ++mode)
    {
        ped = reset();
        const auto                   at = mode == 0 ? VP::GroupSlotRva : mode == 1 ? VP::RemapRva + VP::CaseIndex : VP::TableRva + VP::Selector * 4;
        std::array<unsigned char, 8> saved{};
        const auto                   count = mode == 0 ? sizeof(std::uintptr_t) : mode == 1 ? std::size_t{1} : sizeof(std::uint32_t);
        std::memcpy(saved.data(), reinterpret_cast<void*>(binding.image + at), count);
        const auto bad = static_cast<unsigned char>(saved[0] ^ 1);
        fixture.transform.image.Patch(at, &bad, 1, mode == 0 ? PAGE_READONLY : PAGE_EXECUTE_READ);
        bool value = true;
        Check(memory.ReadVisible(ped, value) != ClockResult::Ok && value && memory.SetVisible(ped, true, &receipt) != ClockResult::Ok && !receipt.entered &&
                  !Host::attempts,
              "wrong group/remap/branch cannot borrow a correctly copied native flag");
        fixture.transform.image.Patch(at, saved.data(), count, mode == 0 ? PAGE_READONLY : PAGE_EXECUTE_READ);
    }
    ped = reset();
    bool prior = true;
    PoolTestHost::image = 0;
    Check(memory.ReadVisible(ped, prior) != ClockResult::Ok && prior && memory.SetVisible(ped, true, &receipt) != ClockResult::Ok && !receipt.entered &&
              !Host::attempts,
          "current module replacement revokes both native visibility methods");
    PoolTestHost::image = binding.image;
    std::puts("Production native ped visibility primitive fixture completed");
    std::printf("Passed %u native ped-visibility checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
