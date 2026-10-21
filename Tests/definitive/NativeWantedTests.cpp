/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CWanted/native transactions over owned Windows memory only
 *****************************************************************************/
// Reuse the real checked player binding and sparse SEC_IMAGE fixture. Its test
// main is not run, and no native instructions in the owned image are executed.
#define wmain FrozenPlayerInfoFixtureMain
#include "NativePlayerInfoTests.cpp"
#undef wmain
#include "../../Client/game_de/NativeWantedMemory.h"
#include "../../Client/game_de/NativeWantedProfile.h"
#include "../../Client/game_de/CWantedDE.h"

namespace WantedHost
{
    using namespace mta::game_de;
    using Memory = NativeWantedMemory;
    unsigned                                  calls = 0, exchanges = 0;
    std::uint32_t                             requested = 0;
    std::uintptr_t                            functionAddress = 0;
    void*                                     receiver = nullptr;
    ClockResult                               outcome = ClockResult::Ok;
    std::function<void(void*, std::uint32_t)> duringCall;
    std::function<void()>                     beforeCas, afterCas;
    bool                                      casFault = false;
    LONG                                      Cas32(volatile LONG* address, LONG value, LONG expected)
    {
        ++exchanges;
        if (casFault)
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        if (beforeCas)
            beforeCas();
        const auto observed = _InterlockedCompareExchange(address, value, expected);
        if (afterCas)
            afterCas();
        return observed;
    }
    ClockResult Invoke(Memory::NativeFunction function, void* object, std::uint32_t level, std::uint32_t& exception) noexcept
    {
        ++calls;
        functionAddress = reinterpret_cast<std::uintptr_t>(function);
        receiver = object;
        requested = level;
        if (duringCall)
            duringCall(object, level);
        if (outcome == ClockResult::OriginalException)
            exception = EXCEPTION_ACCESS_VIOLATION;
        return outcome;
    }
    void Reset()
    {
        PlayerHost::Reset();
        calls = exchanges = requested = 0;
        functionAddress = 0;
        receiver = nullptr;
        outcome = ClockResult::Ok;
        duringCall = {};
        beforeCas = afterCas = {};
        casFault = false;
    }
}
#undef InterlockedCompareExchange
#define InterlockedCompareExchange WantedHost::Cas32
#define ReadProcessMemory          PlayerHost::Read
#include "../../Client/game_de/NativeWantedMemory.cpp"
#undef ReadProcessMemory
#undef InterlockedCompareExchange
#include "../../Client/game_de/CWantedDE.cpp"

