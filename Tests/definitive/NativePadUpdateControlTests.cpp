/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the production updater owner on an owned synthetic entry
 *****************************************************************************/
#include "../../Client/game_de/NativePadUpdateControl.h"
#include "../../Client/game_de/NativePadUpdateProfile.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include "../../Client/game_de/NativeServiceAdmission.h"
#include "../../Client/game_de/NativePadStateAccess.h"
#include "../../Client/game_de/CPadControlDE.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace
{
    using Bytes = std::array<unsigned char, 32>;
    unsigned              checks = 0;
    std::uintptr_t        fixturePage = 0;
    std::atomic<unsigned> originalCalls{0}, stateAccesses{0};
    std::atomic<bool>     blockOriginal{false};
    std::atomic<DWORD>    originalWait{WAIT_FAILED};
    HANDLE                entered = nullptr, releaseOriginal = nullptr;

    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, description);
            std::exit(EXIT_FAILURE);
        }
    }

    __declspec(noinline) void OriginalTestBody()
    {
        originalCalls.fetch_add(1);
        if (blockOriginal.load())
        {
            SetEvent(entered);
            originalWait.store(WaitForSingleObject(releaseOriginal, 5000));
        }
    }

    DWORD WINAPI CallUpdater(void*)
    {
        reinterpret_cast<void (*)()>(fixturePage)();
        return 0;
    }
}

namespace mta::game_de::SyntheticImageAccess
{
    // Only the fixture's MEM_PRIVATE allocation substitutes for image access.
    // Real OS protection/query/copy calls still validate every entry read.
    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* destination, std::size_t size, bool writable, bool executable) noexcept
    {
        if (!image || image != fixturePage || !destination || address < image || address - image >= 4096 || !size || size > 4096 - (address - image))
            return false;
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.Type != MEM_PRIVATE || info.State != MEM_COMMIT ||
            reinterpret_cast<std::uintptr_t>(info.AllocationBase) != image || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const DWORD protection = info.Protect & 0xff;
        const bool  canRead =
            protection == PAGE_READWRITE || protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE;
        const bool canExecute = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE;
        if (!canRead || (writable && protection != PAGE_READWRITE) || (executable && !canExecute))
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), destination, size, &copied) && copied == size;
    }
}

namespace mta::game_de::SyntheticPadUpdateProfile
{
    using Binding = NativePadUpdateProfile::Binding;
    inline constexpr std::uintptr_t UpdaterRva = 0, EndRva = 64;
    Bytes                           EntryBytes{};

    bool Revalidate(const Binding& binding, bool requireUnhookedEntry = true) noexcept
    {
        if (!fixturePage || binding.image != fixturePage || binding.updater != fixturePage || binding.pads != fixturePage + 128)
            return false;
        Bytes actual{};
        return SyntheticImageAccess::ReadImage(binding.image, binding.updater, actual.data(), actual.size(), false, true) &&
               (!requireUnhookedEntry || actual == EntryBytes);
    }

    bool Qualify(Binding& output) noexcept
    {
        const Binding proposed{fixturePage, fixturePage, fixturePage + 128};
        if (!SyntheticPadUpdateProfile::Revalidate(proposed))
            return false;
        output = proposed;
        return true;
    }
}

// Include the actual owner exactly once. Only build identity and image-page
// selection are replaced; Detours, peers, unwind families, admission, callback,
// producer thread, generations and cleanup state machine remain production.
#define NativePadUpdateProfile SyntheticPadUpdateProfile
#define NativeImageAccess      SyntheticImageAccess
#include "../../Client/game_de/NativePadUpdateControl.cpp"
#undef NativeImageAccess
#undef NativePadUpdateProfile

namespace
{
    namespace Admission = mta::game_de::NativeServiceAdmission;
    using mta::game_de::NativePadUpdateControl;
    using mta::game_de::NativePadUpdateState;

    Bytes ReadEntry()
    {
        Bytes bytes{};
        Check(mta::game_de::SyntheticImageAccess::ReadImage(fixturePage, fixturePage, bytes.data(), bytes.size(), false, true),
              "read the actual fixture-owned executable entry");
        return bytes;
    }

    bool NoStateAccess(std::uintptr_t, std::size_t, bool, bool) noexcept
    {
        stateAccesses.fetch_add(1);
        return false;
    }

    class TestPad final : public mta::game_de::CPadControlDE
    {
    public:
        TestPad(mta::game_de::NativePadStateAccess& stateAccess, mta::game_de::NativePadUpdateAccess& updateAccess) noexcept
            : CPadControlDE(stateAccess, updateAccess)
        {
        }
        void          Restore() override { Unqualified(); }
        void          Store() override { Unqualified(); }
        void          SetHornHistoryValue(bool) override { Unqualified(); }
        void          SetLastTimeTouched(DWORD) override { Unqualified(); }
        short         GetShakeDur() override { Unqualified(); }
        unsigned char GetShakeFreq() override { Unqualified(); }
        void          SetShakeDur(short) override { Unqualified(); }

