/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Focused proof of the actual present owner and diagnostic retirement
 *****************************************************************************/

#include <windows.h>
#include "../../Client/game_de/NativeFrameBoundaryProbe.h"
namespace mta::game_de
{
    bool FixtureReleaseNativeDispatchRelay() noexcept;
}
// Only the fixture can refuse this cleanup edge. Its ordinary path calls the
// real probe function; production receives no bypass or fault-control API.
#define ReleaseNativeDispatchRelay FixtureReleaseNativeDispatchRelay
#include "../../Client/game_de/NativePresentOwner.cpp"
#undef ReleaseNativeDispatchRelay
#include "../../Client/game_de/NativeRenderObservation.cpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <unknwn.h>

using namespace mta::game_de;
namespace Manager = mta::game_de::PresentOwnerDetail;
namespace Service = mta::game_de::NativeServiceAdmission;
namespace Present = mta::game_de::NativePresentOwner;
static_assert(sizeof(RenderDeviceLeaseV1) == 48 && sizeof(RenderDeviceWindowLeaseV2) == 64, "device capture wire layouts stay unchanged");

namespace
{
    unsigned                         checks = 0;
    std::atomic<unsigned>            originalCalls{0};
    std::atomic<std::uintptr_t>      receivedViewport{0};
    std::atomic<std::uint32_t>       receivedInterval{0};
    HANDLE                           originalEntered = nullptr, originalResume = nullptr;
    bool                             refuseRelay = false;
    unsigned                         relayRefusals = 0;
    std::array<unsigned char, 0x1e0> viewport{};
    std::array<unsigned char, 0x900> rhi{};

