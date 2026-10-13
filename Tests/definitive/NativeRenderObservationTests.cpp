/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the production observer against owned memory and hooks
 *****************************************************************************/

// The ordinary executable still fails the shipping Begin guard. Including the
// real translation unit exposes its private runtime to owned fixtures without
// adding a production bypass, fake image allowance, or exported test API.
#include "../../Client/game_de/NativeRenderObservation.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>

using namespace mta::game_de;

namespace
{
    unsigned                         checks = 0;
    std::atomic<unsigned>            originalCalls{0};
    HANDLE                           entered = nullptr, resume = nullptr;
    std::array<unsigned char, 0xc0>  viewport{};
    std::array<unsigned char, 0x200> rhi{};

    void Check(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, label);
            std::exit(1);
        }
    }
    template <typename T>
    void Put(std::uintptr_t address, T value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    __declspec(noinline) bool Original(void*, std::uint32_t interval)
    {
        originalCalls.fetch_add(1);
        return interval != 0 && GetCurrentThreadId() != 0;
    }
    __declspec(noinline) bool OriginalBlocked(void*, std::uint32_t interval)
    {
        originalCalls.fetch_add(1);
        SetEvent(entered);
        WaitForSingleObject(resume, 5000);
        return interval != 0;
    }
    bool OriginalException(void*, std::uint32_t)
    {
        originalCalls.fetch_add(1);
        RaiseException(0xe0424242, 0, 0, nullptr);
        return false;
    }
    bool OriginalNested(void* object, std::uint32_t interval)
    {
        originalCalls.fetch_add(1);
        if (callbackDepth == 1)
            HookPresent(object, interval);
        return true;
    }
    bool CatchOriginalException(void* object)
    {
        __try
        {
            HookPresent(object, 1);
        }
        __except (GetExceptionCode() == 0xe0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }
    void Configure(std::uintptr_t image, PresentFunction original = Original)
    {
        runtime.image = image;
        runtime.original = original;
        runtime.producerThread = GetCurrentThreadId();
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.hooked.store(false);
        runtime.started = GetTickCount64();
        ResetCounters();
        viewport.fill(0);
        rhi.fill(0);
        const auto object = reinterpret_cast<std::uintptr_t>(viewport.data());
        const auto owner = reinterpret_cast<std::uintptr_t>(rhi.data());
        Put(object, image + 0x44fbc08);
        Put(object + 0x18, owner);
        Put(object + 0x40, std::uintptr_t{0x1234});
        Put(object + 0x4c, std::uint32_t{1920});
        Put(object + 0x50, std::uint32_t{1080});
        // Deliberately unreadable object addresses prove that neither Capture
        // nor the producer tries COM or dereferences these borrowed interfaces.
        Put(object + 0x78, std::uintptr_t{1});
        Put(owner, image + 0x44fb2b8);
        Put(owner + 0x18, image + 0x44fafa8);
        Put(owner + 0x170, std::uintptr_t{2});
        Put(owner + 0x158, std::uintptr_t{3});
        Put(image + 0x576fb60, owner);
    }
}

int main(int argc, char** argv)
{
    Check(argc == 2, "pinned code fixture path supplied");
    std::array<unsigned char, presentBytes> fingerprint{};
    std::ifstream                           file(argv[1], std::ios::binary);
    file.read(reinterpret_cast<char*>(fingerprint.data()), fingerprint.size());
    Check(file.gcount() == static_cast<std::streamsize>(fingerprint.size()) && file.peek() == std::char_traits<char>::eof(), "exact complete421 byte fixture");
    {
        HashResources hash;
        Check(hash.Open() && BCryptHashData(hash.hash, fingerprint.data(), static_cast<ULONG>(fingerprint.size()), 0) >= 0 && hash.Finish(presentHash),
              "production digest accepts full pinned function");
    }
    for (std::size_t index = 0; index < fingerprint.size(); ++index)
    {
        auto altered = fingerprint;
        altered[index] ^= 1;
        HashResources hash;
        Check(hash.Open() && BCryptHashData(hash.hash, altered.data(), static_cast<ULONG>(altered.size()), 0) >= 0 && !hash.Finish(presentHash),
              "every native function byte is fingerprinted");
    }
    NativeRenderObservation owner;
    Check(!owner.Begin(), "ordinary executable rejected");
    NativeRenderObservationState state;
    owner.ReadState(state);
    Check(!state.bound && !state.active && !state.modulePinned && !runtime.hooked.load() && !runtime.original,
          "wrong image installs no hook and pins no module");
    Check(state.failureReason == static_cast<unsigned>(Failure::UnsupportedBuild), "wrong image has precise failure");
    Check(owner.End() && owner.End(), "unbound End idempotent");
    const auto image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x5770000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(image != 0, "owned synthetic image allocation");
    Configure(image);
    const auto originalViewport = viewport;
    const auto originalRhi = rhi;
    Check(HookPresent(viewport.data(), 2), "native bool result preserved");
    owner.ReadState(state);
    Check(state.nativePresentCallbacks == 1 && state.acceptedSnapshots == 1 && state.rejectedSnapshots == 0, "owned callback publishes scalar evidence");
    Check(state.width == 1920 && state.height == 1080 && state.syncInterval == 2 && state.callbackThreadId == GetCurrentThreadId(),
          "native dimensions and callback thread");
    Check(state.windowPresent && state.swapchainPresent && state.devicePresent && state.contextPresent, "native member presence observed without COM");
    Check(state.lastOriginalReturned && state.lastOriginalResult && state.failureReason == 0, "completed original evidence");
    Check(viewport == originalViewport && rhi == originalRhi, "observation never writes native objects");
    Check(!HookPresent(viewport.data(), 0), "false native bool preserved without trial failure");
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 2 && state.lastOriginalReturned && !state.lastOriginalResult && state.failureReason == 0,
          "false original is valid observation");

    Configure(image);
    NativeRenderObservationState unchanged;
    unchanged.width = 777;
    Put(reinterpret_cast<std::uintptr_t>(viewport.data()), image + 0x4500000);
    Check(!Capture(viewport.data(), 1, unchanged) && unchanged.width == 777, "invalid vtable preserves unpublished output");
    Check(HookPresent(viewport.data(), 1), "invalid viewport still forwards original");
    owner.ReadState(state);
    Check(
        !state.active && state.rejectedSnapshots == 1 && state.acceptedSnapshots == 0 && state.failureReason == static_cast<unsigned>(Failure::InvalidViewport),
        "invalid viewport disarms");
    Put(reinterpret_cast<std::uintptr_t>(viewport.data()), image + 0x44fbc08);
    HookPresent(viewport.data(), 1);
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 0 && state.nativePresentCallbacks == 2, "restored identity cannot silently rearm");

    Configure(image, OriginalException);
    Check(CatchOriginalException(viewport.data()), "original SEH propagates");
    Check(runtime.inFlight.load() == 0 && callbackDepth == 0, "SEH releases hook lifetime scope");
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 0 && !state.lastOriginalReturned, "exception is not published as returned original");
    Configure(image, OriginalNested);
    HookPresent(viewport.data(), 1);
    owner.ReadState(state);
    Check(state.nativePresentCallbacks == 2 && state.nestedCallbacks == 1 && !state.active &&
              state.failureReason == static_cast<unsigned>(Failure::NestedCallback),
          "nested callback forwards but disarms");
    Check(runtime.inFlight.load() == 0 && callbackDepth == 0, "nested lifetime unwinds");

    Configure(image);
    std::thread engine([] { HookPresent(viewport.data(), 1); });
    engine.join();
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 1 && state.callbackThreadId != GetCurrentThreadId(), "engine thread differs from producer");
    HookPresent(viewport.data(), 2);
    owner.ReadState(state);
    Check(state.foreignThreadCallbacks == 1 && state.acceptedSnapshots == 1 && state.active, "later foreign callback forwards without publication");
    Configure(image);
    AcquireSRWLockShared(&runtime.snapshotLock);
    std::thread contended([] { HookPresent(viewport.data(), 1); });
    contended.join();
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(state.publicationMisses == 1 && state.acceptedSnapshots == 0 && state.active, "engine publication never waits on producer lock");
    Configure(image);
    runtime.started = GetTickCount64() + 1000;
    owner.ReadState(state);
    Check(!state.active && state.failureReason == static_cast<unsigned>(Failure::TimeBudget), "backward observation clock disarms before snapshot");
    Configure(image);
    runtime.started = GetTickCount64() - maximumObservationMs;
    HookPresent(viewport.data(), 1);
    owner.ReadState(state);
    Check(!state.active && state.acceptedSnapshots == 0 && state.failureReason == static_cast<unsigned>(Failure::TimeBudget),
          "bounded observation expires before native capture");

    Configure(image, OriginalBlocked);
    entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    resume = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(entered && resume, "owned cross-thread barriers");
    bool pending = false;
    Check(ChangeHooks(true, pending), "genuine Detours attach on owned test function");
    runtime.hooked.store(true);
    PresentFunction volatile target = &OriginalBlocked;
    std::thread blocked([&] { target(viewport.data(), 1); });
    Check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "actual detoured engine callback entered");
    Check(!owner.End(), "producer End disarms and retains active callback");
    owner.ReadState(state);
    Check(!state.active && state.pendingCleanup && state.failureReason == 0 && runtime.hooked.load(), "busy cleanup is retryable without permanent failure");
    Check(!ChangeHooks(false, pending) && pending && runtime.hooked.load(), "post-suspension inFlight check aborts detach safely");
    SetEvent(resume);
    blocked.join();
    Check(runtime.inFlight.load() == 0, "aborted transaction resumes owned engine thread");
    Check(owner.End(), "producer cleanup retry genuinely detaches");
    owner.ReadState(state);
    Check(state.bound && !state.active && !state.pendingCleanup && state.nativePresentCallbacks == 1 && state.failureReason == 0,
          "End retains actual bound evidence and counters");
    const auto countAfterEnd = state.nativePresentCallbacks;
    target(viewport.data(), 1);
    owner.ReadState(state);
    Check(state.nativePresentCallbacks == countAfterEnd, "detached target stops entering observer");
    Check(owner.End(), "detached End remains idempotent");
    CloseHandle(entered);
    CloseHandle(resume);

    Configure(image);
    HookPresent(viewport.data(), 1);
    Check(owner.End(), "direct callback cleanup");
    owner.ReadState(state);
    const auto              oldGeneration = state.generation;
    NativeRenderObservation nextOwner;
    Check(!nextOwner.Begin(), "new owner still cannot admit wrong executable");
    NativeRenderObservationState retired;
    owner.ReadState(retired);
    Check(retired.generation == oldGeneration && retired.acceptedSnapshots == state.acceptedSnapshots && retired.width == state.width,
          "retired owner preserves own final evidence across generations");
    Check(nextOwner.End(), "failed next generation cleanup");
    Check(!runtime.modulePinned.load(), "synthetic fixtures never pin executable/module");
    VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE);
    std::printf("Passed %u production native render observation checks\n", checks);
    return 0;
}