    private:
        [[noreturn]] static void Unqualified() { throw std::logic_error("unqualified SDK fixture method"); }
    };

    struct WrongThread
    {
        NativePadUpdateControl* owner = nullptr;
        bool                    read = true, disabled = true, output = true;
    };

    DWORD WINAPI WrongProducer(void* opaque)
    {
        auto& request = *static_cast<WrongThread*>(opaque);
        request.read = request.owner->IsEnabled(request.output);
        request.disabled = request.owner->SetDisabled(true);
        return 0;
    }

    void Join(HANDLE worker)
    {
        Check(worker != nullptr, "the fixture creates its own worker thread");
        Check(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0, "fixture-owned worker exits within its bound");
        Check(CloseHandle(worker) != FALSE, "close the completed fixture worker");
    }

    HANDLE BeginBlockedCall()
    {
        Check(ResetEvent(entered) && ResetEvent(releaseOriginal), "reset only the fixture's worker coordination events");
        originalWait.store(WAIT_FAILED);
        blockOriginal.store(true);
        const auto worker = CreateThread(nullptr, 0, CallUpdater, nullptr, 0, nullptr);
        Check(worker && WaitForSingleObject(entered, 5000) == WAIT_OBJECT_0, "actual hooked updater enters its blocked original callback");
        return worker;
    }

    void ReleaseBlockedCall(HANDLE worker)
    {
        Check(SetEvent(releaseOriginal) != FALSE, "release the fixture-owned original callback");
        Join(worker);
        blockOriginal.store(false);
        Check(originalWait.load() == WAIT_OBJECT_0, "the original callback finished because the test released it");
    }

    void ChangeByte(std::size_t index, unsigned char value)
    {
        DWORD previous = 0;
        Check(index < 32 && VirtualProtect(reinterpret_cast<void*>(fixturePage), 4096, PAGE_READWRITE, &previous),
              "temporarily change protection of only the owned entry page");
        *reinterpret_cast<unsigned char*>(fixturePage + index) = value;
        DWORD ignored = 0;
        Check(VirtualProtect(reinterpret_cast<void*>(fixturePage), 4096, previous, &ignored) != FALSE &&
                  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(fixturePage), 4096),
              "restore owned entry protection and instruction visibility");
    }
}