    void Check(bool value, const char* text)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks, text);
            std::exit(1);
        }
    }
    template <typename T>
    void Put(std::uintptr_t address, T value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }
    void Record(void* object, std::uint32_t interval)
    {
        receivedViewport.store(reinterpret_cast<std::uintptr_t>(object));
        receivedInterval.store(interval);
        originalCalls.fetch_add(1);
    }
    __declspec(noinline) bool Original(void* object, std::uint32_t interval)
    {
        Record(object, interval);
        return interval != 0;
    }
    __declspec(noinline) bool OriginalBlocked(void* object, std::uint32_t interval)
    {
        Record(object, interval);
        SetEvent(originalEntered);
        WaitForSingleObject(originalResume, 5000);
        return interval != 0;
    }
    bool OriginalException(void* object, std::uint32_t interval)
    {
        Record(object, interval);
        RaiseException(0xe0424242, 0, 0, nullptr);
        return false;
    }
    bool OriginalNested(void* object, std::uint32_t interval)
    {
        Record(object, interval);
        if (Manager::callbackDepth == 1)
            Manager::HookPresent(object, interval);
        return true;
    }
    bool CatchOriginal()
    {
        __try
        {
            Manager::HookPresent(viewport.data(), 0xf1234567u);
        }
        __except (GetExceptionCode() == 0xe0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            return true;
        }
        return false;
    }
    void ClockBlocked(const char* text)
    {
        Service::Token token;
        Check(!Service::TryAcquire(Service::Category::Clock, token), text);
    }
    void ClockAllowed(const char* text)
    {
        Service::Token token;
        Check(Service::TryAcquire(Service::Category::Clock, token), text);
        Check(Service::Release(token), "actual Clock token released after its check");
    }
    void Configure(NativeRenderObservation& observer, std::uintptr_t image, Present::PresentFunction original = &Original)
    {
        Check(!Manager::runtime.hooked.load() && !Manager::runtime.inFlight.load() && Manager::runtime.retainedSuspensions.empty() &&
                  !Manager::runtime.safetyHeldSuspensions && !Manager::runtime.admissionSerial.load() && !runtime.devicePending.load() &&
                  !runtime.deviceReleaseFault.load(),
              "independent case begins with retired real owner state");
        // The public shipping admission still refuses the ordinary test image.
        // It establishes the observer's real generation; only this private TU
        // subsequently supplies owned memory/function fixtures, as the earlier
        // render fixture did. No copied native code is invoked.
        Check(!observer.Begin(), "shipping Begin keeps exact executable admission");
        NativeRenderObservationState rejected;
        observer.ReadState(rejected);
        Check(rejected.failureReason == static_cast<unsigned>(Failure::UnsupportedBuild) && !rejected.bound && !Manager::runtime.hooked.load(),
              "wrong image never supplies a shipping hook lease");
        Service::Token render;
        Check(Service::TryAcquire(Service::Category::Render, render), "owned fixture acquires the real Render exclusion token");
        const auto generation = Present::CurrentGeneration();
        Check(generation != 0, "real generation was allocated before the image refusal");
        Manager::runtime.admissionGeneration.store(generation);
        Manager::runtime.admissionSerial.store(render.serial);
        Manager::runtime.image = image;
        Manager::runtime.original = original;
        Manager::runtime.route = Manager::SelectRoute(RenderObservationRoute::D3D11);
        Manager::runtime.config = {};
        Manager::runtime.queryFrameBoundary = Manager::runtime.nativeDispatch = false;
        Manager::runtime.producerThread = GetCurrentThreadId();
        Manager::runtime.consumer = &PresentBody;
        Manager::runtime.failureHandler = &OwnerFailure;
        Manager::runtime.bound.store(true);
        Manager::runtime.detached = Manager::runtime.consumerCleanupComplete = false;
        Manager::runtime.releaseFailureAfterSnapshot = false;
        runtime.producerThread = Manager::runtime.producerThread;
        runtime.image = image;
        runtime.route = Manager::runtime.route;
        runtime.requestedRoute = static_cast<unsigned>(RenderObservationRoute::D3D11);
        runtime.queryGpuOwnership = runtime.classifyCustomPresent = runtime.queryFrameBoundary = runtime.nativeDispatch = runtime.captureDeviceWindow = false;
        runtime.registeredGeneration.store(generation);
        runtime.devicePhase.store(DeviceCapturePhase::Off);
        runtime.deviceLease = {};
        runtime.deviceWindow = {};
        runtime.started = GetTickCount64();
        ResetCounters();
        ResetFrameBoundaryObservation();
        runtime.active.store(true);
        viewport.fill(0);
        rhi.fill(0);
        const auto object = reinterpret_cast<std::uintptr_t>(viewport.data());
        const auto owner = reinterpret_cast<std::uintptr_t>(rhi.data());
        Put(object, image + runtime.route->viewportTable);
        Put(object + 0x18, owner);
        Put(object + 0x40, std::uintptr_t{0x1234});
        Put(object + 0x4c, std::uint32_t{1920});
        Put(object + 0x50, std::uint32_t{1080});
        Put(object + runtime.route->swapchainOffset, std::uintptr_t{1});
        Put(owner, image + 0x44fb2b8);
        Put(owner + 0x18, image + 0x44fafa8);
        Put(owner + 0x170, std::uintptr_t{2});
        Put(owner + 0x158, std::uintptr_t{3});
        Put(image + 0x576fb60, owner);
    }
    void AttachOwned()
    {
        bool pending = false;
        Check(Manager::ChangeHooks(true, pending) && !pending, "actual manager attaches Detours only to an owned fixture function");
        Manager::runtime.hooked.store(true);
    }
    void CheckForwarding(NativeRenderObservation& observer, std::uintptr_t image)
    {
        Configure(observer, image);
        const auto before = originalCalls.load();
        Check(Manager::HookPresent(viewport.data(), 0x81234567u) && receivedInterval.load() == 0x81234567u &&
                  receivedViewport.load() == reinterpret_cast<std::uintptr_t>(viewport.data()) && originalCalls.load() == before + 1,
              "real owner forwards exact viewport/full DWORD interval/true original once");
        Check(!Manager::HookPresent(viewport.data(), 0) && originalCalls.load() == before + 2 && receivedInterval.load() == 0,
              "real owner forwards zero interval and false original once");
        Check(!Manager::runtime.inFlight.load() && Manager::callbackDepth == 0, "normal owner callback accounting drains");
        const auto                     accepted = runtime.accepted.load();
        const auto                     staleBefore = originalCalls.load();
        const Present::CallbackContext stale{&Original, Present::CurrentGeneration() - 1, 1};
        Check(PresentBody(stale, viewport.data(), 0xffffffffu) && originalCalls.load() == staleBefore + 1 && receivedInterval.load() == 0xffffffffu &&
                  runtime.accepted.load() == accepted && runtime.callbacks.load() == 2 && runtime.active.load(),
              "stale diagnostic context forwards its exact original without counters/capture/disarm");
        Check(observer.End(), "forwarding case retires its Render claim");
        Configure(observer, image, &OriginalException);
        const auto exceptionBefore = originalCalls.load();
        Check(CatchOriginal() && originalCalls.load() == exceptionBefore + 1, "actual original SEH propagates through owner and diagnostic");
        Check(!Manager::runtime.inFlight.load() && !Manager::callbackDepth && !runtime.accepted.load(),
              "SEH drains actual owner accounting without inventing a return");
        Check(observer.End(), "exception case retires");
        Configure(observer, image, &OriginalNested);
        const auto nestedBefore = originalCalls.load();
        Check(Manager::HookPresent(viewport.data(), 7) && originalCalls.load() == nestedBefore + 2, "nested owner forwards each genuine callback once");
        NativeRenderObservationState state;
        observer.ReadState(state);
        Check(state.nestedCallbacks == 1 && state.nativePresentCallbacks == 2 && !state.active && state.failureReason == 6 &&
                  !Manager::runtime.inFlight.load() && !Manager::callbackDepth,
              "nested diagnostic refusal preserves real owner lifetime accounting");
        Check(observer.End(), "nested case retires");
        std::puts("actualNativePresentOwnerCallbackTested=1");
    }
    void CheckDeadline(NativeRenderObservation& observer, std::uintptr_t image)
    {
        static_assert(maximumObservationMs == 30000, "bounded diagnostics keep their admitted deadline");
        Configure(observer, image);
        runtime.started = GetTickCount64() - maximumObservationMs;
        const auto before = originalCalls.load();
        Check(Manager::HookPresent(viewport.data(), 0xf0000001u) && originalCalls.load() == before + 1,
              "expired diagnostic still forwards the actual original");
        NativeRenderObservationState state;
        observer.ReadState(state);
        Check(!state.active && !state.acceptedSnapshots && state.failureReason == 8 && state.bound,
              "deadline disarms before capture but keeps historical bound");
        ClockBlocked("expired diagnostic cannot silently release the manager's Render claim");
        Check(observer.End(), "deadline owner retires only through actual cleanup");
        Configure(observer, image);
        runtime.started = GetTickCount64() + 1000;
        observer.ReadState(state);
        Check(!state.active && state.failureReason == 8, "backward observation clock keeps the existing refusal");
        Check(observer.End(), "backward-clock owner retires");
        std::puts("actualNativePresentOwnerDiagnosticDeadlineTested=1");
    }
    void CheckDetachRetry(NativeRenderObservation& observer, std::uintptr_t image)
    {
        Configure(observer, image, &OriginalBlocked);
        originalEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        originalResume = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(originalEntered && originalResume, "owned original barriers created");
        AttachOwned();
        const auto trampoline = Manager::runtime.original;
        Present::PresentFunction volatile selected = &OriginalBlocked;
        std::thread callback([&] { selected(viewport.data(), 0xfedcba98u); });
        Check(WaitForSingleObject(originalEntered, 2000) == WAIT_OBJECT_0, "actual detoured original reached its owned barrier");
        Check(!observer.End() && Manager::runtime.hooked.load() && Manager::runtime.original == trampoline && Manager::runtime.consumer == &PresentBody &&
                  Manager::runtime.inFlight.load() == 1,
              "pending End retains the actual hook/trampoline/static consumer and blocked original");
        ClockBlocked("Clock remains excluded while the real original is blocked");
        bool pending = false;
        Check(!Manager::ChangeHooks(false, pending) && pending && Manager::runtime.hooked.load(), "post-hold actual in-flight check refuses detach safely");
        SetEvent(originalResume);
        callback.join();
        Check(!Manager::runtime.inFlight.load() && receivedInterval.load() == 0xfedcba98u, "blocked original returns with its exact DWORD argument");
        Check(observer.End() && !Manager::runtime.hooked.load(), "owning retry genuinely detaches after original drain");
        ClockAllowed("Clock admits only after successful final cleanup");
        CloseHandle(originalEntered);
        CloseHandle(originalResume);
        originalEntered = originalResume = nullptr;

        Configure(observer, image);
        AttachOwned();
        const auto failureCount = relayRefusals;
        refuseRelay = true;
        Check(!observer.End() && !Manager::runtime.hooked.load() && !Manager::runtime.detached && relayRefusals == failureCount + 1 &&
                  Manager::runtime.consumer == &PresentBody && Present::ModulePinned(),
              "real manager retains its module/consumer/admission when the relay cleanup edge refuses");
        ClockBlocked("relay refusal retains actual Render exclusion");
        refuseRelay = false;
        Check(observer.End() && Manager::runtime.detached, "relay cleanup retry calls the real release and completes manager retirement");
        ClockAllowed("Clock admits after relay cleanup retry");
        std::puts("actualNativePresentOwnerDetachRetryTested=1");
    }
    struct Device final : IUnknown
    {
        unsigned                  references = 1, releases = 0;
        bool                      fault = false, releasedUnderRender = false, releasedAfterDetach = false;
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE   AddRef() override { return ++references; }
        ULONG STDMETHODCALLTYPE   Release() override
        {
            ++releases;
            releasedUnderRender = Service::IsClaimed(Service::Category::Render);
            releasedAfterDetach = !Manager::runtime.hooked.load() && !Manager::runtime.inFlight.load() && Manager::runtime.detached;
            if (fault)
                RaiseException(0xe0434344, 0, 0, nullptr);
            return --references;
        }
    };
    void OfferDevice(Device& device)
    {
        runtime.deviceLease = {};
        runtime.deviceLease.device = reinterpret_cast<ID3D12Device*>(&device);
        runtime.deviceLease.generation = Present::CurrentGeneration();
        runtime.deviceLease.acceptedSnapshots = runtime.deviceLease.dispatchEntries = 1;
        runtime.deviceLease.sourceProfileId = 2;
        runtime.devicePending.store(true);
        runtime.devicePhase.store(DeviceCapturePhase::Ready);
    }
    void CheckDeviceRetirement(NativeRenderObservation& observer, std::uintptr_t image)
    {
        Configure(observer, image);
        AttachOwned();
        Device untaken;
        OfferDevice(untaken);
        {
            Present::Operation operation;
            const auto         generation = Present::CurrentGeneration();
            Check(operation.acquired && !Present::Release(operation, generation) && !Present::CompleteCleanup(operation, generation),
                  "actual manager rejects release/complete before real detach");
            Check(Present::Detach(operation, generation) && !Present::Release(operation, generation) && !untaken.releases,
                  "actual manager rejects release after detach until diagnostic retirement is complete");
        }
        ClockBlocked("untaken diagnostic device retains Render after actual detach");
        Check(observer.End() && untaken.releases == 1 && untaken.references == 0 && untaken.releasedUnderRender && untaken.releasedAfterDetach,
              "real diagnostic End releases untaken device exactly once after detach and before Render release");
        ClockAllowed("Clock admits only after untaken device retirement");
        Configure(observer, image);
        Device taken;
        OfferDevice(taken);
        RenderDeviceLeaseV1 lease;
        Check(observer.TakeDeviceLease(lease) == Status::Ok && lease.device == reinterpret_cast<ID3D12Device*>(&taken) && lease.size == 48 &&
                  lease.version == 1 && lease.acceptedSnapshots == 1 && lease.dispatchEntries == 1 && lease.sourceProfileId == 2 && !lease.reserved &&
                  !taken.releases,
              "actual device getter transfers independent ownership once");
        Check(observer.End() && !taken.releases && taken.references == 1, "taken Core device survives manager/diagnostic End");
        Check(ReleaseRenderDeviceReference(lease.device) && taken.releases == 1 && !taken.references, "Core releases its independent transferred reference");
        Configure(observer, image);
        Device unknown;
        unknown.fault = true;
        OfferDevice(unknown);
        Check(!observer.End() && unknown.releases == 1 && runtime.deviceReleaseFault.load() && !Manager::runtime.consumerCleanupComplete,
              "unknown COM Release effect prevents manager final retirement");
        ClockBlocked("unknown device cleanup effect retains Render");
        Check(!observer.End() && unknown.releases == 1, "unknown Release effect is not repeated on retry");
        // Only this private mock is reset to isolate following cases. This is
        // not a shipping recovery API or a claim that an unknown COM effect healed.
        runtime.deviceReleaseFault.store(false);
        Check(observer.End(), "private fault fixture retires without another Release");
        std::puts("actualNativePresentOwnerDeviceLifetimeTested=1");
        std::puts("actualNativePresentOwnerAdmissionRetirementTested=1");
    }
    DWORD WINAPI RefuseResume(HANDLE)
    {
        SetLastError(ERROR_ACCESS_DENIED);
        return static_cast<DWORD>(-1);
    }
    void CheckRetainedResume(NativeRenderObservation& observer, std::uintptr_t image)
    {
        Configure(observer, image);
        const auto ready = CreateEventW(nullptr, TRUE, FALSE, nullptr), release = CreateEventW(nullptr, TRUE, FALSE, nullptr),
                   done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(ready && release && done, "retained callback barriers created");
        std::thread callback(
            [&]
            {
                Manager::runtime.inFlight.store(1);
                AcquireSRWLockExclusive(&runtime.snapshotLock);
                SetEvent(ready);
                WaitForSingleObject(release, 5000);
                ReleaseSRWLockExclusive(&runtime.snapshotLock);
                Manager::runtime.inFlight.store(0);
                SetEvent(done);
            });
        Check(WaitForSingleObject(ready, 2000) == WAIT_OBJECT_0, "owned callback holds the actual diagnostic publication lock");
        HANDLE retained = nullptr;
        Check(SuspendThread(callback.native_handle()) == 0 &&
                  DuplicateHandle(GetCurrentProcess(), callback.native_handle(), GetCurrentProcess(), &retained, 0, FALSE, DUPLICATE_SAME_ACCESS),
              "actual sole suspension and owned handle recorded");
        Check(!Manager::ResumeOwnedThread(retained, &RefuseResume), "real resume helper preserves a failed own suspension");
        Manager::runtime.retainedSuspensions.push_back(retained);
        NativeRenderObservationState state;
        observer.ReadState(state);
        Check(Manager::runtime.inFlight.load() == 1 && Present::ReadOwnerState().retained,
              "producer ReadState returns without waiting on the retained suspended callback's lock");
        ClockBlocked("retained suspended callback excludes Clock");
        SetEvent(release);
        observer.End();
        Check(WaitForSingleObject(done, 2000) == WAIT_OBJECT_0 && !Manager::runtime.inFlight.load(),
              "real manager retries owned resume before demanding callback drain");
        callback.join();
        Check(observer.End() && Manager::runtime.retainedSuspensions.empty() && !Manager::runtime.safetyHeldSuspensions,
              "drained retained-resume retry completes actual cleanup");
        CloseHandle(ready);
        CloseHandle(release);
        CloseHandle(done);
        std::puts("actualNativePresentOwnerRetainedResumeTested=1");
    }
    void CheckGenerations(NativeRenderObservation& observer, std::uintptr_t image)
    {
        Configure(observer, image);
        const auto              generation = Present::CurrentGeneration();
        NativeRenderObservation fresh;
        Check(fresh.End() && runtime.active.load() && Present::CurrentGeneration() == generation,
              "generation-zero observer End cannot retire the registered diagnostic");
        {
            Present::Operation operation;
            Check(!Present::Release(operation, generation - 1) && Service::IsClaimed(Service::Category::Render),
                  "stale manager release cannot consume current Render ownership");
        }
        Check(Manager::HookPresent(viewport.data(), 3), "current generation publishes one actual callback");
        Check(observer.End(), "old generation saves coherent final state");
        NativeRenderObservationState before, after;
        observer.ReadState(before);
        NativeRenderObservation next;
        Check(!next.Begin() && Present::CurrentGeneration() != generation, "new ordinary-image refusal still allocates a distinct real generation");
        observer.ReadState(after);
        Check(after.generation == before.generation && after.acceptedSnapshots == before.acceptedSnapshots && after.width == before.width,
              "retired diagnostic preserves its own final snapshot across a new generation");
        Check(observer.End() && next.End(), "stale End cannot interfere with next generation cleanup");
        std::puts("actualNativePresentOwnerGenerationTested=1");
    }
}

