/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the current Core idle body against actual SDK clients.
 *****************************************************************************/
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include <SString.h>

class CCoreInterface;
class CExceptionInformation;

// The runner copies this complete declaration from the current SDK. Forward
// declarations suffice because none of the unrelated interface payloads is used.
#include "CoreIdleClientSdk.generated.inl"

namespace
{
    unsigned checks = 0;
    enum Event : unsigned
    {
        Update = 1,
        Pre,
        Post,
        IdleFirst,
        IdleReplacement,
        IdleHigh
    };

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }

    [[noreturn]] void Unexpected(const char* method)
    {
        std::fprintf(stderr, "Unexpected SDK client method: %s\n", method);
        std::abort();
    }

    class StrictClient final : public CClientBase
    {
    public:
        StrictClient(std::vector<unsigned>& eventLog, unsigned identifier) : events(eventLog), idleEvent(identifier) {}
        int  ClientInitialize(const char*, CCoreInterface*) override { Unexpected("ClientInitialize"); }
        void ClientShutdown() override { Unexpected("ClientShutdown"); }
        void PreFrameExecutionHandler() override { Unexpected("PreFrameExecutionHandler"); }
        void PreHUDRenderExecutionHandler(bool, bool) override { Unexpected("PreHUDRenderExecutionHandler"); }
        void PostFrameExecutionHandler() override { Unexpected("PostFrameExecutionHandler"); }
        void IdleHandler() override
        {
            ++idleCalls;
            observedThis = reinterpret_cast<std::uintptr_t>(this);
            events.push_back(idleEvent);
        }
        void RestreamModel(std::uint16_t) override { Unexpected("RestreamModel"); }
        bool WebsiteRequestResultHandler(const std::unordered_set<SString>&) override { Unexpected("WebsiteRequestResultHandler"); }
        bool ProcessCommand(const char*, size_t, const void*, size_t) override { Unexpected("ProcessCommand"); }
        bool HandleException(CExceptionInformation*) override { Unexpected("HandleException"); }
        void GetPlayerNames(std::vector<SString>& names) override
        {
            ++nameCalls;
            observedThis = reinterpret_cast<std::uintptr_t>(this);
            names.emplace_back(std::string("Owned client name"));
        }
        void OnWindowFocusChange(bool focused) override
        {
            ++focusCalls;
            observedThis = reinterpret_cast<std::uintptr_t>(this);
            focusValues.push_back(focused);
        }

        std::vector<unsigned>& events;
        unsigned               idleEvent;
        unsigned               idleCalls = 0;
        unsigned               nameCalls = 0, focusCalls = 0;
        std::vector<bool>      focusValues;
        std::uintptr_t         observedThis = 0;
    };
}

// These two independently mutable owners match CModManager's real accessors:
// IsLoaded reports the module reference; GetClient reports the published client.
class CModManager
{
public:
    bool         IsLoaded() const { return moduleLoaded; }
    CClientBase* GetClient()
    {
        ++clientReads;
        return client;
    }
    bool         moduleLoaded = false;
    CClientBase* client = nullptr;
    unsigned     clientReads = 0;
};

class CCore
{
public:
    explicit CCore(CModManager& manager) : m_pModManager(&manager) {}
    void HandleIdlePulse();
    void UpdateIsWindowMinimized()
    {
        ++updates;
        events.push_back(Update);
    }
    bool IsWindowMinimized() const { return minimized; }
    void DoPreFramePulse()
    {
        ++prePulses;
        events.push_back(Pre);
    }
    void DoPostFramePulse()
    {
        ++postPulses;
        events.push_back(Post);
        if (afterPost)
            afterPost(*this);
    }

    CModManager*          m_pModManager;
    bool                  minimized = false;
    unsigned              updates = 0, prePulses = 0, postPulses = 0;
    std::vector<unsigned> events;
    void (*afterPost)(CCore&) = nullptr;
    CClientBase* replacement = nullptr;
};

// No handwritten copy of the dispatch is present in this fixture.
#include "CoreIdleClientOwnershipBody.generated.inl"
#include "CoreCurrentClientDispatch.generated.inl"

