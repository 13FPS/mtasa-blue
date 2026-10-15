/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the production observer against owned memory and hooks
 *****************************************************************************/

// The ordinary executable still fails the shipping Begin guard. Including the
// real translation unit exposes its private runtime to owned fixtures without
// adding a production bypass, fake image allowance, or exported test API.
#include <windows.h>
namespace
{
    HANDLE (*fixtureBeforeSuspend)(HANDLE) = nullptr;
    void (*fixtureAfterSuspend)(HANDLE, DWORD, DWORD) = nullptr;
    void (*fixtureAfterClose)(HANDLE, BOOL) = nullptr;
}
DWORD WINAPI FixtureNativeSuspendThread(HANDLE thread)
{
    // Only this private compilation adapts the API call. The default path is
    // the real Win32 operation; controlled fixtures also forward it exactly
    // once and preserve its immediate error for the production refusal path.
    if (!fixtureBeforeSuspend && !fixtureAfterSuspend)
        return ::SuspendThread(thread);
    const auto selected = fixtureBeforeSuspend ? fixtureBeforeSuspend(thread) : thread;
    const auto result = ::SuspendThread(selected);
    const auto error = result == static_cast<DWORD>(-1) ? GetLastError() : ERROR_SUCCESS;
    if (fixtureAfterSuspend)
        fixtureAfterSuspend(thread, result, error);
    if (result == static_cast<DWORD>(-1))
        SetLastError(error);
    return result;
}
BOOL WINAPI FixtureNativeCloseHandle(HANDLE handle)
{
    const auto result = ::CloseHandle(handle);
    if (fixtureAfterClose)
    {
        const auto error = result ? ERROR_SUCCESS : GetLastError();
        fixtureAfterClose(handle, result);
        if (!result)
            SetLastError(error);
    }
    return result;
}
#define SuspendThread FixtureNativeSuspendThread
#define CloseHandle   FixtureNativeCloseHandle
#include "../../Client/game_de/NativeRenderObservation.cpp"
#undef CloseHandle
#undef SuspendThread
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <thread>
#include <unknwn.h>

#ifdef MTA_TEST_NATIVE_DISPATCH_UNAVAILABLE
int main()
{
    using namespace mta::game_de;
    NativeRenderObservation owner;
    if (owner.Begin(RenderObservationRoute::D3D12, true, true, true, true))
        return 1;
    NativeRenderObservationState state;
    owner.ReadState(state);
    if (state.failureReason != static_cast<unsigned>(NativeRenderObservationFailure::UnsupportedRoute) || state.pendingCleanup || state.modulePinned ||
        state.nativePresentCallbacks || state.acceptedSnapshots || state.gpu.attempts || state.boundary.status || state.nativeDispatch.status)
        return 2;
    if (!owner.End() || owner.Begin(RenderObservationRoute::D3D12, true, true, true))
        return 3;
    owner.ReadState(state);
    if (state.failureReason != static_cast<unsigned>(NativeRenderObservationFailure::UnsupportedBuild) || !owner.End())
        return 4;
    std::printf("Passed 4 native-dispatch unavailable-build checks\n");
    return 0;
}
#else
using namespace mta::game_de;
void CheckFrameBoundaryProbe(const char* customFixture, const char* wrapperFixture, unsigned& checks);
bool BoundaryFixtureOriginal(void*, std::uint32_t);
void PrimeBoundaryDiagnostic(std::uint64_t generation);
void PrimeNativeDispatchDiagnostic(std::uint64_t generation);
bool NativeDispatchFixtureOriginal(void*, std::uint32_t interval);

namespace
{
    unsigned                         checks = 0;
    std::atomic<unsigned>            originalCalls{0};
    HANDLE                           entered = nullptr, resume = nullptr;
    std::array<unsigned char, 0x1e0> viewport{};
    std::array<unsigned char, 0x900> rhi{};
    std::array<unsigned char, 0x40>  deviceObject{}, queueManager{};

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
    bool CatchBoundaryException(void* object)
    {
        __try
        {
            HookPresent(object, 2);
        }
        __except (GetExceptionCode() == 0xe0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }
    void Configure(std::uintptr_t image, PresentFunction original = Original, RenderObservationRoute route = RenderObservationRoute::D3D11)
    {
        runtime.image = image;
        runtime.original = original;
        runtime.route = SelectRoute(route);
        runtime.requestedRoute = static_cast<std::uint32_t>(route);
        // API1/2 never inherit opt-in COM work from a previously rejected API3 Begin.
        runtime.queryGpuOwnership = false;
        runtime.classifyCustomPresent = false;
        runtime.queryFrameBoundary = false;
        runtime.nativeDispatch = false;
        runtime.producerThread = GetCurrentThreadId();
        runtime.bound.store(true);
        runtime.active.store(true);
        runtime.hooked.store(false);
        runtime.started = GetTickCount64();
        ResetCounters();
        viewport.fill(0);
        rhi.fill(0);
        deviceObject.fill(0);
        queueManager.fill(0);
        const auto object = reinterpret_cast<std::uintptr_t>(viewport.data());
        const auto owner = reinterpret_cast<std::uintptr_t>(rhi.data());
        Put(object, image + runtime.route->viewportTable);
        Put(object + 0x18, owner);
        Put(object + 0x40, std::uintptr_t{0x1234});
        Put(object + 0x4c, std::uint32_t{1920});
        Put(object + 0x50, std::uint32_t{1080});
        // Deliberately unreadable object addresses prove that neither Capture
        // nor the producer tries COM or dereferences these borrowed interfaces.
        Put(object + runtime.route->swapchainOffset, std::uintptr_t{1});
        Put(owner, image + 0x44fb2b8);
        Put(owner + 0x18, image + 0x44fafa8);
        Put(owner + 0x170, std::uintptr_t{2});
        Put(owner + 0x158, std::uintptr_t{3});
        Put(image + 0x576fb60, owner);
        if (route == RenderObservationRoute::D3D12)
        {
            Put(owner + 0x8a8, reinterpret_cast<std::uintptr_t>(deviceObject.data()));
            Put(reinterpret_cast<std::uintptr_t>(deviceObject.data()) + 0x38, reinterpret_cast<std::uintptr_t>(queueManager.data()));
            Put(reinterpret_cast<std::uintptr_t>(queueManager.data()) + 0x28, std::uintptr_t{4});
            // The D3D12 callback does not borrow a D3D11 global root or assume
            // its device/context layout. The queue remains undereferenceable.
            Put(image + 0x576fb60, std::uintptr_t{1});
        }
    }
    void EnableOwnership(std::uintptr_t image, std::uint32_t explicitCount = 1, std::uint32_t afrCount = 1)
    {
        runtime.queryGpuOwnership = true;
        Put(image + 0x503e8e0, explicitCount);
        Put(image + 0x503f45c, afrCount);
    }
    struct OwnedUnknown final : IUnknown
    {
        unsigned                  calls = 0;
        std::uintptr_t            changeAddress = 0;
        bool                      succeed = false, failRelease = false;
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** output) override
        {
            ++calls;
            if (output)
                *output = nullptr;
            if (changeAddress)
                Put(changeAddress, std::uintptr_t{0x4321});
            if (succeed && output)
            {
                *output = this;
                return S_OK;
            }
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            if (failRelease)
                RaiseException(0xe0434343, 0, 0, nullptr);
            return 1;
        }
    };
    void CheckCodeFixture(const char* path, const RouteDescriptor& route)
    {
        std::vector<unsigned char> fingerprint(route.presentBytes);
        std::ifstream              file(path, std::ios::binary);
        file.read(reinterpret_cast<char*>(fingerprint.data()), static_cast<std::streamsize>(fingerprint.size()));
        Check(file.gcount() == static_cast<std::streamsize>(fingerprint.size()) && file.peek() == std::char_traits<char>::eof(),
              "exact complete route function fixture");
        HashResources original;
        Check(original.Open() && BCryptHashData(original.hash, fingerprint.data(), static_cast<ULONG>(fingerprint.size()), 0) >= 0 &&
                  original.Finish(route.presentHash),
              "production route digest accepts full pinned function");
        for (std::size_t index = 0; index < fingerprint.size(); ++index)
        {
            auto altered = fingerprint;
            altered[index] ^= 1;
            HashResources hash;
            Check(hash.Open() && BCryptHashData(hash.hash, altered.data(), static_cast<ULONG>(altered.size()), 0) >= 0 && !hash.Finish(route.presentHash),
                  "every route native function byte is fingerprinted");
        }
        HashResources foreign;
        const auto&   other = route.route == RenderObservationRoute::D3D11 ? d3d12Route : d3d11Route;
        Check(foreign.Open() && BCryptHashData(foreign.hash, fingerprint.data(), static_cast<ULONG>(fingerprint.size()), 0) >= 0 &&
                  !foreign.Finish(other.presentHash),
              "one route cannot borrow another full function fingerprint");
    }
    struct OwnedCustomReads final : CustomPresentReads
    {
        std::uintptr_t                            image = 0x140000000, presenter = 0x10000, table = image + CustomPresentTableRva, device = 0x20000;
        std::array<std::uintptr_t, 7>             slots{};
        std::array<std::vector<unsigned char>, 7> code;
        unsigned                                  reads = 0, fail = 0;
        bool                                      exception = false, imageOwned = true, executable = true;
        bool                                      Fault()
        {
            if (++reads != fail)
                return false;
            if (exception)
                RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
            return true;
        }
        bool Object(std::uintptr_t address, void* output, std::size_t bytes) override
        {
            if (Fault() || bytes != sizeof(std::uintptr_t))
                return false;
            if (address == presenter)
                std::memcpy(output, &table, bytes);
            else if (address == presenter + 0x18)
                std::memcpy(output, &device, bytes);
            else
                return false;
            return true;
        }
        bool Image(std::uintptr_t address, void* output, std::size_t bytes, bool execute) override
        {
            if (Fault() || !imageOwned || (execute && !executable))
                return false;
            if (address == image + CustomPresentTableRva && bytes == sizeof(slots) && !execute)
            {
                std::memcpy(output, slots.data(), bytes);
                return true;
            }
            const auto& signatures = CustomPresentSignatures();
            for (std::size_t index = 0; index < signatures.size(); ++index)
            {
                if (execute && address == image + signatures[index].functionRva && bytes == code[index].size())
                {
                    std::memcpy(output, code[index].data(), bytes);
                    return true;
                }
            }
            return false;
        }
    };
    void CheckCustomFixture(const char* path)
    {
        using Classification = CustomPresentClassificationStatus;
        const auto       number = [](Classification status) { return static_cast<std::uint32_t>(status); };
        OwnedCustomReads fixture;
        const auto&      signatures = CustomPresentSignatures();
        std::ifstream    file(path, std::ios::binary);
        for (std::size_t index = 0; index < signatures.size(); ++index)
        {
            fixture.slots[index] = fixture.image + signatures[index].functionRva;
            fixture.code[index].resize(signatures[index].bytes);
            file.read(reinterpret_cast<char*>(fixture.code[index].data()), static_cast<std::streamsize>(fixture.code[index].size()));
            Check(file.gcount() == static_cast<std::streamsize>(fixture.code[index].size()), "complete presenter function fixture bytes supplied");
        }
        Check(file.peek() == std::char_traits<char>::eof() && fixture.code[4] == fixture.code[6], "shared no-op slots use the same complete three-byte leaf");
        CustomPresentClassificationState state;
        CustomPresentProbeRoots          roots;
        ClassifyCustomPresent(fixture, fixture.image, fixture.presenter, fixture.device, state, roots);
        Check(state.status == number(Classification::KnownNativePresenter) && state.implementationId == 1 && state.slotSignatureMask == 127 &&
                  state.staticPolicyFlags == 15 && CustomPresentRootsUnchanged(fixture, roots),
              "all seven actual signatures and native device association identify only static presenter policy");
        auto absent = fixture;
        absent.reads = 0;
        ClassifyCustomPresent(absent, absent.image, 0, absent.device, state, roots);
        Check(
            state.status == number(Classification::Absent) && !state.implementationId && !state.slotSignatureMask && !state.staticPolicyFlags && !absent.reads,
            "absent presenter clears reused success without object or image reads");
        auto unknown = fixture;
        unknown.table = 0x30000;
        ClassifyCustomPresent(unknown, unknown.image, unknown.presenter, unknown.device, state, roots);
        Check(state.status == number(Classification::UnknownImplementation) && !state.implementationId && !state.slotSignatureMask && !state.staticPolicyFlags,
              "an unknown or third-party table is never followed or classified as native");
        for (std::size_t slot = 0; slot < signatures.size(); ++slot)
        {
            auto altered = fixture;
            altered.slots[slot] += 1;
            ClassifyCustomPresent(altered, altered.image, altered.presenter, altered.device, state, roots);
            Check(state.status == number(Classification::SignatureMismatch) && state.slotSignatureMask == (1u << slot) - 1 && !state.implementationId &&
                      !state.staticPolicyFlags,
                  "each mismatched vtable slot rejects the known class and retains only its verified prefix");
        }
        // Mutation of every byte, including leaf immediates and function tails,
        // proves that a recognizable prologue cannot admit a changed policy.
        for (std::size_t slot = 0; slot < 6; ++slot)
        {
            for (std::size_t byte = 0; byte < fixture.code[slot].size(); ++byte)
            {
                auto altered = fixture;
                altered.code[slot][byte] ^= 1;
                ClassifyCustomPresent(altered, altered.image, altered.presenter, altered.device, state, roots);
                Check(state.status == number(Classification::SignatureMismatch) && state.slotSignatureMask == (1u << slot) - 1 && !state.implementationId &&
                          !state.staticPolicyFlags,
                      "every complete presenter code byte is checked before policy publication");
            }
        }
        for (unsigned read = 1; read <= 10; ++read)
        {
            for (const bool exception : {false, true})
            {
                auto failed = fixture;
                failed.reads = 0;
                failed.fail = read;
                failed.exception = exception;
                ClassifyCustomPresent(failed, failed.image, failed.presenter, failed.device, state, roots);
                const auto verifiedMask = read <= 3 ? 0u : (1u << (read - 3)) - 1;
                Check(state.status == number(Classification::ReadFault) && state.slotSignatureMask == verifiedMask && !state.implementationId &&
                          !state.staticPolicyFlags,
                      "every object/table/code/device read fault or SEH clears known implementation and policy");
            }
        }
        for (const bool ownerFault : {false, true})
        {
            auto failed = fixture;
            failed.imageOwned = !ownerFault;
            failed.executable = ownerFault;
            ClassifyCustomPresent(failed, failed.image, failed.presenter, failed.device, state, roots);
            Check(state.status == number(Classification::ReadFault) && !state.implementationId && !state.staticPolicyFlags,
                  "foreign image pages or nonexecutable code cannot satisfy signature admission");
        }
        ClassifyCustomPresent(fixture, fixture.image, fixture.presenter, fixture.device + 1, state, roots);
        Check(state.status == number(Classification::DeviceMismatch) && state.slotSignatureMask == 127 && !state.implementationId && !state.staticPolicyFlags,
              "a complete known table cannot claim another native device association");
        ClassifyCustomPresent(fixture, fixture.image, fixture.presenter, fixture.device, state, roots);
        auto changed = fixture;
        changed.table += 8;
        Check(!CustomPresentRootsUnchanged(changed, roots), "post-query table mutation rejects the borrowed classification");
        changed = fixture;
        changed.device += 8;
        Check(!CustomPresentRootsUnchanged(changed, roots), "post-query native device mutation rejects the borrowed classification");
        changed = fixture;
        changed.reads = 0;
        changed.fail = 1;
        Check(!CustomPresentRootsUnchanged(changed, roots), "post-query read failure cannot retain a known policy");
    }

}