namespace
{
    namespace W = NativeWantedProfile;
    using WantedMemory = NativeWantedMemory;
    template <class T>
    T& WantedField(std::uintptr_t object, std::size_t offset)
    {
        return *reinterpret_cast<T*>(object + offset);
    }
    void ProtectWanted(std::uintptr_t address, std::size_t bytes, DWORD flags)
    {
        DWORD old = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(address), bytes, flags, &old) != FALSE, "change only owned wanted page protections");
    }
    void SeedWanted(OwnedImage& host, std::uintptr_t wanted)
    {
        for (const auto& pin : W::Code)
        {
            host.Patch(pin.rva, pin.bytes.data(), pin.size);
            ProtectWanted(host.image + pin.rva, pin.size, PAGE_EXECUTE_READ);
        }
        const std::uint32_t maximumLevel = 6, timer = 1000;
        const auto          maximumChaos = W::MaximumChaos[6];
        for (auto rva : {W::MaximumLevelRva, W::MaximumChaosRva, W::TimerRva})
            ProtectWanted(host.image + rva, 4, PAGE_READWRITE);
        host.Patch(W::MaximumLevelRva, &maximumLevel, sizeof(maximumLevel));
        host.Patch(W::MaximumChaosRva, &maximumChaos, sizeof(maximumChaos));
        host.Patch(W::TimerRva, &timer, sizeof(timer));
        host.Patch(W::WantedRootRva, &wanted, sizeof(wanted));
    }
    struct WantedAccess final : NativeWantedAccess, NativePlayerInfoAccess
    {
        WantedMemory&         memory;
        CWantedDE             sdk{*this};
        WantedMemory::Receipt receipt;
        explicit WantedAccess(WantedMemory& value) : memory(value) {}
        ClockResult ReadWantedLevel(std::uint32_t& value) noexcept override { return memory.ReadLevel(value, PlayerHost::Admit); }
        ClockResult SetWantedLevel(std::uint32_t value, bool noFlash) noexcept override
        {
            return memory.SetLevel(value, noFlash, PlayerHost::Admit, &receipt, WantedHost::Invoke);
        }
        ClockResult GetWanted(CWanted*& output) noexcept override
        {
            const auto result = memory.Qualify(PlayerHost::Admit);
            if (result == ClockResult::Ok)
                output = &sdk;
            return result;
        }
    };
    struct DefaultWanted final : NativeWantedAccess
    {
    };
    void ExerciseWanted(OwnedImage& host)
    {
        LoadedPlayer ped(host);
        auto*        allocation = VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        auto*        secondAllocation = VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Check(allocation && secondAllocation, "allocate only owned native wanted-shaped objects");
        const auto wanted = reinterpret_cast<std::uintptr_t>(allocation), second = reinterpret_cast<std::uintptr_t>(secondAllocation);
        SeedWanted(host, wanted);
        PlayerHost::image = host.image;
        NativePlayerInfoMemory player;
        WantedMemory           memory(player);
        std::uint32_t          output = 0x12345678;
        Check(memory.ReadLevel(output, PlayerHost::Admit) == ClockResult::NotInitialized && output == 0x12345678,
              "unbound wanted read preserves caller output");
        Check(player.Bind() == ClockResult::Ok, "reuse genuine checked player binding");
        Check(memory.Qualify(nullptr) == ClockResult::NotInitialized, "wanted existence never grants a callback lease");
        Check(memory.Qualify(PlayerHost::Admit) == ClockResult::Ok, "qualify current inline PlayerPedData wanted pointer");
        WantedAccess access(memory);
        CWanted&     sdk = access.sdk;
        TestPlayer   playerSdk(access);
        Check(playerSdk.GetWanted() == &sdk, "actual CPlayerInfo GetWanted returns the same stable complete SDK object");
        auto&      level = WantedField<std::uint32_t>(wanted, W::LevelOffset);
        auto&      stamp = WantedField<std::uint32_t>(wanted, W::TimestampOffset);
        const auto reset = [&]
        {
            WantedHost::Reset();
            level = 1;
            stamp = 73;
            WantedField<std::uint32_t>(host.image, W::TimerRva) = 1000;
            WantedField<std::uint32_t>(host.image, W::MaximumLevelRva) = 6;
            WantedField<std::int32_t>(host.image, W::MaximumChaosRva) = 6900;
            host.Patch(W::WantedRootRva, &wanted, sizeof(wanted));
            WantedHost::duringCall = [&](void* object, std::uint32_t requested)
            {
                auto address = reinterpret_cast<std::uintptr_t>(object);
                if (requested <= 6 && WantedField<std::uint32_t>(address, W::LevelOffset) != requested)
                {
                    WantedField<std::uint32_t>(address, W::LevelOffset) = requested;
                    WantedField<std::uint32_t>(address, W::TimestampOffset) = WantedField<std::uint32_t>(host.image, W::TimerRva);
                }
                WantedField<std::uint32_t>(address, 0x38) = 0;
            };
        };
        for (std::uint32_t value : {0u, 1u, 5u, 6u, 7u, UINT32_MAX})
        {
            reset();
            level = value;
            Check(memory.ReadLevel(output, PlayerHost::Admit) == ClockResult::Ok && output == value, "native read retains every raw unsigned level");
            Check(sdk.GetWantedLevel() == static_cast<char>(value > 6 ? 6 : value), "actual SDK uses the classic clamp instead of a shadow level");
        }
        for (std::uint32_t value : {0u, 1u, 6u, 7u, UINT32_MAX})
        {
            reset();
            WantedField<std::uint32_t>(wanted, 0x38) = 91;
            sdk.SetWantedLevel(value);
            Check(WantedHost::calls == 1 && WantedHost::requested == value && WantedHost::receiver == allocation &&
                      WantedHost::functionAddress == host.image + W::SetRva && access.receipt.entered && access.receipt.returned,
                  "SDK passes exact raw DWORD to one real typed receiver/entry boundary");
            Check(WantedField<std::uint32_t>(wanted, 0x38) == 0 && WantedHost::exchanges == 0,
                  "normal setter preserves entered reactions without a synthetic level write or timestamp rollback");
        }
        reset();
        sdk.SetWantedLevelNoFlash(5);
        Check(level == 5 && stamp == 73 && WantedHost::calls == 1 && WantedHost::exchanges == 1 && access.receipt.timestampRestored,
              "classic NoFlash restores only the previous timestamp after real setter effects");
        reset();
        WantedHost::duringCall = {};
        sdk.SetWantedLevelNoFlash(6);
        Check(level == 1 && stamp == 73 && WantedHost::exchanges == 0 && access.receipt.timestampRestored,
              "native NeverWanted-style no-op remains a successful exact transaction");
        reset();
        WantedHost::duringCall = [&](void*, std::uint32_t)
        {
            level = 4;
            stamp = 12345;
        };
        Check(memory.SetLevel(4, true, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::OwnerConflict && stamp == 12345 &&
                  WantedHost::calls == 1 && WantedHost::exchanges == 0 && access.receipt.entered,
              "unattributable later stamp survives failed NoFlash without native replay");
        reset();
        WantedHost::beforeCas = [&] { stamp = 9876; };
        Check(memory.SetLevel(4, true, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::OwnerConflict && stamp == 9876,
              "atomic expected-value conflict preserves a foreign timestamp");
        reset();
        WantedHost::afterCas = [&] { stamp = 9999; };
        Check(memory.SetLevel(4, true, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::OwnerConflict && stamp == 9999,
              "later stamp remains untouched after honest failed restoration acknowledgement");
        reset();
        WantedHost::casFault = true;
        Check(memory.SetLevel(4, true, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::WriteFailure && stamp == 1000 && level == 4 &&
                  access.receipt.entered && WantedHost::calls == 1,
              "timestamp write fault preserves entered level/reactions and never retries native setter");
        reset();
        WantedHost::outcome = ClockResult::OriginalException;
        Check(memory.SetLevel(5, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::OriginalException && level == 5 &&
                  access.receipt.entered && !access.receipt.returned && access.receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION,
              "entered native exception is an explicit partially applied transaction");
        reset();
        WantedHost::duringCall = [&](void*, std::uint32_t)
        {
            level = 4;
            PlayerHost::expiresAt = PlayerHost::ownerChecks + 1;
        };
        Check(memory.SetLevel(4, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::Expired && access.receipt.entered &&
                  access.receipt.returned && level == 4 && WantedHost::calls == 1,
              "revocation after native return preserves effects and reports the real owner refusal");
        reset();
        WantedHost::duringCall = [&](void*, std::uint32_t) { host.Patch(W::WantedRootRva, &second, sizeof(second)); };
        Check(memory.SetLevel(4, true, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::BindingChanged && access.receipt.entered &&
                  WantedHost::exchanges == 0,
              "post-entry replacement wanted pointer is never written through the old snapshot");
        reset();
        sdk.SetWantedLevelNoFlash(4);
        const unsigned ownerBoundaries = PlayerHost::ownerChecks;
        Check(ownerBoundaries > 5 && ownerBoundaries < 100, "bounded real callback owner admission sequence");
        for (unsigned fail = 1; fail <= ownerBoundaries; ++fail)
        {
            reset();
            PlayerHost::expiresAt = fail;
            const auto result = memory.SetLevel(4, true, PlayerHost::Admit, &access.receipt, WantedHost::Invoke);
            Check(result == ClockResult::Expired && WantedHost::calls <= 1, "each revoked owner boundary refuses without retrying native reactions");
            if (!access.receipt.entered)
                Check(level == 1 && stamp == 73 && WantedHost::exchanges == 0, "pre-entry refusal applies no intent");
            if (WantedHost::exchanges)
                Check(stamp == 73 && level == 4, "post-CAS refusal is honest without undoing persistent wanted intent");
        }
        reset();
        level = 7;
        Check(memory.SetLevel(2, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::BindingChanged && !access.receipt.entered,
              "foreign current level cannot enter native reaction arithmetic");
        reset();
        WantedField<std::uint32_t>(host.image, W::MaximumLevelRva) = 7;
        Check(memory.SetLevel(2, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::BindingChanged && !access.receipt.entered,
              "native bounded maximum-level producer domain is checked");
        reset();
        WantedField<std::int32_t>(host.image, W::MaximumChaosRva) = -1;
        Check(memory.SetLevel(2, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::BindingChanged && !access.receipt.entered,
              "foreign maximum-level pair cannot silently drive native dispatch");
        reset();
        const std::uintptr_t null = 0;
        host.Patch(W::WantedRootRva, &null, sizeof(null));
        output = 123;
        Check(memory.ReadLevel(output, PlayerHost::Admit) == ClockResult::NotInitialized && output == 123, "missing wanted pointer never becomes level zero");
        Refuses([&] { playerSdk.GetWanted(); }, "actual player SDK refuses absent wanted service");
        reset();
        ProtectWanted(wanted, 0x1000, PAGE_READONLY);
        Check(memory.ReadLevel(output, PlayerHost::Admit) == ClockResult::Ok, "genuine getter accepts readable native wanted fields");
        Check(memory.SetLevel(2, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::BindingChanged && !access.receipt.entered,
              "native transaction requires the entire writable real wanted allocation");
        ProtectWanted(wanted, 0x1000, PAGE_READWRITE);
        reset();
        PlayerHost::readFault = true;
        output = 123;
        Check(memory.ReadLevel(output, PlayerHost::Admit) != ClockResult::Ok && output == 123, "partial OS read preserves caller output");
        for (const auto& pin : W::Code)
        {
            reset();
            const auto corrupt = static_cast<unsigned char>(pin.bytes[0] ^ 0x40);
            host.Patch(pin.rva, &corrupt, 1);
            output = 123;
            Check(memory.ReadLevel(output, PlayerHost::Admit) == ClockResult::BindingChanged && output == 123,
                  "every exact native producer/consumer pin refuses a foreign image edit");
            Check(memory.SetLevel(2, false, PlayerHost::Admit, &access.receipt, WantedHost::Invoke) == ClockResult::BindingChanged && !access.receipt.entered,
                  "foreign native entry or reaction code is never invoked");
            host.Patch(pin.rva, pin.bytes.data(), pin.size);
        }
        reset();
        ClockResult wrongThread = ClockResult::Ok;
        std::thread other(
            [&]
            {
                std::uint32_t value = 99;
                wrongThread = memory.ReadLevel(value, PlayerHost::Admit);
                Check(value == 99, "wrong-thread output preserved");
            });
        other.join();
        Check(wrongThread == ClockResult::WrongThread, "wanted wrapper cannot move its native player binding to another thread");
        DefaultWanted absent;
        CWantedDE     missing(absent);
        Refuses([&] { missing.GetWantedLevel(); }, "complete SDK getter still refuses missing owner");
        Refuses([&] { missing.SetWantedLevel(0); }, "complete SDK setter never uses a CPU shadow");
        Refuses([&] { missing.SetWantedLevelNoFlash(0); }, "complete NoFlash never becomes an unsupported no-op");
        reset();
        sdk.SetWantedLevelNoFlash(5);
        player.Reset();
        Check(level == 5 && stamp == 73, "retirement preserves successful persistent native wanted intent");
        output = 123;
        Check(memory.ReadLevel(output, PlayerHost::Admit) == ClockResult::NotInitialized && output == 123, "old SDK pointer grants no retired native binding");
        Check(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE && VirtualFree(secondAllocation, 0, MEM_RELEASE) != FALSE,
              "release only fixture-owned wanted allocations after binding retirement");
    }
}
int wmain(int argc, wchar_t** argv)
{
    Check(argc == 2, "explicit workspace-owned wanted fixture directory");
    OwnedImage host;
    host.Create(argv[1]);
    ExerciseWanted(host);
    std::printf("Passed %u native wanted and actual SDK checks (64-bit)\n", checks);
    return 0;
}