namespace
{
    // The preimage regression is a null virtual call. Catch only that exception
    // around the actual member so the owned host reports failure without a crash
    // dialog. Keep this SEH function free of C++ objects requiring unwinding.
    bool InvokeIdle(CCore& core)
    {
        __try
        {
            core.HandleIdlePulse();
            return true;
        }
        __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            std::fprintf(stderr, "Core idle client ownership caught access violation\n");
            return false;
        }
    }

    bool InvokeRelated(CModManager& manager, std::vector<SString>& names)
    {
        __try
        {
            DispatchActivationFocus(&manager, false);
            DispatchPositionFocus(&manager);
            DispatchPlayerNames(&manager, names);
            return true;
        }
        __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            std::fprintf(stderr, "Core current client dispatch caught access violation\n");
            return false;
        }
    }

    void RetireAtPost(CCore& core)
    {
        Check(core.prePulses == 1 && core.postPulses == 1, "retirement occurs after both minimized pulses");
        core.m_pModManager->client = nullptr;
    }

    void ReplaceAtPost(CCore& core)
    {
        Check(core.prePulses == 1 && core.postPulses == 1, "replacement occurs after both minimized pulses");
        core.m_pModManager->client = core.replacement;
    }

    class HighClientOwner
    {
    public:
        explicit HighClientOwner(std::vector<unsigned>& events)
        {
            // Each candidate is an allocation request, never a fabricated object
            // pointer. Placement construction happens only in our committed page.
            const std::uintptr_t candidates[] = {0x0000012300000000ULL, 0x0000023400000000ULL, 0x0000034500000000ULL};
            for (const auto candidate : candidates)
            {
                storage = VirtualAlloc(reinterpret_cast<void*>(candidate), sizeof(StrictClient), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                if (storage)
                    break;
            }
            if (!storage)
                storage = VirtualAlloc(nullptr, sizeof(StrictClient), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!storage)
                throw std::runtime_error("owned high client allocation failed");
            if (reinterpret_cast<std::uintptr_t>(storage) <= (std::numeric_limits<std::uint32_t>::max)())
            {
                VirtualFree(storage, 0, MEM_RELEASE);
                storage = nullptr;
                throw std::runtime_error("owned client allocation did not exceed the 32-bit address range");
            }
            client = new (storage) StrictClient(events, IdleHigh);
        }
        ~HighClientOwner()
        {
            if (client)
                client->~StrictClient();
            if (storage)
                VirtualFree(storage, 0, MEM_RELEASE);
        }
        HighClientOwner(const HighClientOwner&) = delete;
        HighClientOwner& operator=(const HighClientOwner&) = delete;
        StrictClient*    client = nullptr;

    private:
        void* storage = nullptr;
    };

    void LiveClient()
    {
        CModManager  manager;
        CCore        core(manager);
        StrictClient client(core.events, IdleFirst);
        manager.moduleLoaded = true;
        manager.client = &client;
        Check(InvokeIdle(core), "live client idle completes");
        Check(manager.clientReads == 1, "visible idle fetches the published SDK client exactly once");
        Check(client.idleCalls == 1, "live SDK client receives exactly one idle call");
        Check(core.updates == 1 && core.prePulses == 0 && core.postPulses == 0, "visible window retains only the minimized-state update");
        Check(core.events == std::vector<unsigned>{Update, IdleFirst}, "visible idle order is update then client");
    }

    void NoPublishedClient()
    {
        const char* phases[] = {"startup before client publication", "transient client retirement", "retained DLL after shutdown"};
        for (const auto* phase : phases)
        {
            CModManager manager;
            CCore       core(manager);
            manager.moduleLoaded = true;
            Check(manager.IsLoaded() && manager.GetClient() == nullptr, phase);
            Check(InvokeIdle(core), "loaded DLL without a current client skips idle safely");
            Check(core.events == std::vector<unsigned>{Update}, "null client preserves the state update without a dispatch");
        }
        CModManager manager;
        CCore       core(manager);
        Check(InvokeIdle(core), "unloaded module without a client skips idle safely");
        Check(core.events == std::vector<unsigned>{Update}, "unloaded idle still updates minimized state");
    }

    void MinimizedLiveClient()
    {
        CModManager  manager;
        CCore        core(manager);
        StrictClient client(core.events, IdleFirst);
        manager.moduleLoaded = true;
        manager.client = &client;
        core.minimized = true;
        Check(InvokeIdle(core), "minimized live client idle completes");
        Check(manager.clientReads == 1, "minimized idle fetches the client once after both pulses");
        Check(core.prePulses == 1 && core.postPulses == 1 && client.idleCalls == 1, "both minimized pulses and one idle are preserved");
        Check(core.events == std::vector<unsigned>{Update, Pre, Post, IdleFirst}, "minimized pulses precede the idle dispatch");
    }

    void RetiredDuringPulse()
    {
        CModManager  manager;
        CCore        core(manager);
        StrictClient oldClient(core.events, IdleFirst);
        manager.moduleLoaded = true;
        manager.client = &oldClient;
        core.minimized = true;
        core.afterPost = RetireAtPost;
        Check(InvokeIdle(core), "post-frame client retirement cannot cause a null idle call");
        Check(manager.IsLoaded() && manager.GetClient() == nullptr, "refused DLL retirement remains a distinct owner after the client clears");
        Check(oldClient.idleCalls == 0, "retired client receives no idle call");
        Check(core.events == std::vector<unsigned>{Update, Pre, Post}, "retirement preserves pre/post pulses without a stale dispatch");
    }

    void ReplacedDuringPulse()
    {
        CModManager  manager;
        CCore        core(manager);
        StrictClient oldClient(core.events, IdleFirst), replacement(core.events, IdleReplacement);
        manager.moduleLoaded = true;
        manager.client = &oldClient;
        core.minimized = true;
        core.afterPost = ReplaceAtPost;
        core.replacement = &replacement;
        Check(InvokeIdle(core), "post-frame replacement completes");
        Check(manager.clientReads == 1, "replacement is fetched in one current-client query");
        Check(oldClient.idleCalls == 0 && replacement.idleCalls == 1, "idle reads the replacement published by post-frame work");
        Check(core.events == std::vector<unsigned>{Update, Pre, Post, IdleReplacement}, "replacement dispatch follows both minimized pulses");
    }

    void HighAddressClient()
    {
        CModManager     manager;
        CCore           core(manager);
        StrictClient    oldClient(core.events, IdleFirst);
        HighClientOwner owner(core.events);
        CClientBase*    sdkClient = owner.client;
        const auto      address = reinterpret_cast<std::uintptr_t>(sdkClient);
        Check(address > (std::numeric_limits<std::uint32_t>::max)(), "actual SDK client interface resides above 4 GiB");
        manager.moduleLoaded = true;
        manager.client = sdkClient;
        Check(InvokeIdle(core), "high address SDK client dispatch completes");
        Check(owner.client->idleCalls == 1 && owner.client->observedThis == address, "idle retains the complete SDK receiver address");
        core.events.clear();
        manager.client = &oldClient;
        core.minimized = true;
        core.afterPost = ReplaceAtPost;
        core.replacement = sdkClient;
        Check(InvokeIdle(core), "minimized publication of a high address client completes");
        Check(oldClient.idleCalls == 0 && owner.client->idleCalls == 2, "post-frame replacement uses the current full-width receiver");
        Check(core.events == std::vector<unsigned>{Update, Pre, Post, IdleHigh}, "high address replacement preserves exact pulse order");
        std::vector<SString> names;
        Check(InvokeRelated(manager, names), "high address SDK focus and name dispatches complete");
        Check(owner.client->focusCalls == 2 && owner.client->focusValues == std::vector<bool>{false, true},
              "both native window focus values reach the current full-width SDK receiver");
        Check(owner.client->nameCalls == 1 && names.size() == 1 && owner.client->observedThis == address,
              "player-name dispatch preserves the actual SDK receiver and payload");
        std::printf("Owned SDK client address: 0x%llx\n", static_cast<unsigned long long>(address));
    }

    void RelatedCurrentClientDispatches()
    {
        CModManager          manager;
        CCore                core(manager);
        StrictClient         first(core.events, IdleFirst), replacement(core.events, IdleReplacement);
        std::vector<SString> names;
        manager.moduleLoaded = true;
        Check(InvokeRelated(manager, names), "retained loaded DLL without a client skips both focus calls and names");
        Check(names.empty(), "absent client retains the empty player-name fallback");
        manager.client = &first;
        Check(InvokeRelated(manager, names), "live client receives focus and name callbacks");
        Check(first.focusCalls == 2 && first.focusValues == std::vector<bool>{false, true}, "activation and position focus values remain false then true");
        Check(first.nameCalls == 1 && names.size() == 1 && names[0] == "Owned client name", "live client fills the real SString player-name vector once");
        manager.client = &replacement;
        names.clear();
        Check(InvokeRelated(manager, names), "related dispatches use the currently published replacement");
        Check(first.focusCalls == 2 && first.nameCalls == 1 && replacement.focusCalls == 2 && replacement.nameCalls == 1,
              "related callbacks never reuse a previously published client");
        manager.client = nullptr;
        names.clear();
        Check(InvokeRelated(manager, names) && names.empty(), "client retirement leaves a retained DLL and an empty completion vector");
        DispatchActivationFocus(nullptr, true);
        DispatchPositionFocus(nullptr);
        DispatchPlayerNames(nullptr, names);
        Check(names.empty(), "outer null-manager gates remain intact");
        manager.moduleLoaded = false;
        manager.client = &replacement;
        Check(InvokeRelated(manager, names), "unloaded-module outer gates remain intact");
        Check(replacement.focusCalls == 2 && replacement.nameCalls == 1 && names.empty(), "module gating preserves surrounding focus and chat behavior");
    }
}

int main()
{
    static_assert(sizeof(void*) == 8, "Core idle ownership fixture requires x64");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try
    {
        LiveClient();
        NoPublishedClient();
        MinimizedLiveClient();
        RetiredDuringPulse();
        ReplacedDuringPulse();
        RelatedCurrentClientDispatches();
        HighAddressClient();
        std::printf("Passed %u actual Core idle client ownership checks (64-bit)\n", checks);
        std::puts("Core idle client ownership tests completed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Core idle client ownership failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