bool mta::game_de::FixtureReleaseNativeDispatchRelay() noexcept
{
    if (refuseRelay)
    {
        ++relayRefusals;
        return false;
    }
    return mta::game_de::ReleaseNativeDispatchRelay();
}

int main()
{
    NativeRenderObservation observer;
    {
        Service::Token clock;
        const auto     generation = Present::CurrentGeneration();
        Check(Service::TryAcquire(Service::Category::Clock, clock), "actual Clock owner admitted before any Render lease");
        Check(!observer.Begin() && Present::CurrentGeneration() == generation && Service::IsClaimed(Service::Category::Clock),
              "actual manager reservation refuses Clock conflict without altering its generation or Clock owner");
        Check(Service::Release(clock), "independent Clock owner releases its own token");
    }
    Check(!observer.Begin() && !Present::ModulePinned(), "shipping image guard rejects fixture before module pin");
    Check(observer.End(), "initial public image refusal cleans up");
    const auto image = reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, 0x5770000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(image != 0, "owned scalar fixture allocation");
    // Pin the actual owner callback's module in this owned test process. This
    // does not bypass shipping image admission or load any game executable.
    HMODULE pinned = nullptr;
    Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&Manager::HookPresent),
                             &pinned) != FALSE,
          "actual callback module pinned in owned test process");
    Manager::runtime.modulePinned.store(true);
    CheckForwarding(observer, image);
    CheckDeadline(observer, image);
    CheckDetachRetry(observer, image);
    CheckDeviceRetirement(observer, image);
    CheckRetainedResume(observer, image);
    CheckGenerations(observer, image);
    Check(!Service::IsClaimed(Service::Category::Render) && !Manager::runtime.hooked.load() && !Manager::runtime.inFlight.load() &&
              Manager::runtime.retainedSuspensions.empty(),
          "focused cases leave no native service/hook/suspension ownership");
    Check(VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE) != FALSE, "owned scalar fixture freed after retirement");
    std::printf("Passed %u focused actual native present owner checks\n", checks);
    return 0;
}
