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
#include <unknwn.h>

using namespace mta::game_de;
void CheckFrameBoundaryProbe(const char* customFixture, unsigned& checks);
bool BoundaryFixtureOriginal(void*, std::uint32_t);

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
    };
    configureBoundaryPublication();
    Check(HookPresent(viewport.data(), 1) && !HookPresent(viewport.data(), 0), "API5 completed scopes preserve both native original results");
    owner.ReadState(state);
    Check(state.boundary.presentEntries == 2 && state.boundary.rejectedEntries == 0 && state.boundary.status == 2 && state.boundary.factsMask == 255 &&
              state.gpu.attempts == 2 && !state.gpu.supportPredicate,
          "API5 cumulative boundary values publish atomically with historical unsupported GPU metadata");
    AcquireSRWLockShared(&runtime.snapshotLock);
    HookPresent(viewport.data(), 1);
    ReleaseSRWLockShared(&runtime.snapshotLock);
    owner.ReadState(state);
    Check(state.boundary.presentEntries == 2 && state.gpu.attempts == 2 && state.publicationMisses == 1,
          "publication miss cannot attach new boundary counters to an old snapshot");
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
    const auto              oldGeneration = state.generation;
    NativeRenderObservation nextOwner;
    Check(!nextOwner.Begin(), "new owner still cannot admit wrong executable");
    NativeRenderObservationState retired;
    owner.ReadState(retired);
    Check(retired.generation == oldGeneration && retired.acceptedSnapshots == state.acceptedSnapshots && retired.width == state.width,
          "retired owner preserves own final evidence across generations");
    Check(nextOwner.End(), "failed next generation cleanup");
    Check(!runtime.modulePinned.load(), "synthetic fixtures never pin executable/module");
    CheckFrameBoundaryProbe(argv[3], checks);
    VirtualFree(reinterpret_cast<void*>(image), 0, MEM_RELEASE);
    std::printf("Passed %u production native render observation checks\n", checks);
    return 0;
}
