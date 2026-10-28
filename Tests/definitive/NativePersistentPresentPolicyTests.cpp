/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual shared manager/provider and bounded diagnostic lifetime proof
 *****************************************************************************/
#include "../../Client/game_de/NativePresentOwner.cpp"
#include "../../Client/game_de/NativeFrameBoundaryProbe.cpp"
#include "NativePresentPolicyOwnedImageFixture.inl"
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace mta::game_de::NativePresentOwner
{
    ClockResult FixtureBeginPolicy(const Operation&, PolicyIntent, std::uint64_t&) noexcept;
}
// The fixture replaces only the game-image attach admission. Its negative case
// invokes that real admission; positive cases supply owned MEM_IMAGE profiles.
// Callback selection, provider intent, M gate and cleanup remain actual code.
#define BeginPolicy FixtureBeginPolicy
#include "../../Client/game_de/NativePresentationPolicyOwner.cpp"
#undef BeginPolicy
#include "../../Client/game_de/NativeRenderObservation.cpp"

using namespace mta::game_de;
namespace Manager = mta::game_de::PresentOwnerDetail;
namespace Probe = mta::game_de::boundary_detail;
namespace Owner = mta::game_de::NativePresentOwner;
namespace Gate = mta::game_de::NativePresentTransitionGate;
namespace Admission = mta::game_de::NativeServiceAdmission;

namespace
{
    std::atomic<unsigned>                         checks{0};
    bool                                          ownedAdmission = false;
    NativePresentPolicyFixture::OwnedPolicyImage* fixture = nullptr;
    std::atomic<unsigned>                         targetCalls{0};
    std::atomic<std::uint32_t>                    receivedInterval{0}, receivedFlags{0}, wrapperInterval{0};
    std::atomic<ClockResult>                      targetTransition{ClockResult::NotInitialized};
    std::atomic<bool>                             blockTarget{false};
    HANDLE                                        targetEntered = nullptr, targetResume = nullptr;
    constexpr HRESULT                             targetResult = static_cast<HRESULT>(0x08765432);

    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed after %u checks: %s\n", checks.load(), reason);
            std::exit(1);
        }
    }
    HRESULT STDMETHODCALLTYPE Target(IDXGISwapChain*, UINT interval, UINT flags)
    {
        receivedInterval.store(interval);
        receivedFlags.store(flags);
        targetCalls.fetch_add(1);
        Gate::Transition transition;
        const auto       result = Gate::TryAcquireTransition(0x1234, transition);
        targetTransition.store(result);
        if (result == ClockResult::Ok)
            Check(transition.Release() == ClockResult::Ok, "original can release its independent Clock cooperation transition");
        if (blockTarget.load())
        {
            SetEvent(targetEntered);
            WaitForSingleObject(targetResume, 5000);
        }
        return targetResult;
    }
    bool Wrapper(void*, std::uint32_t interval)
    {
        wrapperInterval.store(interval);
        ++Probe::dispatchDepth;
        HRESULT result = E_FAIL;
        __try
        {
            result = Probe::PresentBody(fixture->Chain(), interval, 0xfedcba98u, fixture->Caller(), &Target, true);
        }
        __finally
        {
            --Probe::dispatchDepth;
        }
        return result == targetResult;
    }
    void RetireOwnedPatchRepresentation()
    {
        // These are test-owned profile bytes, never Detours-installed game
        // hooks. Model their independently completed native retirement before
        // exercising the real final owner/gate/module cleanup order.
        Check(!Manager::runtime.inFlight.load() && !Probe::runtime.inFlight.load(), "owned patch representation retires after original drain");
        Manager::runtime.hooked.store(false);
        Probe::runtime.committed = false;
        Probe::runtime.dispatchPatched = false;
        Probe::runtime.dispatchRelay = nullptr;
    }
    void CheckClockBlocked(const char* reason)
    {
        Admission::Token clock;
        Check(!Admission::TryAcquire(Admission::Category::Clock, clock), reason);
    }
    void CheckClockAllowed()
    {
        Admission::Token clock;
        Check(Admission::TryAcquire(Admission::Category::Clock, clock), "steady policy owns no global Render exclusion");
        Check(Admission::Release(clock), "independent Clock admission releases");
    }
}