int main()
{
    Check(sizeof(void*) == 8, "the owned updater fixture uses AMD64 code");
    auto* page = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(page != nullptr, "allocate only a fixture-owned private code page");
    fixturePage = reinterpret_cast<std::uintptr_t>(page);
    std::memset(page, 0x90, 4096);
    page[0] = 0x48;
    page[1] = 0xb8;
    const auto destination = reinterpret_cast<std::uintptr_t>(&OriginalTestBody);
    std::memcpy(page + 2, &destination, sizeof(destination));
    page[10] = 0xff;
    page[11] = 0xe0;
    std::memcpy(mta::game_de::SyntheticPadUpdateProfile::EntryBytes.data(), page, 32);
    DWORD prior = 0;
    Check(VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &prior) && FlushInstructionCache(GetCurrentProcess(), page, 4096),
          "publish owned MOV RAX / JMP RAX code read-execute before detouring");
    entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    releaseOriginal = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(entered && releaseOriginal, "create bounded callback coordination events");
    const auto             original = ReadEntry();
    NativePadUpdateControl owner;
    Admission::Token       clock;
    Check(Admission::TryAcquire(Admission::Category::Clock, clock), "claim actual shared clock admission before updater setup");
    Check(!owner.Begin() && ReadEntry() == original && !Admission::IsClaimed(Admission::Category::PadUpdate),
          "clock ownership refuses setup without installing an entry or pad admission");
    Check(Admission::Release(clock), "release the fixture's clock token");
    Admission::Token input, render;
    Check(Admission::TryAcquire(Admission::Category::Input, input) && Admission::TryAcquire(Admission::Category::Render, render),
          "claim the actual coexisting local input and rendering categories");
    Check(owner.Begin(), "the actual owner installs real Detours on the qualified private updater");
    NativePadUpdateState active;
    owner.Read(active);
    Check(active.generation && active.bound && active.hooked && active.enabled && active.ownedEntry && !active.stopping && !active.retainedPeers,
          "successful setup publishes a bound enabled owner with exact captured entry identity");
    Check(Admission::IsClaimed(Admission::Category::Input) && Admission::IsClaimed(Admission::Category::Render) &&
              Admission::IsClaimed(Admission::Category::PadUpdate),
          "pad production coexists with input and rendering admission");
    Admission::Token forbidden{Admission::Category::Render, 999};
    Check(!Admission::TryAcquire(Admission::Category::Clock, forbidden) && forbidden.serial == 999 && forbidden.category == Admission::Category::Render,
          "clock refusal preserves caller token while pad update is admitted");
    mta::game_de::NativePadStateAccess states(fixturePage + 128, NoStateAccess);
    TestPad                            testPad(states, owner);
    CPad&                              sdk = testPad;
    Check(sdk.IsEnabled(), "the real SDK facade reads the actual native owner");
    CallUpdater(nullptr);
    Check(originalCalls.load() == 1, "enabled Detours callback invokes the original exactly once");
    sdk.Disable(true);
    CallUpdater(nullptr);
    Check(!sdk.IsEnabled() && originalCalls.load() == 1, "SDK Disable suppresses the entire native updater globally");
    sdk.Disable(false);
    CallUpdater(nullptr);
    Check(sdk.IsEnabled() && originalCalls.load() == 2 && stateAccesses.load() == 0,
          "SDK restoration resumes original production without writing any pad state or disable word");
    WrongThread wrong{&owner};
    Join(CreateThread(nullptr, 0, WrongProducer, &wrong, 0, nullptr));
    Check(!wrong.read && !wrong.disabled && wrong.output && sdk.IsEnabled(), "foreign producer thread refuses reads/writes and preserves output/state");
    NativePadUpdateControl competing;
    Check(!competing.Begin(), "a second producer cannot steal an active global generation");
    NativePadUpdateState unchanged;
    owner.Read(unchanged);
    Check(unchanged.generation == active.generation && unchanged.bound && unchanged.hooked, "competing setup preserves the original owner");
    const auto worker = BeginBlockedCall();
    Check(!owner.End(), "End defers while an actual original callback remains in flight");
    NativePadUpdateState pending;
    owner.Read(pending);
    Check(pending.bound && pending.hooked && pending.stopping && pending.enabled && pending.callbacks == 1 &&
              Admission::IsClaimed(Admission::Category::PadUpdate),
          "deferred cleanup retains callback, trampoline and admission while releasing suppression");
    ReleaseBlockedCall(worker);
    Check(owner.End(), "later End retires the actual hook once the callback returns");
    NativePadUpdateState retired;
    owner.Read(retired);
    Check(!retired.bound && !retired.hooked && !retired.callbacks && !retired.retainedPeers && retired.generation == active.generation && retired.calls == 4 &&
              retired.forwarded == 3 && retired.suppressed == 1,
          "retired readback preserves the real generation and final callback counters");
    Check(ReadEntry() == original && !Admission::IsClaimed(Admission::Category::PadUpdate), "detach restores original entry bytes before releasing admission");
    Check(Admission::Release(input) && Admission::Release(render), "retire only the fixture's input/render tokens");
    CallUpdater(nullptr);
    Check(originalCalls.load() == 4, "restored unhooked updater reaches its original body normally");
    Check(owner.End(), "repeated End remains harmless after successful retirement");

    NativePadUpdateControl foreign;
    Check(foreign.Begin(), "a later producer acquires a new independent owner generation");
    const auto owned = ReadEntry();
    ChangeByte(31, static_cast<unsigned char>(owned[31] ^ 1));
    const auto mutation = ReadEntry();
    bool       enabledOutput = true;
    Check(!foreign.IsEnabled(enabledOutput) && enabledOutput && !foreign.SetDisabled(true),
          "foreign entry mutation refuses producer operations without fake output");
    Check(!foreign.End() && ReadEntry() == mutation && Admission::IsClaimed(Admission::Category::PadUpdate),
          "cleanup never overwrites a foreign patch or releases its unresolved restoration debt");
    ChangeByte(31, owned[31]);
    Check(foreign.End() && ReadEntry() == original, "restoring only the fixture's foreign byte permits honest native detach");

    auto abandoned = std::make_unique<NativePadUpdateControl>();
    Check(abandoned->Begin(), "create an owned callback generation for abandoned cleanup");
    const auto abandonedWorker = BeginBlockedCall();
    abandoned.reset();
    NativePadUpdateControl afterAbandonment;
    Check(!afterAbandonment.Begin() && !NativePadUpdateControl::RecoverAbandoned() && Admission::IsClaimed(Admission::Category::PadUpdate),
          "destroyed producer retains process-level cleanup instead of releasing an active callback or permitting a stolen generation");
    ReleaseBlockedCall(abandonedWorker);
    Check(NativePadUpdateControl::RecoverAbandoned() && ReadEntry() == original && !Admission::IsClaimed(Admission::Category::PadUpdate),
          "explicit abandoned recovery restores actual bytes and shared admission after callback return");
    Check(afterAbandonment.Begin() && afterAbandonment.End(), "a new producer can start only after abandoned cleanup genuinely retires");
    Check(ReadEntry() == original && !Admission::IsClaimed(Admission::Category::PadUpdate), "all native owner episodes finish fully restored");
    Check(CloseHandle(entered) && CloseHandle(releaseOriginal), "retire fixture-owned callback coordination handles");
    fixturePage = 0;
    Check(VirtualFree(page, 0, MEM_RELEASE) != FALSE, "free executable private memory only after every native hook is detached");
    std::printf("Passed %u production pad updater owner checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