bool OwnedNativeDispatchCleanup(bool& pending)
{
    using namespace mta::game_de;
    runtime.nativeDispatch = runtime.queryFrameBoundary = true;
    return ChangeHooks(false, pending);
}
void* OwnedNativeDispatchWrapperHook()
{
    return reinterpret_cast<void*>(&mta::game_de::HookPresent);
}
bool OwnedNativeDispatchTransaction(bool install, std::uintptr_t wrapper, bool& pending)
{
    using namespace mta::game_de;
    if (install)
    {
        Check(!runtime.hooked.load() && runtime.retainedSuspensions.empty() && !runtime.safetyHeldSuspensions && !runtime.inFlight.load() &&
                  !FrameBoundaryInFlight(),
              "complete API6 fixture begins without inherited hooks or holds");
        // Only the private owned test advances past admission. The production
        // Begin image/fingerprint gates are unchanged, and this code is never
        // compiled into Game DE. No copied native function is ever invoked.
        runtime.image = 0;
        runtime.original = reinterpret_cast<PresentFunction>(wrapper);
        runtime.producerThread = GetCurrentThreadId();
        runtime.nativeDispatch = runtime.queryFrameBoundary = true;
        runtime.active.store(false);
        InitializeSetupDiagnostic(runtime.generation.load());
    }
    const bool result = ChangeHooks(install, pending);
    if (result)
        Check(runtime.hooked.load() == install && runtime.retainedSuspensions.empty() && !runtime.safetyHeldSuspensions && !runtime.inFlight.load() &&
                  !FrameBoundaryInFlight() &&
                  (install ? reinterpret_cast<std::uintptr_t>(runtime.original) != wrapper : reinterpret_cast<std::uintptr_t>(runtime.original) == wrapper),
              "complete API6 transaction owns/restores native trampoline and balances every production suspension");
    if (result && !install)
    {
        runtime.original = nullptr;
        runtime.nativeDispatch = runtime.queryFrameBoundary = false;
    }
    return result;
}
void OwnedNativeDispatchSetupSnapshot(mta::game_de::RenderSetupDiagnosticV1& state)
{
    state = mta::game_de::runtime.setup;
}
bool OwnedNativeDispatchSetupRefusal(std::uintptr_t wrapper, bool afterHolds, mta::game_de::RenderSetupDiagnosticV1& state)
{
    using namespace mta::game_de;
    const auto                    savedEntry = runtime.nextThread;
    const auto                    savedImage = runtime.nextThreadImage;
    std::array<unsigned char, 24> foreignEntry{};
    if (!afterHolds)
        runtime.nextThread = reinterpret_cast<NextThread>(foreignEntry.data());
    bool       pending = false;
    const bool installed = OwnedNativeDispatchTransaction(true, afterHolds ? 0 : wrapper, pending);
    runtime.nextThread = savedEntry;
    runtime.nextThreadImage = savedImage;
    state = runtime.setup;
    runtime.original = reinterpret_cast<PresentFunction>(wrapper);
    return !installed && !pending && !runtime.hooked.load() && runtime.retainedSuspensions.empty() && !runtime.safetyHeldSuspensions;
}
bool OwnedNativeDispatchSafetyHeld()
{
    using namespace mta::game_de;
    return runtime.safetyHeldSuspensions && !runtime.retainedSuspensions.empty();
}
bool OwnedNativeDispatchDiscovery(void (*afterSnapshot)(), DWORD (*newThreadId)())
{
    using namespace mta::game_de;
    ThreadHandles threads, held;
    bool          ready = PrepareNativeThreadEnumeration() && CollectThreads(threads);
    if (ready)
    {
        afterSnapshot();
        const auto childId = newThreadId();
        const bool childWasAbsent = std::none_of(threads.begin(), threads.end(), [&](HANDLE thread) { return GetThreadId(thread) == childId; });
        ready = HoldNativeDispatchThreads(threads, held);
        const auto found = std::find_if(threads.begin(), threads.end(), [&](HANDLE thread) { return GetThreadId(thread) == childId; });
        ready = ready && childWasAbsent && found != threads.end();
        if (ready)
            ready = SuspendThread(*found) == 1 && ResumeThread(*found) == 2;
    }
    for (const auto thread : held)
        if (thread)
            ResumeThread(thread);
    for (const auto thread : threads)
        CloseHandle(thread);
    return ready;
}
namespace
{
    struct ExitingDiscoveryChild
    {
        HANDLE ready = nullptr, release = nullptr, thread = nullptr;
        DWORD  id = 0;
    };
    struct ExitingDiscoveryFixture
    {
        unsigned                             mode = 0, passes = 0, deadCalls = 0, liveCalls = 0;
        HANDLE                               create = nullptr, created = nullptr, creatorReady = nullptr, creatorStop = nullptr, restricted = nullptr;
        HANDLE                               deadIterator = nullptr, deadDuplicate = nullptr;
        unsigned                             iteratorCloses = 0, duplicateCloses = 0;
        ExitingDiscoveryChild                dead, live;
        std::array<ExitingDiscoveryChild, 2> extraDead{};
        std::array<unsigned, 2>              extraDeadCalls{};
        std::array<unsigned, 2>              extraIteratorCloses{}, extraDuplicateCloses{};
        std::array<std::array<HANDLE, 4>, 2> extraIterators{}, extraDuplicates{};
        std::array<std::array<bool, 4>, 2>   extraClosedIterators{}, extraClosedDuplicates{};
        std::array<HANDLE, 4>                deadIterators{}, deadDuplicates{};
        std::array<bool, 4>                  closedIterators{}, closedDuplicates{};
        NextThread                           originalNext = nullptr;
        CompareThreadObjects                 originalCompare = nullptr;
        unsigned                             unknownComparisons = 0, continuedDeadCalls = 0;
        bool  started = false, valid = true, continuedExitedIterator = false, syncRequested = true, liveContextChecked = false, deadHandlesClosed = false;
        DWORD deadResult = 0, deadError = 0, deadWait = WAIT_FAILED, liveResult = static_cast<DWORD>(-1);
        DWORD deadExitCode = 0;
        bool  deadExitProved = false, extraDeadReferencesReleased = false;
    };
    ExitingDiscoveryFixture* exitingDiscoveryFixture = nullptr;
    DWORD WINAPI             ExitingDiscoveryChildBody(void* argument)
    {
        auto& child = *static_cast<ExitingDiscoveryChild*>(argument);
        SetEvent(child.ready);
        WaitForSingleObject(child.release, INFINITE);
        return STILL_ACTIVE;  // A signaled object, not exit259, must prove exit.
    }
    LONG NTAPI ExitingDiscoveryNext(HANDLE process, HANDLE previous, ACCESS_MASK access, ULONG attributes, ULONG flags, PHANDLE next)
    {
        auto& fixture = *exitingDiscoveryFixture;
        if (!previous)
            ++fixture.passes;
        if (previous && previous == fixture.deadIterator)
        {
            fixture.continuedExitedIterator = true;
            ++fixture.continuedDeadCalls;
            fixture.valid = fixture.valid && WaitForSingleObject(previous, 0) == WAIT_OBJECT_0;
            // This reference is consumed by the call below. Forget its value
            // before later iterator opens can reuse the closed handle number.
            fixture.deadIterator = nullptr;
        }
        fixture.syncRequested = fixture.syncRequested && (access & SYNCHRONIZE) != 0;
        if (fixture.liveCalls && !fixture.liveContextChecked)
        {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            fixture.liveContextChecked =
                GetThreadContext(fixture.live.thread, &context) != FALSE && NativeDispatchInstructionSafe(static_cast<std::uintptr_t>(context.Rip), 0);
        }
        auto status = fixture.originalNext(process, previous, access, attributes, flags, next);
        // The private proxy still acquires every object from the real native
        // iterator. Delayed first sightings model a creator whose later child
        // was behind the scan; each newly discovered dead object must still
        // reset stability even when retained earlier exits are neutral.
        auto hidden = [&]()
        {
            const auto id = GetThreadId(*next);
            return (id == fixture.live.id && fixture.passes == 1 && (fixture.mode == 0 || fixture.mode == 6)) ||
                   (fixture.mode == 6 && ((id == fixture.extraDead[0].id && fixture.passes < 2) || (id == fixture.extraDead[1].id && fixture.passes < 3)));
        };
        while (status == 0 && hidden())
        {
            const auto skipped = *next;
            status = fixture.originalNext(process, skipped, access, attributes, flags, next);
            CloseHandle(skipped);
        }
        if (status == 0 && GetThreadId(*next) == fixture.dead.id && fixture.mode == 2)
        {
            HANDLE          withoutSync = nullptr;
            constexpr DWORD control = THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION;
            if (!DuplicateHandle(GetCurrentProcess(), *next, GetCurrentProcess(), &withoutSync, control, FALSE, 0))
            {
                CloseHandle(*next);
                *next = nullptr;
                fixture.valid = false;
                return static_cast<LONG>(0xc0000001u);
            }
            CloseHandle(*next);
            *next = withoutSync;
        }
        if (status == 0 && GetThreadId(*next) == fixture.dead.id)
        {
            fixture.deadIterator = *next;
            if (fixture.deadCalls < fixture.deadIterators.size())
                fixture.deadIterators[fixture.deadCalls] = *next;
            else
                fixture.valid = false;
        }
        for (unsigned index = 0; status == 0 && index < fixture.extraDead.size(); ++index)
            if (fixture.extraDead[index].thread && GetThreadId(*next) == fixture.extraDead[index].id)
            {
                if (fixture.extraDeadCalls[index] < fixture.extraIterators[index].size())
                    fixture.extraIterators[index][fixture.extraDeadCalls[index]] = *next;
                else
                    fixture.valid = false;
            }
        return status;
    }
    LONG NTAPI ExitingDiscoveryCompare(HANDLE first, HANDLE second)
    {
        auto& fixture = *exitingDiscoveryFixture;
        if (fixture.mode == 7 && fixture.originalCompare(first, fixture.dead.thread) == 0 && fixture.originalCompare(second, fixture.dead.thread) == 0)
        {
            ++fixture.unknownComparisons;
            return static_cast<LONG>(0xc0000008u);
        }
        return fixture.originalCompare(first, second);
    }
    HANDLE ExitingDiscoveryBeforeSuspend(HANDLE thread)
    {
        auto& fixture = *exitingDiscoveryFixture;
        if (!fixture.started)
        {
            // This is before the first actual initial hold. The creator and
            // children can finish their loader/CRT work before any peer stops.
            fixture.started = true;
            const bool created = SetEvent(fixture.create) != FALSE && WaitForSingleObject(fixture.created, 5000) == WAIT_OBJECT_0;
            fixture.valid = fixture.valid && created;
            Sleep(5);
            fixture.originalNext = runtime.nextThread;
            fixture.originalCompare = runtime.compareThreadObjects;
            runtime.nextThread = &ExitingDiscoveryNext;
            if (fixture.mode == 7)
                runtime.compareThreadObjects = &ExitingDiscoveryCompare;
        }
        for (unsigned index = 0; index < fixture.extraDead.size(); ++index)
            if (fixture.extraDead[index].thread && GetThreadId(thread) == fixture.extraDead[index].id)
            {
                auto& child = fixture.extraDead[index];
                DWORD exitCode = 0;
                ++fixture.extraDeadCalls[index];
                if (fixture.extraDeadCalls[index] <= fixture.extraDuplicates[index].size())
                    fixture.extraDuplicates[index][fixture.extraDeadCalls[index] - 1] = thread;
                else
                    fixture.valid = false;
                fixture.valid = SetEvent(child.release) != FALSE && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 &&
                                GetExitCodeThread(thread, &exitCode) != FALSE && exitCode == STILL_ACTIVE && fixture.valid;
                return thread;
            }
        if (GetThreadId(thread) != fixture.dead.id)
            return thread;
        ++fixture.deadCalls;
        fixture.deadDuplicate = thread;
        if (fixture.deadCalls <= fixture.deadDuplicates.size())
            fixture.deadDuplicates[fixture.deadCalls - 1] = thread;
        else
            fixture.valid = false;
        fixture.deadHandlesClosed = false;
        if (fixture.mode != 1)
        {
            const auto proofHandle = fixture.dead.thread ? fixture.dead.thread : thread;
            fixture.deadExitProved = SetEvent(fixture.dead.release) != FALSE && WaitForSingleObject(proofHandle, 5000) == WAIT_OBJECT_0 &&
                                     GetExitCodeThread(proofHandle, &fixture.deadExitCode) != FALSE;
            fixture.valid = fixture.valid && fixture.deadExitProved;
            if (fixture.mode == 0 && fixture.deadExitProved)
            {
                // Production's new strong identity handle must keep the object
                // valid even after its creator releases these extra references.
                // Other positive modes deliberately retain all creator refs.
                if (fixture.dead.thread && CloseHandle(fixture.dead.thread))
                    fixture.dead.thread = nullptr;
                if (fixture.restricted && CloseHandle(fixture.restricted))
                    fixture.restricted = nullptr;
                fixture.extraDeadReferencesReleased = !fixture.dead.thread && !fixture.restricted;
                fixture.valid = fixture.valid && fixture.extraDeadReferencesReleased;
            }
        }
        fixture.deadWait = WaitForSingleObject(thread, 0);
        // Live denial uses a real same-object restricted handle. Other-error
        // denial uses a real invalid handle while the candidate is signaled.
        // Neither control fabricates a SuspendThread return or last error.
        return fixture.mode == 1 ? fixture.restricted : fixture.mode == 3 ? nullptr : thread;
    }
    void ExitingDiscoveryAfterSuspend(HANDLE thread, DWORD result, DWORD error)
    {
        auto&      fixture = *exitingDiscoveryFixture;
        const auto id = GetThreadId(thread);
        if (id == fixture.dead.id)
        {
            fixture.deadResult = result;
            fixture.deadError = error;
        }
        if (id == fixture.live.id)
        {
            ++fixture.liveCalls;
            fixture.liveResult = result;
        }
    }
    void ExitingDiscoveryAfterClose(HANDLE handle, BOOL result)
    {
        auto& fixture = *exitingDiscoveryFixture;
        for (unsigned child = 0; child < fixture.extraDead.size(); ++child)
            for (unsigned index = 0; index < fixture.extraDeadCalls[child]; ++index)
            {
                if (handle == fixture.extraDuplicates[child][index] && !fixture.extraClosedDuplicates[child][index])
                {
                    fixture.extraClosedDuplicates[child][index] = true;
                    ++fixture.extraDuplicateCloses[child];
                }
                else if (handle == fixture.extraIterators[child][index] && !fixture.extraClosedIterators[child][index])
                {
                    fixture.extraClosedIterators[child][index] = true;
                    ++fixture.extraIteratorCloses[child];
                }
                else
                    continue;
                fixture.valid = fixture.valid && result != FALSE;
                return;
            }
        for (unsigned index = 0; index < fixture.deadCalls; ++index)
        {
            if (handle == fixture.deadDuplicates[index] && !fixture.closedDuplicates[index])
            {
                fixture.closedDuplicates[index] = true;
                ++fixture.duplicateCloses;
            }
            else if (handle == fixture.deadIterators[index] && !fixture.closedIterators[index])
            {
                fixture.closedIterators[index] = true;
                ++fixture.iteratorCloses;
            }
            else
                continue;
            fixture.valid = fixture.valid && result != FALSE;
            break;
        }
        // Numeric values may be reused after a completed close; only current
        // unclosed references participate in exact close accounting.
        fixture.deadHandlesClosed = fixture.duplicateCloses == fixture.deadCalls && fixture.iteratorCloses == fixture.deadCalls;
    }
}
bool OwnedNativeDispatchExitingInstall(std::uintptr_t wrapper, unsigned mode, mta::game_de::RenderSetupDiagnosticV1& state)
{
    using namespace mta::game_de;
    ExitingDiscoveryFixture fixture;
    fixture.mode = mode;
    const std::array<ExitingDiscoveryChild*, 4> children{&fixture.dead, &fixture.live, &fixture.extraDead[0], &fixture.extraDead[1]};
    const unsigned                              childCount = mode == 6 ? 4u : mode == 5 ? 3u : 2u;
    fixture.create = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    fixture.created = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    fixture.creatorReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    fixture.creatorStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    for (unsigned index = 0; index < childCount; ++index)
    {
        auto* child = children[index];
        child->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        child->release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(child->ready && child->release, "exiting discovery owns child barriers before any hold");
    }
    Check(fixture.create && fixture.created && fixture.creatorReady && fixture.creatorStop, "exiting discovery owns creator barriers before any hold");
    std::thread creator(
        [&]
        {
            SetEvent(fixture.creatorReady);
            WaitForSingleObject(fixture.create, INFINITE);
            for (unsigned index = 0; index < childCount; ++index)
            {
                auto* child = children[index];
                child->thread = CreateThread(nullptr, 0, &ExitingDiscoveryChildBody, child, 0, &child->id);
                if (!child->thread || WaitForSingleObject(child->ready, 5000) != WAIT_OBJECT_0)
                    fixture.valid = false;
            }
            fixture.restricted = OpenThread(THREAD_QUERY_INFORMATION | SYNCHRONIZE, FALSE, fixture.dead.id);
            if (!fixture.restricted)
                fixture.valid = false;
            SetEvent(fixture.created);
            WaitForSingleObject(fixture.creatorStop, INFINITE);
        });
    Check(WaitForSingleObject(fixture.creatorReady, 5000) == WAIT_OBJECT_0, "creator is present in the actual initial thread snapshot");
    exitingDiscoveryFixture = &fixture;
    fixtureBeforeSuspend = &ExitingDiscoveryBeforeSuspend;
    fixtureAfterSuspend = &ExitingDiscoveryAfterSuspend;
    fixtureAfterClose = &ExitingDiscoveryAfterClose;
    bool       pending = false;
    const bool installed = OwnedNativeDispatchTransaction(true, wrapper, pending);
    fixtureBeforeSuspend = nullptr;
    fixtureAfterSuspend = nullptr;
    fixtureAfterClose = nullptr;
    if (fixture.originalNext)
        runtime.nextThread = fixture.originalNext;
    if (fixture.originalCompare)
        runtime.compareThreadObjects = fixture.originalCompare;
    exitingDiscoveryFixture = nullptr;
    state = runtime.setup;
    const bool     positive = mode == 0 || mode == 4 || mode == 5;
    const unsigned expectedDeadCalls = mode == 0 || mode == 6 ? 4u : mode == 4 || mode == 5 ? 3u : mode == 7 ? 2u : 1u;
    bool           distinctDeadHandles = true;
    for (unsigned index = 0; index < fixture.deadCalls; ++index)
        distinctDeadHandles = fixture.deadDuplicates[index] != fixture.deadIterators[index] && distinctDeadHandles;
    Check(fixture.started && fixture.valid && fixture.originalNext && fixture.syncRequested && fixture.deadCalls == expectedDeadCalls,
          "production discovery enumerates and duplicates an owned child with synchronization rights before its actual suspension attempt");
    Check(fixture.deadResult == static_cast<DWORD>(-1) && fixture.deadError == static_cast<DWORD>(mode == 3 ? ERROR_INVALID_HANDLE : ERROR_ACCESS_DENIED),
          "production stable suspension receives the actual Win32 failure without replacing its original error");
    Check(positive || mode == 6 || mode == 7 ? fixture.continuedExitedIterator && fixture.continuedDeadCalls == expectedDeadCalls - (mode == 7 ? 1u : 0u)
                                             : !fixture.continuedExitedIterator,
          "every accepted dead branch continues from the still-owned native iterator; failing controls close without continuing");
    Check(distinctDeadHandles && fixture.duplicateCloses == expectedDeadCalls && fixture.iteratorCloses == expectedDeadCalls && fixture.deadHandlesClosed,
          "production closes every iterator after continuation and each independent dead duplicate/cache handle exactly once");
    Check(!pending && runtime.retainedSuspensions.empty() && !runtime.safetyHeldSuspensions,
          "every completed exit/refusal pipeline balances its initial holds without retaining ownership");
    for (unsigned index = 0; index < fixture.extraDead.size(); ++index)
        Check(fixture.extraIteratorCloses[index] == fixture.extraDeadCalls[index] && fixture.extraDuplicateCloses[index] == fixture.extraDeadCalls[index],
              "every additional dead-object iterator and strong cache/duplicate handle closes once on success and churn refusal");
    if (positive)
    {
        const unsigned expectedPasses = mode == 0 ? 4u : 3u;
        Check(installed && fixture.deadWait == WAIT_OBJECT_0 && fixture.passes == expectedPasses && state.stablePasses == expectedPasses &&
                  fixture.liveCalls == 1 && fixture.liveResult == 0 && fixture.liveContextChecked && state.heldThreads == state.initialThreads + 1,
              "retained exited objects do not starve later live discovery; first sightings reset and two full later passes complete within four");
        Check(fixture.deadExitProved && fixture.deadExitCode == STILL_ACTIVE &&
                  (mode == 0 ? fixture.extraDeadReferencesReleased : !fixture.extraDeadReferencesReleased),
              "real wait proves exit259 with both released-creator and fully retained-reference lifetime cases");
        if (mode == 5)
            Check(fixture.extraDeadCalls[0] == 3 && !fixture.extraDeadCalls[1],
                  "multiple distinct retained exited objects each require fresh Suspend5/wait proof on all three passes");
    }
    else if (mode == 6)
        Check(!installed && fixture.deadExitProved && !fixture.extraDeadReferencesReleased && fixture.deadWait == WAIT_OBJECT_0 && fixture.passes == 4 &&
                  state.stablePasses == 4 && state.stage == static_cast<unsigned>(RenderSetupStage::StableLimit) && state.failureStage == state.stage &&
                  !state.errorDomain && !state.errorCode && !state.transactionCommitted && !state.hooksCommitted && !state.installSucceeded &&
                  fixture.liveCalls == 1 && state.heldThreads == state.initialThreads + 1 && fixture.extraDeadCalls[0] == 3 && fixture.extraDeadCalls[1] == 2,
              "first-seen dead-object churn resets stability and exhausts four passes before any transaction instead of being treated as neutral");
    else if (mode == 7)
        Check(!installed && fixture.passes == 2 && fixture.unknownComparisons == 1 && state.stage == static_cast<unsigned>(RenderSetupStage::StableSuspend) &&
                  state.failureStage == state.stage && state.errorDomain == static_cast<unsigned>(RenderSetupErrorDomain::NtStatus) &&
                  state.errorCode == 0xc0000008u && !state.transactionCommitted && !state.hooksCommitted && !state.installSucceeded,
              "an unknown raw object-comparison status refuses before any transaction without interpreting it as equality or inequality");
    else
        Check(!installed && fixture.passes == 1 && !fixture.liveCalls && state.stage == static_cast<unsigned>(RenderSetupStage::StableSuspend) &&
                  state.failureStage == state.stage && state.errorDomain == static_cast<unsigned>(RenderSetupErrorDomain::Win32) &&
                  state.errorCode == fixture.deadError && !state.transactionCommitted && !state.hooksCommitted && !state.installSucceeded &&
                  state.heldThreads == state.initialThreads &&
                  fixture.deadWait == (mode == 1   ? WAIT_TIMEOUT
                                       : mode == 2 ? WAIT_FAILED
                                                   : WAIT_OBJECT_0),
              "live denial, failed wait and other suspension errors retain exact pretransaction stage18 refusal without skipping a thread");
    Check(SuspendThread(fixture.live.thread) == 0 && ResumeThread(fixture.live.thread) == 1 && SuspendThread(creator.native_handle()) == 0 &&
              ResumeThread(creator.native_handle()) == 1,
          "production borrowed enrollment and cleanup restore exactly the original creator/live-child suspend counts");
    for (unsigned index = 0; index < childCount; ++index)
        SetEvent(children[index]->release);
    SetEvent(fixture.creatorStop);
    creator.join();
    for (unsigned index = 0; index < childCount; ++index)
    {
        auto* child = children[index];
        if (child->thread)
        {
            Check(WaitForSingleObject(child->thread, 5000) == WAIT_OBJECT_0, "owned discovery child finishes cooperatively without forced termination");
            Check(CloseHandle(child->thread) != FALSE, "retained owned child reference closes after join");
        }
        else
            Check(child == &fixture.dead && fixture.deadExitProved && fixture.extraDeadReferencesReleased,
                  "only the already joined positive dead child can have its fixture reference released early");
        Check(CloseHandle(child->ready) && CloseHandle(child->release), "owned discovery child control handles close after join");
    }
    Check((!fixture.restricted || CloseHandle(fixture.restricted)) && CloseHandle(fixture.create) && CloseHandle(fixture.created) &&
              CloseHandle(fixture.creatorReady) && CloseHandle(fixture.creatorStop),
          "owned discovery creator and restricted control handles are released");
    return positive ? installed : !installed;
}
DWORD WINAPI RefuseThreadResume(HANDLE)
{
    return static_cast<DWORD>(-1);
}
bool OwnedNativeDispatchVmTransactions()
{
    using namespace mta::game_de;
    Check(PrepareNativeThreadEnumeration(), "exact local NtGetNextThread stub admits bounded pre-hold enumeration");
    const auto                    savedEntry = runtime.nextThread;
    const auto                    savedImage = runtime.nextThreadImage;
    std::array<unsigned char, 24> foreignStub{};
    Check(Read(reinterpret_cast<std::uintptr_t>(savedEntry), foreignStub.data(), foreignStub.size()), "owned fixture reads only cached system stub");
    Check(NativeThreadStubMatches(foreignStub), "all24 original system stub bytes match the admitted profile");
    for (auto& byte : foreignStub)
    {
        byte ^= 1;
        Check(!NativeThreadStubMatches(foreignStub), "each changed system stub byte refuses the actual signature helper");
        byte ^= 1;
    }
    runtime.nextThread = reinterpret_cast<NextThread>(foreignStub.data());
    Check(!PrepareNativeThreadEnumeration(), "copied or altered non-image NtGetNextThread entry refuses before any hold");
    runtime.nextThread = savedEntry;
    runtime.nextThreadImage = savedImage;
    const auto savedComparison = runtime.compareThreadObjects;
    Check(Read(reinterpret_cast<std::uintptr_t>(savedComparison), foreignStub.data(), foreignStub.size()),
          "owned fixture reads only cached native comparison stub");
    Check(NativeThreadComparisonStubMatches(foreignStub), "all24 original native comparison bytes match the admitted profile");
    for (auto& byte : foreignStub)
    {
        byte ^= 1;
        Check(!NativeThreadComparisonStubMatches(foreignStub), "each changed comparison stub byte refuses the actual signature helper");
        byte ^= 1;
    }
    runtime.compareThreadObjects = reinterpret_cast<CompareThreadObjects>(foreignStub.data());
    Check(!PrepareNativeThreadEnumeration(), "copied non-image native comparison entry refuses before any hold");
    runtime.compareThreadObjects = savedComparison;
    Check(PrepareNativeThreadEnumeration(), "both exact system entries remain admissible after copied-entry refusal");
    ThreadHandles vmHandles;
    vmHandles.reserve(1024);
    MEMORY_BASIC_INFORMATION vmPage{};
    Check(VirtualQuery(vmHandles.data(), &vmPage, sizeof(vmPage)) == sizeof(vmPage) && vmPage.AllocationBase == vmHandles.data() && vmPage.Type == MEM_PRIVATE,
          "owned and retained handle buffers use direct VM allocations");
    const HANDLE lockStart = CreateEventW(nullptr, TRUE, FALSE, nullptr), lockReady = CreateEventW(nullptr, TRUE, FALSE, nullptr),
                 lockRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr), allocationStart = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(lockStart && lockReady && lockRelease && allocationStart, "VM transaction fixture events");
    std::atomic<bool> allocatorReady{false}, attempted{false}, completed{false}, peerLocked{false};
    std::thread       allocator(
        [&]
        {
            allocatorReady.store(true);
            WaitForSingleObject(allocationStart, INFINITE);
            attempted.store(true);
            void* allocation = HeapAlloc(GetProcessHeap(), 0, 1024 * 1024);
            completed.store(allocation != nullptr);
            if (allocation)
                HeapFree(GetProcessHeap(), 0, allocation);
        });
    std::thread peer(
        [&]
        {
            WaitForSingleObject(lockStart, INFINITE);
            peerLocked.store(HeapLock(GetProcessHeap()) != FALSE);
            SetEvent(lockReady);
            WaitForSingleObject(lockRelease, INFINITE);
            if (peerLocked.load())
                HeapUnlock(GetProcessHeap());
        });
    while (!allocatorReady.load())
        Sleep(1);
    SetEvent(lockStart);
    WaitForSingleObject(lockReady, INFINITE);
    Check(peerLocked.load() && SuspendThread(peer.native_handle()) == 0, "owned peer is held while it owns backend heap lock");
    SetEvent(allocationStart);
    while (!attempted.load())
        Sleep(1);
    Sleep(10);
    Check(!completed.load(), "backend-heap allocation is demonstrably blocked; no production guard relies on HeapLock");
    PVOID original = reinterpret_cast<PVOID>(&Original);
    Check(DetourTransactionBegin() == NO_ERROR && DetourUpdateThreadExternallySuspended(peer.native_handle()) == NO_ERROR &&
              DetourAttach(&original, reinterpret_cast<PVOID>(&HookPresent)) == NO_ERROR && DetourTransactionCommit() == NO_ERROR,
          "actual VM vendor allocation and commit complete while CRT heap allocation is blocked");
    Check(SuspendThread(peer.native_handle()) == 1 && ResumeThread(peer.native_handle()) == 2,
          "borrowed enrollment and Commit leave exactly caller's sole suspend count");
    Check(DetourTransactionBegin() == NO_ERROR && DetourUpdateThreadExternallySuspended(peer.native_handle()) == NO_ERROR &&
              DetourDetach(&original, reinterpret_cast<PVOID>(&HookPresent)) == NO_ERROR && DetourTransactionCommit() == NO_ERROR,
          "actual VM detach commit completes under stopped heap owner");
    Check(DetourTransactionBegin() == NO_ERROR && DetourUpdateThreadExternallySuspended(peer.native_handle()) == NO_ERROR &&
              DetourAttach(&original, reinterpret_cast<PVOID>(&HookPresent)) == NO_ERROR && DetourTransactionAbort() == NO_ERROR &&
              original == reinterpret_cast<PVOID>(&Original),
          "actual VM abort frees nodes without CRT dependency");
    Check(SuspendThread(peer.native_handle()) == 1 && ResumeThread(peer.native_handle()) == 2 && !completed.load(),
          "borrowed Abort also preserves one own count while unrelated CRT allocation stays blocked");
    Check(!ResumeOwnedThread(peer.native_handle(), &RefuseThreadResume) && SuspendThread(peer.native_handle()) == 1 && ResumeThread(peer.native_handle()) == 2,
          "forced own resume failure leaves live peer and exact own count available for retry");
    SetEvent(lockRelease);
    Check(ResumeOwnedThread(peer.native_handle()), "same owner retries sole suspension after vendor completion");
    peer.join();
    allocator.join();
    Check(completed.load(), "blocked CRT allocation completes after held owner resumes and releases heap");
    CloseHandle(lockStart);
    CloseHandle(lockReady);
    CloseHandle(lockRelease);
    CloseHandle(allocationStart);
    return true;
}
int main(int argc, char** argv)
{
    Check(argc == 4, "both route and complete presenter signature fixture paths supplied");
    CheckCodeFixture(argv[1], d3d11Route);
    CheckCodeFixture(argv[2], d3d12Route);
    CheckCustomFixture(argv[3]);
    NativeRenderObservation owner;
    Check(!owner.Begin(), "ordinary executable rejected");
    NativeRenderObservationState state;
    owner.ReadState(state);
    Check(!state.bound && !state.active && !state.modulePinned && !runtime.hooked.load() && !runtime.original,
          "wrong image installs no hook and pins no module");
    Check(state.failureReason == static_cast<unsigned>(Failure::UnsupportedBuild), "wrong image has precise failure");
    Check(state.requestedRoute == 1 && state.observedRoute == 0 && state.presenceKnownMask == 0, "default API1 route remains D3D11, unobserved until callback");
    Check(owner.End() && owner.End(), "unbound End idempotent");
    Check(!owner.Begin(RenderObservationRoute::Unknown), "unknown route is never guessed");
    owner.ReadState(state);
    Check(state.failureReason == static_cast<unsigned>(Failure::UnsupportedRoute) && state.requestedRoute == 0 && !state.modulePinned && !runtime.hooked.load(),
          "unknown route rejected before image pin or hooks");
    Check(owner.End(), "unknown route cleanup");
    Check(!owner.Begin(static_cast<RenderObservationRoute>(3)), "out of domain route rejected");
    Check(owner.End(), "out of domain route cleanup");
    Check(!owner.Begin(RenderObservationRoute::D3D12), "ordinary executable rejects D3D12 without relaxed guard");
    owner.ReadState(state);
    Check(state.failureReason == static_cast<unsigned>(Failure::UnsupportedBuild) && state.requestedRoute == 2 && state.observedRoute == 0 &&
              !state.modulePinned && !runtime.hooked.load(),
          "explicit D3D12 wrong-image rejection remains unobserved and unpinned");
    Check(owner.End(), "wrong-image D3D12 cleanup");
    Check(!owner.Begin(RenderObservationRoute::D3D12, true), "API3 still rejects an ordinary executable before any COM query");
    owner.ReadState(state);
    Check(!state.bound && !state.modulePinned && !runtime.hooked.load() && state.gpu.attempts == 0 && state.gpu.metadataValid == 0,
          "wrong-image API3 cannot publish borrowed GPU ownership or install a hook");
    Check(owner.End(), "wrong-image API3 cleanup");
    Check(!owner.Begin(RenderObservationRoute::D3D12, true, true), "API4 rejects a nongame image before reading presenter objects");
    owner.ReadState(state);
    Check(!state.bound && !state.modulePinned && !runtime.hooked.load() && !state.custom.status && !state.custom.implementationId &&
              !state.custom.slotSignatureMask && !state.custom.staticPolicyFlags && !state.gpu.attempts,
          "failed API4 image admission cannot fabricate static policy or COM metadata");
    Check(owner.End(), "wrong-image API4 cleanup");
    Check(!owner.Begin(RenderObservationRoute::D3D12, true, true, true), "API5 rejects a nongame image before endpoint or lifecycle preparation");
    owner.ReadState(state);
    Check(!state.bound && !state.modulePinned && !runtime.hooked.load() && !state.gpu.attempts && !state.boundary.status && !state.boundary.endpointProfileId &&
              !state.boundary.presentEntries && !state.boundary.resizeEpoch,
          "wrong-image API5 leaves the additive initial tail zero");
    Check(owner.End(), "wrong-image API5 cleanup");
    RenderEndpointDiagnosticV1 unavailableDiagnostic;
    unavailableDiagnostic.version = 99;
    Check(!owner.ReadEndpointDiagnostic(unavailableDiagnostic) && unavailableDiagnostic.version == 99,
          "wrong-image API5 has no separate endpoint diagnostic and preserves output");
    RenderSetupDiagnosticV1 setupDiagnostic;
    setupDiagnostic.version = 99;
    Check(!owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.version == 99,
          "pre-preparation wrong-image lease has no setup cache and preserves output");

    const auto image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x5770000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(image != 0, "owned synthetic image allocation");
    Configure(image, NativeDispatchFixtureOriginal, RenderObservationRoute::D3D12);
    EnableOwnership(image, 2, 1);
    runtime.classifyCustomPresent = runtime.queryFrameBoundary = runtime.nativeDispatch = true;
    PrimeNativeDispatchDiagnostic(runtime.generation.load());
    InitializeSetupDiagnostic(runtime.generation.load());
    runtime.setupTracing = false;
    Check(owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.version == 1 && setupDiagnostic.attempted == 1 &&
              setupDiagnostic.stage == static_cast<unsigned>(RenderSetupStage::PrepareFrameBoundary),
          "exact API6 producer reads its separate generation-owned setup cache");
    {
        ApiOperation            busyOperation;
        RenderSetupDiagnosticV1 preserved;
        std::memset(&preserved, 0xA5, sizeof(preserved));
        const auto expected = preserved;
        Check(busyOperation.acquired && !owner.ReadSetupDiagnostic(preserved) && std::memcmp(&preserved, &expected, sizeof(preserved)) == 0,
              "busy generation operation refuses the setup getter without altering any output field");
    }
    bool        foreignSetupRejected = false;
    std::thread foreignSetupReader(
        [&]
        {
            RenderSetupDiagnosticV1 preserved;
            preserved.version = 99;
            foreignSetupRejected = !owner.ReadSetupDiagnostic(preserved) && preserved.version == 99;
        });
    foreignSetupReader.join();
    Check(foreignSetupRejected, "foreign setup reader cannot access worker-owned scalars or alter its output");
    Check(HookPresent(viewport.data(), 1) && !HookPresent(viewport.data(), 0), "API6 native dispatch preserves forwarded native original results");
    owner.ReadState(state);
    Check(state.nativeDispatch.endpointProfileId == 2 && state.nativeDispatch.status == 2 && state.nativeDispatch.presentEntries == 2 &&
              state.nativeDispatch.factsMask == 255 && state.boundary.endpointProfileId == 0 && !state.boundary.presentEntries && !state.gpu.supportPredicate,
          "API6 associations accumulate separately without promoting API5 standard entry or GPU permission");
    HookPresent(viewport.data(), 4);
    owner.ReadState(state);
    Check(state.nativeDispatch.status == 10 && state.nativeDispatch.endpointProfileId == 2 && state.nativeDispatch.presentEntries == 2 &&
              !state.nativeDispatch.factsMask && state.acceptedSnapshots == 3,
          "a later absent dispatch keeps cumulative completed evidence and clears current association facts");
    AcquireSRWLockShared(&runtime.snapshotLock);
    HookPresent(viewport.data(), 1);
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(state.nativeDispatch.presentEntries == 2 && state.nativeDispatch.status == 10 && state.acceptedSnapshots == 3 && state.publicationMisses == 1,
          "missed API6 publication cannot overwrite either tail or completed dispatch counters");
    RenderEndpointDiagnosticV1 nativeEndpointDiagnostic;
    Check(owner.ReadEndpointDiagnostic(nativeEndpointDiagnostic) && nativeEndpointDiagnostic.acceptedSnapshotsLow == 3,
          "API6 producer retains the separate generation-coherent endpoint diagnostic");
    AcquireSRWLockExclusive(&runtime.snapshotLock);
    runtime.retainedSuspensions.push_back(nullptr);
    runtime.accepted.store(4);
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 3 && state.nativeDispatch.presentEntries == 2,
          "held snapshot lock returns paired cached evidence without a new accepted count");
    unavailableDiagnostic.version = 99;
    Check(!owner.ReadEndpointDiagnostic(unavailableDiagnostic) && unavailableDiagnostic.version == 99,
          "retained suspension refuses optional diagnostic untouched before either callback-shared lock");
    setupDiagnostic.version = 99;
    Check(!owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.version == 99,
          "retained suspension refuses setup getter untouched without acquiring the held snapshot lock");
    runtime.inFlight.store(1);
    Check(!owner.Begin(RenderObservationRoute::D3D12, true, true, true, true), "held callback readback rejects a new generation without waiting for its lock");
    runtime.inFlight.store(0);
    runtime.retainedSuspensions.clear();
    runtime.accepted.store(3);
    ReleaseSRWLockExclusive(&runtime.snapshotLock);
    runtime.safetyHeldSuspensions = true;
    Check(!owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.version == 99,
          "safety-held state alone also refuses setup getter without output changes");
    runtime.safetyHeldSuspensions = false;
    runtime.setupTracing = true;
    SetupRefused(RenderSetupStage::NativeThreadStubMatch);
    SetupRefused(RenderSetupStage::TransactionPeers, RenderSetupErrorDomain::Detours, 123);
    FinishSetupChange(false, true);
    runtime.inFlight.store(1);
    Check(!owner.End(), "API6 native wrapper in flight retains its owner");
    Check(owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.failureStage == static_cast<unsigned>(RenderSetupStage::NativeThreadStubMatch) &&
              !setupDiagnostic.errorDomain && !setupDiagnostic.errorCode && setupDiagnostic.cleanupAttempted == 1 && !setupDiagnostic.cleanupComplete,
          "busy End preserves first refusal/error while marking incomplete cleanup");
    runtime.inFlight.store(0);
    Check(owner.End(), "API6 fixture drains without a frame sink or GPU command");
    Check(owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.stage == static_cast<unsigned>(RenderSetupStage::NativeThreadStubMatch) &&
              setupDiagnostic.failureStage == setupDiagnostic.stage && setupDiagnostic.cleanupAttempted == 1 && setupDiagnostic.cleanupComplete == 1 &&
              !setupDiagnostic.errorDomain && !setupDiagnostic.errorCode && !setupDiagnostic.installSucceeded,
          "successful End keeps first setup refusal readable and changes only cleanup facts");
    const HANDLE lockedReady = CreateEventW(nullptr, TRUE, FALSE, nullptr), lockedRelease = CreateEventW(nullptr, TRUE, FALSE, nullptr),
                 lockedDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(lockedReady && lockedRelease && lockedDone, "counted held-callback retry events");
    std::thread lockedCallback(
        [&]
        {
            runtime.inFlight.store(1);
            AcquireSRWLockExclusive(&runtime.snapshotLock);
            SetEvent(lockedReady);
            WaitForSingleObject(lockedRelease, INFINITE);
            ReleaseSRWLockExclusive(&runtime.snapshotLock);
            runtime.inFlight.store(0);
            SetEvent(lockedDone);
        });
    WaitForSingleObject(lockedReady, INFINITE);
    HANDLE retainedCallback = nullptr;
    Check(SuspendThread(lockedCallback.native_handle()) == 0 &&
              DuplicateHandle(GetCurrentProcess(), lockedCallback.native_handle(), GetCurrentProcess(), &retainedCallback, 0, FALSE, DUPLICATE_SAME_ACCESS),
          "owned failed-resume model retains the counted callback's actual suspension and handle");
    Check(!ResumeOwnedThread(retainedCallback, &RefuseThreadResume), "own-resume helper preserves failed counted callback for public End retry");
    runtime.retainedSuspensions.push_back(retainedCallback);
    SetEvent(lockedRelease);
    owner.End();
    Check(WaitForSingleObject(lockedDone, 1000) == WAIT_OBJECT_0 && !runtime.inFlight.load(),
          "End retries a retained counted callback's resume before requiring its in-flight count or snapshot lock to drain");
    lockedCallback.join();
    Check(owner.End(), "a drained counted-callback retry remains cleanup-idempotent");
    CloseHandle(lockedReady);
    CloseHandle(lockedRelease);
    CloseHandle(lockedDone);
    ResetFrameBoundaryObservation();
    Configure(image);
    const auto originalViewport = viewport;
    const auto originalRhi = rhi;
    Check(HookPresent(viewport.data(), 2), "native bool result preserved");
    owner.ReadState(state);
    Check(state.nativePresentCallbacks == 1 && state.acceptedSnapshots == 1 && state.rejectedSnapshots == 0, "owned callback publishes scalar evidence");
    Check(state.width == 1920 && state.height == 1080 && state.syncInterval == 2 && state.callbackThreadId == GetCurrentThreadId(),
          "native dimensions and callback thread");
    Check(state.windowPresent && state.swapchainPresent && state.devicePresent && state.contextPresent, "native member presence observed without COM");
    Check(state.requestedRoute == 1 && state.observedRoute == 1 && state.presenceKnownMask == 15 && !state.queuePresent,
          "D3D11 published route and known fields");
    Check(state.lastOriginalReturned && state.lastOriginalResult && state.failureReason == 0, "completed original evidence");
    Check(viewport == originalViewport && rhi == originalRhi, "observation never writes native objects");
    Check(!HookPresent(viewport.data(), 0), "false native bool preserved without trial failure");
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 2 && state.lastOriginalReturned && !state.lastOriginalResult && state.failureReason == 0,
          "false original is valid observation");

    Configure(image, Original, RenderObservationRoute::D3D12);
    owner.ReadState(state);
    Check(state.requestedRoute == 2 && state.observedRoute == 0 && state.presenceKnownMask == 0 && !state.queuePresent,
          "zero D3D12 callbacks never fabricate observed route or presence");
    const auto d3d12Viewport = viewport;
    const auto d3d12Owner = rhi;
    const auto d3d12Device = deviceObject, d3d12Manager = queueManager;
    Check(HookPresent(viewport.data(), 2), "D3D12 original result preserved");
    owner.ReadState(state);
    Check(state.requestedRoute == 2 && state.observedRoute == 2 && state.presenceKnownMask == 19 && state.queuePresent == 1,
          "D3D12 returned callback records queue separately from unknown device/context");
    Check(state.windowPresent && state.swapchainPresent && !state.devicePresent && !state.contextPresent && state.width == 1920 && state.height == 1080,
          "D3D12 constructor-pinned scalar and swapchain fields");
    Check(viewport == d3d12Viewport && rhi == d3d12Owner && deviceObject == d3d12Device && queueManager == d3d12Manager,
          "D3D12 observation never writes or invokes native objects/interfaces");
    Check(!HookPresent(viewport.data(), 0), "false D3D12 original remains valid");
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 2 && state.lastOriginalReturned && !state.lastOriginalResult && state.failureReason == 0,
          "D3D12 native boolfalse does not imply COM or failure");
    Check(state.gpu.attempts == 0 && state.gpu.queryStage == 0 && state.gpu.queryStatus == 0,
          "API2 with deliberately unreadable borrowed interfaces never calls COM or fabricates API3 metadata");

    OwnedUnknown ownedSwapchain, ownedQueue;
    const auto   object = reinterpret_cast<std::uintptr_t>(viewport.data());
    const auto   queueMember = reinterpret_cast<std::uintptr_t>(queueManager.data()) + 0x28;
    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image, 2, 1);
    Check(HookPresent(viewport.data(), 1), "API3 multi-GPU refusal still forwards original");
    owner.ReadState(state);
    Check(state.gpu.attempts == 1 && state.gpu.accepted == 0 && state.gpu.rejected == 1 && state.gpu.metadataValid == 0 &&
              state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::SingleNode) &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::Unsupported),
          "native multi-GPU facts publish explicit unsupported metadata without touching unreadable COM roots");
    constexpr std::array<std::uint32_t, 4> explicitCounts{0, 1, 1, 2}, afrCounts{1, 0, 2, 1};
    for (std::size_t index = 0; index < explicitCounts.size(); ++index)
    {
        Configure(image, Original, RenderObservationRoute::D3D12);
        EnableOwnership(image, explicitCounts[index], afrCounts[index]);
        Put(object + 0x1d0, std::uintptr_t{0x1234});
        Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
        Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
        Check(!HookPresent(viewport.data(), 0), "custom multi-GPU refusal preserves false original result");
        owner.ReadState(state);
        Check(!ownedSwapchain.calls && !ownedQueue.calls && state.gpu.attempts == 1 && state.gpu.rejected == 1 && state.gpu.customPresentPresent == 1 &&
                  state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::CustomPresent) &&
                  state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::Unsupported) && !state.gpu.metadataValid &&
                  !state.gpu.supportPredicate && state.failureReason == 0,
              "custom presenter with non-single native counts retains stage-two refusal without COM");
    }
    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    // An unreadable custom receiver proves that diagnostics query only the
    // native swapchain/queue roots, never the presenter's unknown interface.
    Put(object + 0x1d0, std::uintptr_t{0x1234});
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    const auto customViewport = viewport;
    const auto customDevice = deviceObject, customManager = queueManager;
    Check(!HookPresent(viewport.data(), 0), "custom single-GPU metadata query preserves false original result");
    owner.ReadState(state);
    Check(ownedSwapchain.calls == 1 && !ownedQueue.calls && state.gpu.attempts == 1 && state.gpu.rejected == 1 && state.gpu.customPresentPresent == 1 &&
              state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::SwapchainQuery) &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::Failed) &&
              state.gpu.hresult == static_cast<std::uint32_t>(E_NOINTERFACE) && !state.gpu.metadataValid && !state.gpu.supportPredicate &&
              state.failureReason == 0,
          "custom single-GPU route queries the existing native swapchain without claiming presenter ownership");
    Check(viewport == customViewport && deviceObject == customDevice && queueManager == customManager,
          "custom presenter metadata queries do not replace native roots or invoke the presenter");
    ownedSwapchain.succeed = true;
    Check(!HookPresent(viewport.data(), 0), "custom metadata queue query preserves false original result");
    owner.ReadState(state);
    Check(ownedSwapchain.calls == 2 && ownedQueue.calls == 1 && state.gpu.attempts == 2 && state.gpu.rejected == 2 && state.gpu.customPresentPresent == 1 &&
              state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::QueueQuery) &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::Failed) &&
              state.gpu.hresult == static_cast<std::uint32_t>(E_NOINTERFACE) && !state.gpu.metadataValid && !state.gpu.supportPredicate &&
              state.failureReason == 0,
          "custom single-GPU route queries the creation-chain queue after acquiring the native swapchain");
    ownedSwapchain.succeed = false;
    ownedSwapchain.calls = 0;
    ownedQueue.calls = 0;
    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    runtime.classifyCustomPresent = true;
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    Check(HookPresent(viewport.data(), 1), "API4 absent classification forwards the original even when native QI fails");
    owner.ReadState(state);
    Check(state.custom.status == static_cast<unsigned>(CustomPresentClassificationStatus::Absent) && !state.custom.implementationId &&
              !state.custom.slotSignatureMask && !state.custom.staticPolicyFlags && !state.gpu.customPresentPresent && state.gpu.attempts == 1,
          "absent API4 classification is coherent with the same published native snapshot");
    std::array<std::uintptr_t, 4> ownedPresenter{1, 0, 0, reinterpret_cast<std::uintptr_t>(deviceObject.data())};
    const auto                    configureClassification = [&](PresentFunction original = Original)
    {
        Configure(image, original, RenderObservationRoute::D3D12);
        EnableOwnership(image);
        runtime.classifyCustomPresent = true;
        Put(object + 0x1d0, reinterpret_cast<std::uintptr_t>(ownedPresenter.data()));
        Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
        Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    };
    configureClassification();
    Check(HookPresent(viewport.data(), 1), "unknown API4 presenter is not invoked by diagnostics");
    owner.ReadState(state);
    Check(state.custom.status == static_cast<unsigned>(CustomPresentClassificationStatus::UnknownImplementation) && !state.custom.implementationId &&
              !state.custom.slotSignatureMask && !state.custom.staticPolicyFlags && state.gpu.customPresentPresent && !state.gpu.supportPredicate,
          "an unknown presenter retains unsupported GPU admission and publishes no guessed policy");
    ownedPresenter[0] = image + CustomPresentTableRva;
    configureClassification();
    Check(HookPresent(viewport.data(), 1), "private fixture pages do not impersonate the admitted executable image");
    owner.ReadState(state);
    Check(state.custom.status == static_cast<unsigned>(CustomPresentClassificationStatus::ReadFault) && !state.custom.implementationId &&
              !state.custom.staticPolicyFlags && !state.custom.slotSignatureMask,
          "production image reader rejects a known numeric table address backed by private memory");
    ownedPresenter[0] = 1;
    configureClassification();
    ownedSwapchain.changeAddress = reinterpret_cast<std::uintptr_t>(ownedPresenter.data());
    Check(HookPresent(viewport.data(), 1), "presenter-table mutation during QI still forwards the original");
    owner.ReadState(state);
    Check(state.custom.status == static_cast<unsigned>(CustomPresentClassificationStatus::RootsChanged) && !state.custom.implementationId &&
              !state.custom.slotSignatureMask && !state.custom.staticPolicyFlags &&
              state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::SwapchainQuery),
          "API4 table readback rejects changed policy without replacing independent GPU query provenance");
    ownedSwapchain.changeAddress = 0;
    ownedPresenter[0] = 1;
    configureClassification(OriginalException);
    Check(CatchOriginalException(viewport.data()), "API4 original SEH propagates after classification and metadata reads");
    owner.ReadState(state);
    Check(!state.custom.status && !state.gpu.attempts && !state.acceptedSnapshots && !runtime.inFlight.load(),
          "API4 cannot publish classification for an original that never returned");
    configureClassification();
    AcquireSRWLockShared(&runtime.snapshotLock);
    std::thread missedClassification([&] { HookPresent(viewport.data(), 1); });
    missedClassification.join();
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(state.publicationMisses == 1 && !state.custom.status && !state.gpu.attempts && !state.acceptedSnapshots,
          "API4 missed publication cannot mix a new class policy with old GPU counters");
    configureClassification();
    DWORD priorProtection = 0, restoredProtection = 0;
    Check(VirtualProtect(reinterpret_cast<void*>(image + 0x503e000), 4096, PAGE_NOACCESS, &priorProtection) != FALSE,
          "owned native-count page can model a failed ownership root read");
    Check(HookPresent(viewport.data(), 1), "API4 native root read fault preserves original execution");
    Check(VirtualProtect(reinterpret_cast<void*>(image + 0x503e000), 4096, priorProtection, &restoredProtection) != FALSE,
          "owned native-count page protection restores after fault fixture");
    owner.ReadState(state);
    Check(state.custom.status == static_cast<unsigned>(CustomPresentClassificationStatus::ReadFault) && !state.gpu.customPresentPresent &&
              state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::NativeRoots) && !state.custom.implementationId &&
              !state.custom.slotSignatureMask && !state.custom.staticPolicyFlags,
          "API4 root read failure reports unknown custom presence without inventing a policy");
    ownedSwapchain.calls = ownedQueue.calls = 0;

    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    Check(HookPresent(viewport.data(), 1), "invalid API3 COM root never prevents original forwarding");
    owner.ReadState(state);
    Check(state.gpu.attempts == 1 && state.gpu.rejected == 1 && state.gpu.hresult == static_cast<std::uint32_t>(E_POINTER) &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::Failed) && state.gpu.metadataValid == 0,
          "unreadable API3 roots fail closed before invoking a COM vtable");
    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    Put(object + 0x1d0, std::uintptr_t{0x1234});
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    ownedSwapchain.changeAddress = object + 0x1d0;
    Check(HookPresent(viewport.data(), 1), "owned QI failure/root mutation still preserves original");
    owner.ReadState(state);
    Check(ownedSwapchain.calls == 1 && ownedQueue.calls == 0 && state.gpu.queryStage == static_cast<unsigned>(GpuOwnershipQueryStage::RootReadback) &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::RootsChanged) && state.gpu.metadataValid == 0 &&
              state.gpu.supportPredicate == 0 && state.gpu.attempts == 1 && state.gpu.rejected == 1,
          "custom presenter root readback rejects changed ownership and publishes no stale positive metadata");
    ownedSwapchain.changeAddress = 0;
    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    Put(object + 0x1d0, std::uintptr_t{0x1234});
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    Check(HookPresent(viewport.data(), 1), "owned failing QI executes only on adopted callback thread");
    const auto  beforeForeign = ownedSwapchain.calls;
    std::thread foreignProbe([&] { HookPresent(viewport.data(), 1); });
    foreignProbe.join();
    owner.ReadState(state);
    Check(ownedSwapchain.calls == beforeForeign && state.foreignThreadCallbacks == 1 && state.gpu.attempts == 1 && state.gpu.rejected == 1,
          "foreign native callbacks forward without borrowing or querying another thread's GPU roots");
    Configure(image, OriginalException, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    Put(object + 0x1d0, std::uintptr_t{0x1234});
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    const auto beforeException = ownedSwapchain.calls;
    Check(CatchOriginalException(viewport.data()), "API3 original SEH still propagates after custom-present metadata queries");
    owner.ReadState(state);
    Check(ownedSwapchain.calls == beforeException + 1 && state.gpu.attempts == 0 && state.gpu.queryStage == 0 && state.acceptedSnapshots == 0 &&
              runtime.inFlight.load() == 0,
          "custom-present queries cannot publish completed probe evidence when original never returns");
    Configure(image, Original, RenderObservationRoute::D3D12);
    EnableOwnership(image);
    Put(object + 0x1d0, std::uintptr_t{0x1234});
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    const auto beforeMiss = ownedSwapchain.calls;
    AcquireSRWLockShared(&runtime.snapshotLock);
    std::thread missedProbe([&] { HookPresent(viewport.data(), 1); });
    missedProbe.join();
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(ownedSwapchain.calls == beforeMiss + 1 && state.publicationMisses == 1 && state.gpu.attempts == 0 && state.gpu.queryStage == 0 &&
              state.acceptedSnapshots == 0,
          "custom-present missed publication retains one coherent old snapshot without blocking the engine");
    const auto configureCleanupFailure = [&](PresentFunction original)
    {
        Configure(image, original, RenderObservationRoute::D3D12);
        EnableOwnership(image);
        runtime.classifyCustomPresent = true;
        Put(object + 0x1d0, std::uintptr_t{0x1234});
        Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
        Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
        ownedSwapchain.succeed = true;
        ownedSwapchain.failRelease = true;
    };
    configureCleanupFailure(Original);
    Check(HookPresent(viewport.data(), 1), "callback forwards original after an owned COM release fault");
    owner.ReadState(state);
    Check(!state.active && state.failureReason == static_cast<unsigned>(Failure::GpuOwnershipCleanup) && state.gpu.attempts == 1 &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::CleanupFailed) && state.gpu.metadataValid == 0,
          "COM cleanup fault permanently disarms and publishes failure without treating the callback as clean");
    auto callsAfterCleanup = ownedSwapchain.calls;
    Check(HookPresent(viewport.data(), 1) && ownedSwapchain.calls == callsAfterCleanup,
          "disarmed ownership probe never re-queries after a COM cleanup failure");
    configureCleanupFailure(Original);
    AcquireSRWLockShared(&runtime.snapshotLock);
    std::thread missedCleanup([&] { HookPresent(viewport.data(), 1); });
    missedCleanup.join();
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(!state.active && state.failureReason == static_cast<unsigned>(Failure::GpuOwnershipCleanup) && state.publicationMisses == 1 &&
              state.gpu.attempts == 0 && state.gpu.queryStage == 0,
          "cleanup failure survives missed metadata publication through the independent fatal latch");
    callsAfterCleanup = ownedSwapchain.calls;
    Check(HookPresent(viewport.data(), 1) && ownedSwapchain.calls == callsAfterCleanup, "missed failure publication cannot rearm COM queries");
    configureCleanupFailure(OriginalException);
    Check(CatchOriginalException(viewport.data()), "original SEH still propagates after a COM cleanup failure");
    owner.ReadState(state);
    Check(!state.active && state.failureReason == static_cast<unsigned>(Failure::GpuOwnershipCleanup) && state.gpu.attempts == 0 &&
              state.acceptedSnapshots == 0 && runtime.inFlight.load() == 0,
          "fatal COM cleanup failure remains visible when original never returns or publishes");
    callsAfterCleanup = ownedSwapchain.calls;
    runtime.original = Original;
    Check(HookPresent(viewport.data(), 1) && ownedSwapchain.calls == callsAfterCleanup, "original exception cannot erase a disarm or permit another COM query");
    configureCleanupFailure(Original);
    ownedSwapchain.changeAddress = object + 0x1d0;
    Check(HookPresent(viewport.data(), 1), "simultaneous ownership mutation and failed release still forward original");
    owner.ReadState(state);
    Check(!state.active && state.failureReason == static_cast<unsigned>(Failure::GpuOwnershipCleanup) &&
              state.gpu.queryStatus == static_cast<unsigned>(GpuOwnershipQueryStatus::CleanupFailed),
          "native root readback cannot erase a fatal COM cleanup failure");
    ownedSwapchain.changeAddress = 0;
    ownedSwapchain.succeed = ownedSwapchain.failRelease = false;
    const auto configureBoundaryPublication = [&]
    {
        Configure(image, BoundaryFixtureOriginal, RenderObservationRoute::D3D12);
        EnableOwnership(image, 2, 1);
        runtime.classifyCustomPresent = runtime.queryFrameBoundary = true;
        PrimeBoundaryDiagnostic(runtime.generation.load());
    };
    configureBoundaryPublication();
    Check(HookPresent(viewport.data(), 1) && !HookPresent(viewport.data(), 0), "API5 completed scopes preserve both native original results");
    owner.ReadState(state);
    Check(state.boundary.presentEntries == 2 && state.boundary.rejectedEntries == 0 && state.boundary.status == 2 && state.boundary.factsMask == 255 &&
              state.gpu.attempts == 2 && !state.gpu.supportPredicate,
          "API5 cumulative boundary values publish atomically with historical unsupported GPU metadata");
    RenderEndpointDiagnosticV1 endpointDiagnostic;
    Check(owner.ReadEndpointDiagnostic(endpointDiagnostic) && endpointDiagnostic.version == 1 && endpointDiagnostic.acceptedSnapshotsLow == 2 &&
              endpointDiagnostic.generationLow == static_cast<std::uint32_t>(state.generation),
          "owner reads separate scalar diagnostic tied to the coherently published native snapshot");
    bool        foreignDiagnosticRejected = false;
    std::thread foreignDiagnostic(
        [&]
        {
            RenderEndpointDiagnosticV1 preserved;
            preserved.version = 99;
            foreignDiagnosticRejected = !owner.ReadEndpointDiagnostic(preserved) && preserved.version == 99;
        });
    foreignDiagnostic.join();
    Check(foreignDiagnosticRejected, "endpoint diagnostic requires the native producer thread and preserves foreign output");
    AcquireSRWLockShared(&runtime.snapshotLock);
    HookPresent(viewport.data(), 1);
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(state.boundary.presentEntries == 2 && state.gpu.attempts == 2 && state.publicationMisses == 1,
          "publication miss cannot attach new boundary counters to an old snapshot");
    Check(owner.ReadEndpointDiagnostic(endpointDiagnostic) && endpointDiagnostic.acceptedSnapshotsLow == 2,
          "missed native publication cannot update the separate endpoint diagnostic cache");
    runtime.inFlight.store(1);
    Check(!owner.End() && owner.ReadEndpointDiagnostic(endpointDiagnostic) && endpointDiagnostic.acceptedSnapshotsLow == 2,
          "inflight failed End retains the readable diagnostic and module lifetime");
    runtime.inFlight.store(0);
    Check(owner.End() && owner.ReadEndpointDiagnostic(endpointDiagnostic) && endpointDiagnostic.acceptedSnapshotsLow == 2,
          "successful End preserves the final scalar diagnostic until lease release");
    runtime.active.store(true);
    EnableOwnership(image);
    Put(object + 0x1d0, std::uintptr_t{0x1234});
    Put(object + 0x60, reinterpret_cast<std::uintptr_t>(&ownedSwapchain));
    Put(queueMember, reinterpret_cast<std::uintptr_t>(&ownedQueue));
    ownedSwapchain.succeed = ownedSwapchain.failRelease = true;
    Check(HookPresent(viewport.data(), 1), "later fatal API3 cleanup still forwards native original");
    owner.ReadState(state);
    Check(!state.active && state.failureReason == 10 && state.gpu.attempts == 3 && state.boundary.presentEntries == 2 && state.boundary.status == 2,
          "later fatal API3 cleanup retains explicitly historical boundary cache without new API5 calls");
    configureCleanupFailure(Original);
    runtime.queryFrameBoundary = true;
    HookPresent(viewport.data(), 1);
    owner.ReadState(state);
    Check(state.failureReason == 10 && state.gpu.attempts == 1 && !state.boundary.status && !state.boundary.presentEntries,
          "first fatal API3 cleanup leaves API5 unattempted rather than inventing a boundary query");
    ownedSwapchain.succeed = ownedSwapchain.failRelease = false;
    configureBoundaryPublication();
    HookPresent(viewport.data(), 1);
    Check(CatchBoundaryException(viewport.data()), "boundary cleanup failure preserves original SEH propagation");
    owner.ReadState(state);
    Check(!state.active && state.failureReason == 10 && state.boundary.presentEntries == 1 && state.gpu.attempts == 1 && runtime.inFlight == 0,
          "fatal boundary cleanup survives original SEH while prior coherent snapshot remains intact");
    configureBoundaryPublication();
    HookPresent(viewport.data(), 1);
    AcquireSRWLockShared(&runtime.snapshotLock);
    HookPresent(viewport.data(), 3);
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(!state.active && state.failureReason == 10 && state.publicationMisses == 1 && state.boundary.presentEntries == 1 && state.gpu.attempts == 1,
          "fatal boundary cleanup disarm survives a missed coherent publication");
    for (const auto route : {RenderObservationRoute::D3D11, RenderObservationRoute::D3D12})
    {
        Configure(image, Original, route);
        NativeRenderObservationState preserved;
        preserved.width = 777;
        Put(reinterpret_cast<std::uintptr_t>(viewport.data()),
            image + (route == RenderObservationRoute::D3D11 ? d3d12Route.viewportTable : d3d11Route.viewportTable));
        Check(!Capture(viewport.data(), 1, preserved) && preserved.width == 777, "cross-route native object identity rejected without publishing");
        Check(HookPresent(viewport.data(), 1), "cross-route callback still forwards physical original");
        owner.ReadState(state);
        Check(state.rejectedSnapshots == 1 && !state.active && !state.observedRoute && !state.presenceKnownMask, "rejected route cannot fabricate observation");
    }
    Configure(image, Original, RenderObservationRoute::D3D12);
    Put(reinterpret_cast<std::uintptr_t>(queueManager.data()) + 0x28, std::uintptr_t{0});
    Check(HookPresent(viewport.data(), 1), "missing D3D12 queue still forwards original");
    owner.ReadState(state);
    Check(state.rejectedSnapshots == 1 && !state.active && !state.observedRoute && !state.queuePresent,
          "missing queue fails closed without claiming knowledge");
    Put(reinterpret_cast<std::uintptr_t>(queueManager.data()) + 0x28, std::uintptr_t{4});
    HookPresent(viewport.data(), 1);
    owner.ReadState(state);
    Check(state.acceptedSnapshots == 0, "repaired D3D12 context cannot silently rearm");
    Configure(image, OriginalException, RenderObservationRoute::D3D12);
    Check(CatchOriginalException(viewport.data()), "D3D12 original SEH propagates");
    owner.ReadState(state);
    Check(!state.observedRoute && !state.presenceKnownMask && !state.queuePresent && !state.lastOriginalReturned && runtime.inFlight.load() == 0,
          "D3D12 exception never publishes a returned route");
    Configure(image, OriginalNested, RenderObservationRoute::D3D12);
    HookPresent(viewport.data(), 1);
    owner.ReadState(state);
    Check(state.nestedCallbacks == 1 && !state.active && !state.observedRoute && !state.presenceKnownMask && runtime.inFlight.load() == 0,
          "nested D3D12 callback disarms and unwinds without publishing");
    Configure(image, Original, RenderObservationRoute::D3D12);
    std::thread d3d12Engine([] { HookPresent(viewport.data(), 1); });
    d3d12Engine.join();
    owner.ReadState(state);
    const auto d3d12Thread = state.callbackThreadId;
    Check(state.observedRoute == 2 && d3d12Thread != GetCurrentThreadId(), "D3D12 engine thread adopts only after validated capture");
    HookPresent(viewport.data(), 2);
    owner.ReadState(state);
    Check(state.foreignThreadCallbacks == 1 && state.acceptedSnapshots == 1 && state.callbackThreadId == d3d12Thread && state.active,
          "foreign D3D12 original forwards without replacing route evidence");
    Configure(image, Original, RenderObservationRoute::D3D12);
    AcquireSRWLockShared(&runtime.snapshotLock);
    std::thread d3d12Contended([] { HookPresent(viewport.data(), 1); });
    d3d12Contended.join();
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(state.publicationMisses == 1 && !state.observedRoute && !state.presenceKnownMask && state.active,
          "missed D3D12 publication never fabricates observed route or blocks native thread");

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

    for (const auto route : {RenderObservationRoute::D3D11, RenderObservationRoute::D3D12})
    {
        Configure(image, OriginalBlocked, route);
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
        Check(!state.active && state.pendingCleanup && state.failureReason == 0 && runtime.hooked.load(),
              "busy cleanup is retryable without permanent failure");
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
    }

    // Each round races the production producer read with a first publication
    // on one reused engine thread. Route/scalars must belong to the same
    // accepted-count sample even if the nonblocking publisher misses its lock.
    HANDLE publish = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE published = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Check(publish && published, "owned route-publication barriers");
    std::atomic<bool> stopPublication{false};
    std::thread       publisher(
        [&]
        {
            for (;;)
            {
                WaitForSingleObject(publish, 5000);
                if (stopPublication.load())
                    break;
                HookPresent(viewport.data(), 1);
                SetEvent(published);
            }
        });
    for (const auto route : {RenderObservationRoute::D3D11, RenderObservationRoute::D3D12})
    {
        unsigned publishedRounds = 0;
        for (unsigned round = 0; round < 512; ++round)
        {
            Configure(image, Original, route);
            SetEvent(publish);
            bool coherent = true;
            do
            {
                owner.ReadState(state);
                const bool observed = state.acceptedSnapshots != 0;
                coherent = coherent && state.observedRoute == (observed ? static_cast<unsigned>(route) : 0) && state.lastOriginalReturned == observed &&
                           state.width == (observed ? 1920u : 0u) &&
                           state.presenceKnownMask == (observed ? (route == RenderObservationRoute::D3D11 ? 15u : 19u) : 0u);
            } while (WaitForSingleObject(published, 0) != WAIT_OBJECT_0);
            Check(coherent, "first returned route and accepted count are one coherent snapshot");
            owner.ReadState(state);
            publishedRounds += state.acceptedSnapshots != 0;
        }
        Check(publishedRounds != 0, "publication stress exercised actual returned route snapshots");
    }
    for (unsigned round = 0; round < 64; ++round)
    {
        Configure(image, Original, RenderObservationRoute::D3D12);
        EnableOwnership(image, 2, 1);
        SetEvent(publish);
        bool coherent = true;
        do
        {
            owner.ReadState(state);
            const bool observed = state.acceptedSnapshots != 0;
            coherent = coherent && state.gpu.attempts == state.acceptedSnapshots && state.gpu.accepted == 0 && state.gpu.rejected == state.gpu.attempts &&
                       state.gpu.metadataValid == 0 && state.gpu.supportPredicate == 0 &&
                       state.gpu.queryStage == (observed ? static_cast<unsigned>(GpuOwnershipQueryStage::SingleNode) : 0u) &&
                       state.gpu.queryStatus == (observed ? static_cast<unsigned>(GpuOwnershipQueryStatus::Unsupported) : 0u) &&
                       state.gpu.nativeExplicitGPUCount == (observed ? 2u : 0u);
        } while (WaitForSingleObject(published, 0) != WAIT_OBJECT_0);
        Check(coherent, "API3 metadata and published probe counters are coherent across producer/engine threads");
    }
    stopPublication.store(true);
    SetEvent(publish);
    publisher.join();
    CloseHandle(publish);
    CloseHandle(published);

    Configure(image);
    HookPresent(viewport.data(), 1);
    Check(owner.End(), "direct callback cleanup");
    owner.ReadState(state);
    const auto oldGeneration = state.generation;
    runtime.nativeDispatch = true;
    InitializeSetupDiagnostic(oldGeneration);
    runtime.setup.transactionCommitted = runtime.setup.hooksCommitted = 1;
    FinishSetupChange(true, true);
    Check(owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.installSucceeded == 1,
          "owned getter fixture retains a completed previous-generation setup");
    NativeRenderObservation nextOwner;
    Check(!nextOwner.Begin(), "new owner still cannot admit wrong executable");
    unavailableDiagnostic.version = 99;
    Check(!owner.ReadEndpointDiagnostic(unavailableDiagnostic) && unavailableDiagnostic.version == 99,
          "retired observation cannot read a different generation's private diagnostic");
    setupDiagnostic.version = 99;
    Check(!owner.ReadSetupDiagnostic(setupDiagnostic) && setupDiagnostic.version == 99 && !nextOwner.ReadSetupDiagnostic(setupDiagnostic) &&
              setupDiagnostic.version == 99 && !runtime.setup.version,
          "new rejected Begin clears historical setup and neither old nor wrong-image lease exposes it");
    NativeRenderObservationState retired;
    owner.ReadState(retired);
    Check(retired.generation == oldGeneration && retired.acceptedSnapshots == state.acceptedSnapshots && retired.width == state.width,
          "retired owner preserves own final evidence across generations");
    Check(nextOwner.End(), "failed next generation cleanup");
    Check(!runtime.modulePinned.load(), "synthetic fixtures never pin executable/module");
    CheckFrameBoundaryProbe(argv[3], argv[2], checks);
    VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE);
    std::printf("Passed %u production native render observation checks\n", checks);
    return 0;
}
#endif
