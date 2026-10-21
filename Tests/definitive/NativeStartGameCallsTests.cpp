/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual guarded native StartGame call boundary on owned Windows data
 *****************************************************************************/
#define wmain FrozenReadonlyStatFixtureMain
#include "NativeStatsReaderTests.cpp"
#undef wmain
#include "../../Client/game_de/NativeStartGameCalls.h"
#include "../../Client/game_de/NativeStartGameProfile.h"
#include "../../Client/game_de/NativeBorrowedGcProfile.h"
#include "../../Client/game_de/NativeGameStateProfile.h"
#include "../../Client/game_de/NativeGamePauseProfile.h"
#include "../../Client/game_de/NativeApplicationFrameProfile.h"
#define GetModuleHandleW  StatsTestHost::Module
#define ReadProcessMemory StatsTestHost::Read
#include "../../Client/game_de/NativeStartGameCalls.cpp"
#undef ReadProcessMemory
#undef GetModuleHandleW

namespace Start = NativeStartGameCalls;
namespace SP = NativeStartGameProfile;
namespace GP = NativeBorrowedGcProfile;
namespace GS = NativeGameStateProfile;
namespace
{
    struct StartHost;
    StartHost* startup = nullptr;
    template <typename T>
    void StartStore(std::uintptr_t address, T value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    struct StartHost
    {
        OwnedImage owned;
        // The native receiver contract requires pointer alignment only. Extra
        // fixture alignment would add unrelated structure padding under /WX.
        alignas(std::uintptr_t) std::array<unsigned char, 0x900> application{};
        Start::Receipt receipt{};
        unsigned       checks = 0, claims = 0, calls = 0, ownerFailAt = 0;
        ClockResult    ownerResult = ClockResult::Expired, claimResult = ClockResult::Ok, callResult = ClockResult::Ok;
        bool           attempted = false, effects = true, gcAfter = false, corruptAfter = false, replaceRootAfter = false;
        std::uint32_t  afterState = 7;
        std::uintptr_t Image() const { return owned.image; }
        void*          Receiver() { return application.data(); }
        void           Protect(std::uintptr_t rva, std::size_t bytes, DWORD flags)
        {
            DWORD old = 0;
            Check(VirtualProtect(reinterpret_cast<void*>(Image() + rva), bytes, flags, &old) != FALSE, "protect only owned startup fixture pages");
        }
        void SeedCode(std::uintptr_t rva, const unsigned char* bytes, std::size_t size)
        {
            owned.Patch(rva, bytes, size);
            Protect(rva, size, PAGE_EXECUTE_READ);
        }
        void Reset()
        {
            receipt = {};
            checks = claims = calls = ownerFailAt = 0;
            ownerResult = ClockResult::Expired;
            claimResult = callResult = ClockResult::Ok;
            attempted = gcAfter = corruptAfter = replaceRootAfter = false;
            effects = true;
            afterState = 7;
            StatsTestHost::image = Image();
            StatsTestHost::failAt.store(0);
            StartStore(Image() + NativeApplicationFrameProfile::rootRva, reinterpret_cast<std::uintptr_t>(Receiver()));
            StartStore(reinterpret_cast<std::uintptr_t>(Receiver()), Image() + SP::ReceiverVtableRva);
            StartStore(Image() + SP::ThreadInitializedRva, static_cast<unsigned char>(1));
            StartStore(Image() + SP::ThreadIdRva, GetCurrentThreadId());
            StartStore(Image() + GP::markerRva, LONG{0});
            StartStore(Image() + GS::stateRva, std::uint32_t{7});
            StartStore(Image() + SP::RequestRva, static_cast<unsigned char>(0));
            StartStore(Image() + SP::SelectionRva, std::uint32_t{21});
            StartStore(Image() + NativeGamePauseProfile::userPauseRva, static_cast<unsigned char>(1));
        }
        void Create(const std::filesystem::path& directory)
        {
            owned.Create(directory);
            for (const auto& pin : SP::Code)
                SeedCode(pin.rva, pin.bytes.data(), pin.size);
            for (const auto& pin : GP::pins)
                SeedCode(pin.rva, pin.bytes, pin.size);
            for (const auto& pin : GS::pins)
            {
                owned.Patch(pin.rva, pin.bytes, pin.size);
                Protect(pin.rva, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            }
            for (const auto& pointer : SP::Pointers)
            {
                const auto target = Image() + pointer.targetRva;
                owned.Patch(pointer.slotRva, &target, sizeof(target));
            }
            owned.Patch(SP::NameRva, SP::Name, sizeof(SP::Name));
            for (const auto rva : {NativeApplicationFrameProfile::rootRva, SP::ThreadInitializedRva, SP::ThreadIdRva, GP::markerRva, GS::stateRva,
                                   SP::RequestRva, SP::SelectionRva, NativeGamePauseProfile::userPauseRva})
                Protect(rva, sizeof(std::uintptr_t), PAGE_READWRITE);
            Reset();
        }
        static ClockResult Owner(void* receiver) noexcept
        {
            auto& host = *startup;
            ++host.checks;
            Check(receiver == host.Receiver(), "every owner bookend receives the same actual receiver argument");
            return host.ownerFailAt == host.checks ? host.ownerResult : ClockResult::Ok;
        }
        static ClockResult Claim(void* receiver) noexcept
        {
            auto& host = *startup;
            ++host.claims;
            Check(receiver == host.Receiver() && host.checks == 2 && !host.calls,
                  "final graph/scope admission and once-only claim occur after prerequisites and directly before target entry");
            if (host.claimResult != ClockResult::Ok)
                return host.claimResult;
            if (host.attempted)
                return ClockResult::Busy;
            host.attempted = true;
            return ClockResult::Ok;
        }
        static ClockResult Invoke(Start::NativeFunction target, void* receiver, std::uint32_t& exception) noexcept
        {
            auto& host = *startup;
            ++host.calls;
            Check(reinterpret_cast<std::uintptr_t>(target) == host.Image() + SP::EntryRva && receiver == host.Receiver(),
                  "typed call resolves the named concrete SA entry and passes its real this pointer");
            Check(host.attempted && host.claims == 1 && host.checks == 2, "one attempt is claimed before the exact native target boundary");
            // Never call target. The fixture injects independent owned values
            // to exercise publication/refusal; it does not emulate native UI,
            // allocation, world loading or the UObject graph.
            if (host.effects)
            {
                StartStore(host.Image() + SP::RequestRva, static_cast<unsigned char>(1));
                StartStore(host.Image() + SP::SelectionRva, std::uint32_t{0});
                StartStore(host.Image() + NativeGamePauseProfile::userPauseRva, static_cast<unsigned char>(0));
                StartStore(host.Image() + GS::stateRva, host.afterState);
            }
            if (host.gcAfter)
                StartStore(host.Image() + GP::markerRva, LONG{9});
            if (host.corruptAfter)
            {
                const unsigned char bad = static_cast<unsigned char>(SP::Code.front().bytes[0] ^ 1);
                host.owned.Patch(SP::Code.front().rva, &bad, 1);
            }
            if (host.replaceRootAfter)
                StartStore(host.Image() + NativeApplicationFrameProfile::rootRva, std::uintptr_t{0});
            if (host.callResult == ClockResult::OriginalException)
                exception = EXCEPTION_ACCESS_VIOLATION;
            return host.callResult;
        }
        ClockResult Apply() { return Start::Apply(Image(), Receiver(), &Owner, &Claim, &receipt, &Invoke); }
        void        Refuses(ClockResult expected, const char* reason)
        {
            Check(Apply() == expected, reason);
            Check(!claims && !calls && !attempted && !receipt.entered && !receipt.returned,
                  "pre-entry refusal cannot consume an attempt or invent native completion");
        }
    };
    void NativeThread(StartHost& host)
    {
        Check(Start::CheckThread(host.Image()) == ClockResult::Ok, "actual initialized native GameThread and GC-zero protocol admit owned scope");
        for (const LONG marker : {LONG{1}, LONG{9}, LONG{-1}})
        {
            host.Reset();
            StartStore(host.Image() + GP::markerRva, marker);
            Check(Start::CheckThread(host.Image()) == ClockResult::Busy && *reinterpret_cast<const LONG*>(host.Image() + GP::markerRva) == marker,
                  "atomic zero-probe refuses and preserves every foreign nonzero GC marker");
            host.Refuses(ClockResult::Busy, "in-GC native scope never reaches a startup claim");
        }
        host.Reset();
        StartStore(host.Image() + SP::ThreadInitializedRva, static_cast<unsigned char>(0));
        host.Refuses(ClockResult::NotInitialized, "uninitialized native GameThread cannot authorize UObject calls");
        host.Reset();
        StartStore(host.Image() + SP::ThreadInitializedRva, static_cast<unsigned char>(2));
        host.Refuses(ClockResult::BindingChanged, "foreign malformed native initialization flag refuses");
        host.Reset();
        StartStore(host.Image() + SP::ThreadIdRva, GetCurrentThreadId() ^ DWORD{1});
        host.Refuses(ClockResult::WrongThread, "producer or renderer thread cannot substitute for initialized native GameThread");
        host.Reset();
        host.Protect(GP::markerRva, sizeof(LONG), PAGE_READONLY);
        host.Refuses(ClockResult::ReadFailure, "GC marker must support the real native atomic zero-probe");
        host.Protect(GP::markerRva, sizeof(LONG), PAGE_READWRITE);
        host.Reset();
        ClockResult result = ClockResult::Ok;
        std::thread other([&] { result = host.Apply(); });
        other.join();
        Check(result == ClockResult::WrongThread && !host.calls && !host.claims,
              "actual thread ID is independently checked even if caller offers an owner callback");
    }
    void Provenance(StartHost& host)
    {
        for (const auto& pin : SP::Code)
        {
            host.Reset();
            const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            host.owned.Patch(pin.rva, &bad, 1);
            host.Refuses(ClockResult::BindingChanged, "every actual entry/callee/ABI/thread pin is required before native startup");
            host.owned.Patch(pin.rva, pin.bytes.data(), 1);
        }
        for (const auto& pin : GP::pins)
        {
            host.Reset();
            const auto bad = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            host.owned.Patch(pin.rva, &bad, 1);
            host.Refuses(ClockResult::BindingChanged, "each exact native borrowed-GT/GC protocol pin must remain supported");
            host.owned.Patch(pin.rva, pin.bytes, 1);
        }
        for (const auto& pointer : SP::Pointers)
        {
            host.Reset();
            const std::uintptr_t bad = 0;
            host.owned.Patch(pointer.slotRva, &bad, sizeof(bad));
            host.Refuses(ClockResult::BindingChanged, "reflection descriptor and concrete relocated virtual targets must match the named native action");
            const auto original = host.Image() + pointer.targetRva;
            host.owned.Patch(pointer.slotRva, &original, sizeof(original));
        }
        host.Reset();
        const char bad = 'X';
        host.owned.Patch(SP::NameRva, &bad, 1);
        host.Refuses(ClockResult::BindingChanged, "native action name belongs to the same independently identified route");
        host.owned.Patch(SP::NameRva, SP::Name, 1);
        for (const auto& pin : GS::pins)
        {
            host.Reset();
            const auto badCode = static_cast<unsigned char>(pin.bytes[0] ^ 1);
            host.owned.Patch(pin.rva, &badCode, 1);
            host.Refuses(ClockResult::BindingChanged, "native state7 meaning requires the existing actual dispatch profile");
            host.owned.Patch(pin.rva, pin.bytes, 1);
        }
    }
    void EntryAndCompletion(StartHost& host)
    {
        for (const std::uint32_t after : {7U, 8U, 9U})
        {
            host.Reset();
            host.afterState = after;
            Check(host.Apply() == ClockResult::Ok && host.calls == 1 && host.claims == 1 && host.checks == 3 && host.attempted,
                  "real primitive enters exactly once and accepts native asynchronous progress without forcing state8");
            Check(host.receipt.entered && host.receipt.returned && host.receipt.nativeStateBefore == 7 && host.receipt.nativeStateAfter == after &&
                      host.receipt.requestBefore == 0 && host.receipt.requestAfter == 1 && host.receipt.selectionBefore == 21 &&
                      host.receipt.selectionAfter == 0 && host.receipt.userPauseAfter == 0,
                  "receipt publishes actual qualified request/menu-close effects rather than synchronous game-loaded success");
        }
        for (const std::uint32_t state : {6U, 8U, 9U})
        {
            host.Reset();
            StartStore(host.Image() + GS::stateRva, state);
            host.Refuses(state == 6 ? ClockResult::NotInitialized : ClockResult::Busy, "only the actual frontend7 can begin the new-game menu transaction");
        }
        host.Reset();
        StartStore(host.Image() + SP::RequestRva, static_cast<unsigned char>(1));
        host.Refuses(ClockResult::Busy, "an already pending native request is preserved without claiming another entry");
        host.Reset();
        StartStore(host.Image() + NativeApplicationFrameProfile::rootRva, std::uintptr_t{0});
        host.Refuses(ClockResult::BindingChanged, "same-frame receiver must still be the published native SA root");
        host.Reset();
        StartStore(reinterpret_cast<std::uintptr_t>(host.Receiver()), std::uintptr_t{0});
        host.Refuses(ClockResult::BindingChanged, "a UObject or guessed receiver cannot impersonate the plain native SA application class");
        for (const unsigned stop : {1U, 2U, 3U})
        {
            host.Reset();
            host.ownerFailAt = stop;
            Check(host.Apply() == ClockResult::Expired && host.calls == (stop == 3 ? 1U : 0U) && host.receipt.entered == (stop == 3),
                  "real owner expiry distinguishes a refusal from an already entered native transaction");
            Check(host.attempted == (stop == 3), "an early scope refusal does not consume the generation attempt");
        }
        host.Reset();
        host.claimResult = ClockResult::NotInitialized;
        Check(host.Apply() == ClockResult::NotInitialized && host.claims == 1 && !host.calls && !host.attempted && !host.receipt.entered,
              "missing final real world/provider/UI prerequisites refuse directly before the once-only entry claim");
        host.Reset();
        host.attempted = true;
        Check(host.Apply() == ClockResult::Busy && host.claims == 1 && !host.calls && !host.receipt.entered,
              "a completed or partially entered attempt cannot re-enter within the same owner generation");
        host.Reset();
        host.callResult = ClockResult::OriginalException;
        Check(host.Apply() == ClockResult::OriginalException && host.calls == 1 && host.attempted && host.receipt.entered && !host.receipt.returned &&
                  host.receipt.exceptionCode == EXCEPTION_ACCESS_VIOLATION,
              "partial native exception records one honest entry without fabricated return");
        Check(*reinterpret_cast<const unsigned char*>(host.Image() + SP::RequestRva) == 1,
              "partial native request effect is never retried or rolled back to a previous world");
        host.Reset();
        host.effects = false;
        Check(host.Apply() == ClockResult::BindingChanged && host.calls == 1 && host.attempted && host.receipt.returned &&
                  *reinterpret_cast<const unsigned char*>(host.Image() + SP::RequestRva) == 0,
              "the wrapper never fabricates missing native request effects using direct state or flag writes");
        host.Reset();
        host.gcAfter = true;
        Check(host.Apply() == ClockResult::Busy && host.calls == 1 && host.receipt.returned && host.attempted,
              "loss of native borrowed-GT GC scope after entry is reported without a second attempt");
        Check(*reinterpret_cast<const LONG*>(host.Image() + GP::markerRva) == 9, "foreign GC state remains intact after refusal");
        host.Reset();
        host.corruptAfter = true;
        Check(host.Apply() == ClockResult::BindingChanged && host.calls == 1 && host.receipt.returned,
              "native route changing after invocation cannot be reported as a stable completed request");
        host.owned.Patch(SP::Code.front().rva, SP::Code.front().bytes.data(), 1);
        host.Reset();
        host.replaceRootAfter = true;
        Check(host.Apply() == ClockResult::BindingChanged && host.calls == 1 && host.receipt.returned,
              "replaced native application after entry cannot authorize continuation under an old receiver");
        host.Reset();
        Check(Start::Apply(host.Image(), host.Receiver(), nullptr, &StartHost::Claim, &host.receipt, &StartHost::Invoke) == ClockResult::InvalidArgument &&
                  !host.calls,
              "missing same-frame owner guard cannot be substituted by current thread alone");
        Check(Start::Apply(host.Image(), host.Receiver(), &StartHost::Owner, nullptr, &host.receipt, &StartHost::Invoke) == ClockResult::InvalidArgument &&
                  !host.calls,
              "missing final prerequisites/attempt claim cannot reach the native entry");
        StatsTestHost::image = 0;
        Check(host.Apply() == ClockResult::BindingChanged && !host.calls, "another main image cannot reuse a prior startup binding");
        host.Reset();
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2 || sizeof(void*) != 8)
        return 2;
    StartHost host;
    host.Create(argv[1]);
    startup = &host;
    NativeThread(host);
    Provenance(host);
    EntryAndCompletion(host);
    StatsTestHost::image = 0;
    startup = nullptr;
    std::printf("Passed %u native StartGame routing checks (64-bit; no native engine code executed)\n", checks);
    return 0;
}