namespace mta::game_de::NativePresentOwner
{
    ClockResult FixtureBeginPolicy(const Operation& operation, PolicyIntent intent, std::uint64_t& generation) noexcept
    {
        if (!ownedAdmission)
            return BeginPolicy(operation, intent, generation);
        if (!operation.acquired || !fixture || !fixture->valid || !intent || Manager::runtime.policyGeneration.load())
            return ClockResult::Busy;
        Admission::Token claim;
        if (!Admission::TryAcquire(Admission::Category::Render, claim))
            return ClockResult::Busy;
        generation = Manager::runtime.generation.fetch_add(1) + 1;
        Manager::runtime.policyGeneration.store(generation);
        Manager::runtime.admissionSerial.store(claim.serial);
        Manager::runtime.admissionGeneration.store(generation);
        Manager::runtime.hookProducerThread = Manager::runtime.producerThread = GetCurrentThreadId();
        Manager::runtime.image = fixture->image;
        Manager::runtime.original = &Wrapper;
        Manager::runtime.route = &Manager::d3d12Route;
        Manager::runtime.queryFrameBoundary = Manager::runtime.nativeDispatch = true;
        Manager::runtime.config = {RenderObservationRoute::D3D12, false, false, true, true, false, false};
        Manager::runtime.consumer.store(nullptr);
        Manager::runtime.failureHandler.store(nullptr);
        Manager::runtime.intent.store(intent);
        Manager::runtime.policyActive.store(false);
        Manager::runtime.policyRetiring.store(false);
        Manager::runtime.sharedDiagnostic.store(false);
        Manager::runtime.diagnosticActive.store(false);
        Manager::runtime.diagnosticDispatch.store(Manager::diagnosticClosed);
        Manager::runtime.policyCallbacks.store(0);
        Manager::runtime.policyApplied.store(0);
        Manager::runtime.policyRejected.store(0);
        Manager::runtime.detached = Manager::runtime.consumerCleanupComplete = false;
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&Manager::HookPresent),
                                &pinned))
            return ClockResult::NotInitialized;
        Manager::runtime.modulePinned.store(true);
        if (!fixture->ConfigureOwnedPolicyImage(&Target))
            return ClockResult::BindingChanged;
        Manager::runtime.bound.store(true);
        Manager::runtime.hooked.store(true);
        auto result = Gate::RegisterOwner(generation, Manager::runtime.registration);
        if (result == ClockResult::Ok && !ConfigureNativePresentPolicy(generation, &Manager::SelectPolicyInterval))
            result = ClockResult::BindingChanged;
        if (result == ClockResult::Ok && !Manager::ReleaseAdmission(generation))
            result = ClockResult::RestoreFailure;
        Manager::runtime.policyFailure.store(result);
        Manager::runtime.policyActive.store(result == ClockResult::Ok);
        return result;
    }
}

int main()
{
    NativePresentationPolicyOwner unbound;
    Check(unbound.RetireAll() == ClockResult::Ok && unbound.End() == ClockResult::Ok && !unbound.Pending(),
          "uninitialized adapter release has no invented policy cleanup debt");
    NativePresentPolicyFixture::OwnedPolicyImage image;
    fixture = &image;
    Check(image.valid, "owned actual executable-image section and retained system DXGI are available");
    targetEntered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    targetResume = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(targetEntered && targetResume, "owned blocking original events exist");
    unsigned                      adapter = 0, clientObject = 0;
    auto*                         core = reinterpret_cast<CCoreInterface*>(std::uintptr_t{0x1234});
    NativePresentationPolicyOwner provider;
    Check(provider.BindOwner(core, GetCurrentThreadId(), &adapter) == ClockResult::Ok, "provider binds actual producer identity tuple");
    PresentationPolicyStateV1 state;
    Check(provider.Begin(state) != ClockResult::Ok && state.generation && !state.active && provider.Pending(),
          "actual shipping admission rejects owned EXE while preserving the real failed-generation cleanup debt");
    Check(provider.End() == ClockResult::Ok && !provider.Pending(), "actual early failed attach cleans up without fabricated Configure");
    ownedAdmission = true;
    Check(provider.Begin(state) == ClockResult::Ok && state.active && state.hooked && state.modulePinned && !state.pendingCleanup,
          "owned native attach grants actual provider only after hook and cooperation registration");
    const auto policyGeneration = state.generation;
    CheckClockAllowed();
    PresentationPolicyClientV1 client;
    Check(provider.BindClient(policyGeneration, &clientObject, client) == ClockResult::Ok && client.clientGeneration,
          "constructed object receives an exact fresh provider/client tuple");
    Check(provider.DisableVSync(client) == ClockResult::Ok, "real SDK consumer publishes one-way current client intent");
    const auto before = targetCalls.load();
    Check(Manager::HookPresent(image.viewport.data(), 0xe1234567u), "actual wrapper original result survives final-native interval selection");
    Check(targetCalls.load() == before + 1 && wrapperInterval.load() == 0xe1234567u && receivedInterval.load() == 0 && receivedFlags.load() == 0xfedcba98u,
          "only final native interval changes; wrapper input, all flags and exact target count survive");
    Check(targetTransition.load() == ClockResult::Ok, "short policy read is released before original target");
    Check(provider.ReadState(state) == ClockResult::Ok && state.applied == 1 && state.requested && state.clientActive,
          "provider reads actual native decision counters and current client intent");
    Gate::Transition transition;
    Check(Gate::TryAcquireTransition(900, transition) == ClockResult::Ok, "Clock may retain its actual transition between policy callbacks");
    Check(Manager::HookPresent(image.viewport.data(), 0x91234567u) && receivedInterval.load() == 0x91234567u,
          "busy Clock short-read forwards the entire incoming native interval");
    Check(transition.Release() == ClockResult::Ok, "Clock transition releases for later policy callback");

    NativeRenderObservation observer;
    Check(observer.Begin(RenderObservationRoute::D3D12, true, true, true, true, false, false),
          "real bounded diagnostic joins existing policy without a second hook or policy reset");
    Check(Owner::CurrentGeneration() != policyGeneration && Owner::SharedDiagnostic(Owner::CurrentGeneration()),
          "bounded diagnostic has its own generation while persistent provider epoch remains unchanged");
    CheckClockBlocked("actual diagnostic keeps global Render exclusion through its original/COM lifetime");
    blockTarget.store(true);
    std::thread blocked([] { Manager::HookPresent(fixture->viewport.data(), 7); });
    Check(WaitForSingleObject(targetEntered, 5000) == WAIT_OBJECT_0, "actual shared consumer reached its owned original");
    Check(!observer.End(), "bounded End cannot retire an original or post-original consumer still in flight");
    CheckClockBlocked("failed diagnostic drain retains Render before any COM retirement");
    SetEvent(targetResume);
    blocked.join();
    blockTarget.store(false);
    Check(observer.End(), "bounded consumer retires after original drains while shared policy hook stays installed");
    NativeRenderObservationState observation;
    observer.ReadState(observation);
    Check(!observation.active && !observation.pendingCleanup && Manager::runtime.hooked.load(),
          "completed diagnostic does not mistake permanent policy hook for its cleanup debt");
    Check(provider.ReadState(state) == ClockResult::Ok && state.generation == policyGeneration && state.active,
          "bounded diagnostic cleanup preserves actual policy generation and provider health");
    CheckClockAllowed();
    Check(observer.Begin(RenderObservationRoute::D3D12, true, true, true, true, false, false),
          "a second bounded diagnostic gets fresh state on the same installed owner");
    mta::game_de::runtime.started = GetTickCount64() - 30000;
    observer.ReadState(observation);
    Check(!observation.active && observation.failureReason == 8 && observer.End(),
          "shared diagnostic still enforces the original thirty-second bound before releasing its own claim");
    Check(provider.ReadState(state) == ClockResult::Ok && state.active && state.generation == policyGeneration,
          "diagnostic timeout never becomes permanent policy authority or retires its separate epoch");

    const auto oldClient = client;
    Check(provider.RetireClient(client) == ClockResult::Ok && provider.DisableVSync(client) == ClockResult::OwnerConflict,
          "retired exact client immediately loses publication authority");
    Check(provider.BindClient(policyGeneration, &clientObject, client) == ClockResult::Ok && client.clientGeneration != oldClient.clientGeneration,
          "same object address gets a new nonreused client epoch");
    Check(provider.DisableVSync(oldClient) == ClockResult::OwnerConflict && provider.DisableVSync(client) == ClockResult::Ok,
          "old same-address client cannot publish into its successor");
    ClockResult foreign = ClockResult::Ok;
    std::thread wrong([&] { foreign = provider.DisableVSync(client); });
    wrong.join();
    Check(foreign == ClockResult::WrongThread, "only admitted actual producer can publish SDK intent");
    Check(provider.End(policyGeneration + 1) == ClockResult::OwnerConflict, "stale provider epoch cannot end current hooks");

    Admission::Token clock;
    Check(Admission::TryAcquire(Admission::Category::Clock, clock), "steady policy permits actual Clock lease");
    Check(Gate::TryAcquireTransition(901, transition) == ClockResult::Ok, "actual Clock transition owns its short-read exclusion");
    Check(provider.RetireAll() == ClockResult::Ok && provider.End() == ClockResult::Busy,
          "producer disarms before Clock; native detach waits until retained Clock ownership ends");
    Check(provider.ReadState(state) == ClockResult::Ok && state.retiring && !state.active && state.pendingCleanup && state.hooked &&
              Gate::RegisteredModuleRetained(),
          "Clock-retained policy keeps storage/module and reports genuine passive cleanup debt");
    Check(Manager::HookPresent(image.viewport.data(), 0xf1234567u) && receivedInterval.load() == 0xf1234567u,
          "retired provider passively forwards actual original during Clock retention");
    Check(transition.Release() == ClockResult::Ok && Admission::Release(clock), "Clock retires both actual retained ownership tokens");
    RetireOwnedPatchRepresentation();
    Check(provider.End() == ClockResult::Ok && !provider.Pending() && !Gate::RegisteredModuleRetained(),
          "final actual owner unregister and module cleanup follow Clock and hook drain");
    Check(provider.DisableVSync(client) == ClockResult::OwnerConflict && provider.RetireAll() == ClockResult::Ok && provider.End() == ClockResult::Ok,
          "old SDK token is rejected after final provider retirement, while empty teardown is idempotent");
    CheckClockAllowed();
    CloseHandle(targetEntered);
    CloseHandle(targetResume);
    std::puts("actualPersistentPresentPolicyIntervalTested=1");
    std::puts("actualPersistentPresentPolicyShortReadTested=1");
    std::puts("actualPersistentPresentPolicyClientEpochTested=1");
    std::puts("actualPersistentPresentPolicySharedDiagnosticTested=1");
    std::puts("actualPersistentPresentPolicyClockRetirementTested=1");
    std::puts("actualPersistentPresentPolicyAdmissionRefusalTested=1");
    std::printf("Passed %u focused actual persistent presentation owner checks\n", checks.load());
    return 0;
}
