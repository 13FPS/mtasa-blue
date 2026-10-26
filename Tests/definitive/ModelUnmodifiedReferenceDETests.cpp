/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual unmodified-model SDK cycles over controlled owner replies
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoUnmodifiedDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <functional>
#include <memory>
// Reuse the existing diagnostic fixture's narrow compatibility boundary for
// unchanged Core SDK headers; test and adapter bodies keep strict warnings.
#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif
#include <SharedUtil.h>
#include <core/CCoreInterface.h>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached unmodified model fixture\n", stderr);
        std::abort();
    }
}
#include "ModelUnmodifiedReferenceFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    using Token = NativeModelBackingReferenceToken;
    using Phase = NativeModelBackingReferencePhase;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }
    template <class Operation>
    void Refuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::exception&)
        {
            Check(true, "explicit unsupported or failed owner operation propagates");
            return;
        }
        Check(false, "unsupported or failed operation cannot silently succeed");
    }
    bool Same(const Token& a, const Token& b) noexcept
    {
        return a.ownerGeneration == b.ownerGeneration && a.serial == b.serial && a.modelIndex == b.modelIndex;
    }

    // Script only the capability boundary replies. The real public SDK policy
    // owns all CPU counters; separate owned-image tests exercise native counts
    // and the actual family/cycle controller instead of duplicating them here.
    class Owner final : public NativeModelClassificationAccess, public NativeModelBackingReferenceAccess
    {
    public:
        ClockResult BindModelBackingReference(const Token& token) noexcept override
        {
            ++binds;
            current = token;
            family = token.serial;
            return ClockResult::Ok;
        }
        ClockResult ClaimUnmodifiedModelFamily(const Token& token) noexcept override
        {
            ++claims;
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            claimed = true;
            return ClockResult::Ok;
        }
        ClockResult ReadModelBackingReference(const Token& token, NativeModelBackingReferenceState& output) noexcept override
        {
            ++reads;
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            output = {};
            output.reference = current;
            output.phase = phase;
            output.familySerial = family;
            output.unmodifiedFamily = claimed;
            return ClockResult::Ok;
        }
        ClockResult RenewModelBackingReferenceCycle(const Token& token, Token& output) noexcept override
        {
            ++renews;
            if (!Same(token, current) || phase != Phase::Released)
                return ClockResult::OwnerConflict;
            // A predetermined next reply tests SDK token transport only; the
            // real controller's serial issuance/no-fork rules have own tests.
            current = {token.ownerGeneration, 91, token.modelIndex};
            output = current;
            phase = Phase::Bound;
            return ClockResult::Ok;
        }
        ClockResult AcquireModelBackingReference(const Token& token) noexcept override
        {
            ++acquires;
            acquiredSerial = token.serial;
            cpuAtAcquire = model ? model->GetRefCount() : -1;
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            phase = acquireFailure ? Phase::FaultDebt : Phase::Held;
            return acquireFailure ? ClockResult::OriginalException : ClockResult::Ok;
        }
        ClockResult ReleaseModelBackingReference(const Token& token, NativeModelReferenceRelease::ZeroPolicy policy) noexcept override
        {
            ++releases;
            cpuAtRelease = model ? model->GetRefCount() : -1;
            policyPreserved = policyPreserved && policy == NativeModelReferenceRelease::ZeroPolicy::RemoveAtZero;
            if (!Same(token, current))
                return ClockResult::BindingChanged;
            phase = releaseFailure ? Phase::FaultDebt : Phase::Released;
            return releaseFailure ? ClockResult::OriginalException : ClockResult::Ok;
        }
        ClockResult UnloadUnusedModelFamily(const Token& token, bool& removed) noexcept override
        {
            ++unloads;
            unloadedToken = token;
            cpuAtUnload = model ? model->GetRefCount() : -1;
            // Deliberately write even on refusal. The real SDK must map the
            // status before returning this output or applying any fallback.
            removed = unloadReply;
            return Same(token, current) ? unloadStatus : ClockResult::BindingChanged;
        }
        ClockResult ReadModelInterface(std::uint32_t, std::uintptr_t& output) noexcept override
        {
            ++classificationReads;
            output = 1;
            return ClockResult::Ok;
        }
        ClockResult ReadModelRender(std::uint32_t, std::uintptr_t& output) noexcept override
        {
            ++classificationReads;
            output = 1;
            return ClockResult::Ok;
        }
        Token         current{}, unloadedToken{};
        Phase         phase = Phase::Bound;
        CModelInfo*   model = nullptr;
        std::uint64_t family = 0, acquiredSerial = 0;
        unsigned      binds = 0, claims = 0, reads = 0, acquires = 0, releases = 0, renews = 0, classificationReads = 0;
        unsigned      unloads = 0;
        int           cpuAtAcquire = -1, cpuAtRelease = -1;
        int           cpuAtUnload = -1;
        bool          claimed = false, policyPreserved = true, acquireFailure = false, releaseFailure = false;
        bool          unloadReply = false;
        ClockResult   unloadStatus = ClockResult::Ok;
    };
    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        ClockResult ReadStreamingModelLoaded(std::uint32_t, NativeModelLoadedReader::Value& output) noexcept override
        {
            ++queries;
            output = {};
            output.streamedIn = loaded;
            return ClockResult::Ok;
        }
        ClockResult RequestStreamingModel(std::uint32_t, std::uint32_t flags) noexcept override
        {
            ++requests;
            Check(flags == 0x06, "pending test preserves the actual nonblocking request flags");
            return ClockResult::Ok;
        }
        unsigned queries = 0, requests = 0;
        bool     loaded = true;
    };
    class Streaming final : public fixture::StreamingFixtureBase
    {
    public:
        explicit Streaming(NativeStreamingModelAccess& owner) noexcept : StreamingFixtureBase(owner) {}
    };
    class Game final : public fixture::GameFixtureBase
    {
    public:
        CStreaming* GetStreaming() override
        {
            ++streamingReads;
            return streaming;
        }
        std::uint32_t GetBaseIDforTXD() override
        {
            ++txdReads;
            return 20000;
        }
        bool IsASyncLoadingEnabled(bool ignoreSuspend) override
        {
            ++asyncReads;
            Check(!ignoreSuspend, "actual async default argument remains false");
            return true;
        }
        CStreaming* streaming = nullptr;
        unsigned    streamingReads = 0, txdReads = 0, asyncReads = 0;
    };
    class Core final : public fixture::CoreFixtureBase
    {
    public:
        // These lifecycle cases need no diagnostic callback. Keep the real
        // Core receiver strict without replacing any final reporting hook.
        void LogEvent(uint, const char*, const char*, const char*, uint) override { fixture::Unqualified(); }
    };
    class Model final : public fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t key, Owner& owner, CGame& game, const Token& token, CCoreInterface& core) : ModelFixtureBase(key, owner, game, owner, token, core)
        {
        }
        unsigned dependencies = 0;

    protected:
        void RequestModelDependencies(EModelRequestType, const char*) override { ++dependencies; }
    };
    static_assert(std::is_abstract_v<CModelInfoUnmodifiedDE> && !std::is_abstract_v<Model>);
    static_assert(std::is_base_of_v<CModelInfoDiagnosticsDE, Model>);
    static_assert(std::is_same_v<decltype(&Model::ModelAddRef), void (CModelInfoRequestDE::*)(EModelRequestType, const char*)>);
    static_assert(std::is_same_v<decltype(&Model::RemoveRef), void (CModelInfoRequestDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::IsLoaded), bool (CModelInfoRequestDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::GetRefCount), int (CModelInfoRequestDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::SetCustomModel), bool (CModelInfoUnmodifiedDE::*)(RpClump*)>);
    static_assert(std::is_same_v<decltype(&Model::RestoreOriginalModel), void (CModelInfoUnmodifiedDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::SetColModel), void (CModelInfoUnmodifiedDE::*)(CColModel*)>);
    static_assert(std::is_same_v<decltype(&Model::RestoreColModel), void (CModelInfoUnmodifiedDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::MakeCustomModel), void (CModelInfoUnmodifiedDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::UnloadUnused), bool (CModelInfoUnmodifiedDE::*)()>);
    struct Scenario
    {
        Owner          owner;
        StreamingOwner streamingOwner;
        Streaming      streaming{streamingOwner};
        Game           game;
        Core           core;
        Token          token;
        Model          model;
        explicit Scenario(std::uint32_t key = 45) : token{19, 71, key}, model(key, owner, game, token, core)
        {
            game.streaming = &streaming;
            owner.model = &model;
        }
    };

    void ConstructorAndCustomizationChecks()
    {
        Scenario    s;
        CModelInfo& sdk = s.model;
        Check(s.owner.binds == 1 && s.owner.claims == 1 && s.owner.claimed && sdk.GetRefCount() == 0 && s.owner.acquires == 0 && s.owner.releases == 0 &&
                  s.owner.renews == 0 && s.owner.classificationReads == 0 && s.streamingOwner.queries == 0,
              "construction claims one CPU family without request, readiness, native acquisition or release effects");
        const auto reads = s.owner.reads;
        Refuses([&] { static_cast<void>(sdk.SetCustomModel(nullptr)); });
        Refuses([&] { sdk.RestoreOriginalModel(); });
        Refuses([&] { sdk.SetColModel(nullptr); });
        Refuses([&] { sdk.RestoreColModel(); });
        Refuses([&] { sdk.MakeCustomModel(); });
        Check(sdk.GetRefCount() == 0 && s.owner.reads == reads && s.owner.acquires == 0 && s.owner.releases == 0 && s.owner.renews == 0,
              "five inherited final refusals cannot mutate private immutable replacement state or invoke the owner");
    }
    void CycleChecks()
    {
        Scenario    s;
        CModelInfo& sdk = s.model;
        sdk.ModelAddRef(NON_BLOCKING, nullptr);
        sdk.ModelAddRef(NON_BLOCKING, nullptr);
        Check(sdk.GetRefCount() == 2 && s.owner.acquires == 1 && s.owner.cpuAtAcquire == 0, "real SDK shares one backing acquire across positive CPU refs");
        sdk.RemoveRef();
        Check(sdk.GetRefCount() == 1 && s.owner.releases == 0, "interior CPU decrement cannot release backing ownership");
        sdk.RemoveRef();
        Check(sdk.GetRefCount() == 0 && s.owner.releases == 1 && s.owner.cpuAtRelease == 0 && s.owner.phase == Phase::Released && s.owner.policyPreserved,
              "actual final CPU decrement precedes explicit unmodified RemoveAtZero owner release");
        Check(sdk.IsLoaded() && s.owner.renews == 0, "Released readiness alone cannot create a new native cycle");
        sdk.ModelAddRef(NON_BLOCKING, nullptr);
        Check(sdk.GetRefCount() == 1 && s.owner.renews == 1 && s.owner.acquires == 2 && s.owner.acquiredSerial == 91 && s.owner.family == 71 &&
                  s.owner.cpuAtAcquire == 0,
              "later actual AddRef transports a fresh cycle serial within the same family before CPU commit");
        sdk.RemoveRef();
        Check(sdk.GetRefCount() == 0 && s.owner.releases == 2 && s.owner.policyPreserved, "second acknowledged source cycle releases exactly once");
    }
    void PendingAndBoundedEdges()
    {
        {
            Scenario s;
            s.streamingOwner.loaded = false;
            s.model.ModelAddRef(NON_BLOCKING, nullptr);
            Check(s.model.GetRefCount() == 1 && s.owner.acquires == 0 && s.streamingOwner.requests == 1 && s.model.dependencies == 1,
                  "unloaded source AddRef retains actual pending request ordering");
            const auto reads = s.owner.reads;
            s.model.RemoveRef();
            Check(s.model.GetRefCount() == 0 && s.owner.releases == 0 && s.owner.reads == reads,
                  "pending cancellation precedes readiness and never releases an unacquired cycle");
        }
        for (const std::uint32_t key : {0u, 45u})
        {
            Scenario s(key);
            s.model.ModelAddRef(NON_BLOCKING, nullptr);
            if (key)
                s.streamingOwner.loaded = false;
            s.model.RemoveRef();
            Check(s.model.GetRefCount() == 0 && s.owner.releases == 0 && s.owner.phase == Phase::Held,
                  "actual CJ or unloaded source final-decrement skip retains its known Held obligation");
            s.streamingOwner.loaded = true;
            Refuses([&] { s.model.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(s.model.GetRefCount() == 0 && s.owner.acquires == 1 && s.owner.renews == 0 && s.owner.releases == 0,
                  "unreleased Held cycle at CPU zero refuses reuse rather than fabricating another source acquire");
        }
    }
    void FailureChecks()
    {
        {
            Scenario s;
            s.owner.acquireFailure = true;
            Refuses([&] { s.model.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(s.model.GetRefCount() == 0 && s.owner.acquires == 1 && s.owner.phase == Phase::FaultDebt,
                  "lost acquire acknowledgement precedes real CPU commit and preserves owner debt");
            Refuses([&] { s.model.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(s.owner.acquires == 1 && s.owner.renews == 0, "faulted acquisition cannot renew or replay");
        }
        {
            Scenario s;
            s.model.ModelAddRef(NON_BLOCKING, nullptr);
            s.owner.releaseFailure = true;
            Refuses([&] { s.model.RemoveRef(); });
            Check(s.model.GetRefCount() == 0 && s.owner.releases == 1 && s.owner.cpuAtRelease == 0 && s.owner.phase == Phase::FaultDebt,
                  "release exception keeps the original preceding CPU decrement and owner obligation");
            Refuses([&] { s.model.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(s.owner.acquires == 1 && s.owner.releases == 1 && s.owner.renews == 0, "lost release acknowledgement never authorizes renewal or replay");
        }
    }

    struct UnloadEffects
    {
        int                      references;
        Phase                    phase;
        std::array<unsigned, 13> calls;
    };
    UnloadEffects ObserveUnloadEffects(Scenario& s)
    {
        return {s.model.GetRefCount(),
                s.owner.phase,
                {s.owner.binds, s.owner.claims, s.owner.reads, s.owner.acquires, s.owner.releases, s.owner.renews, s.owner.classificationReads,
                 s.streamingOwner.queries, s.streamingOwner.requests, s.game.streamingReads, s.game.txdReads, s.game.asyncReads, s.model.dependencies}};
    }
    void UnloadEffectsUnchanged(Scenario& s, const UnloadEffects& before)
    {
        const auto after = ObserveUnloadEffects(s);
        Check(after.references == before.references && after.phase == before.phase && after.calls == before.calls,
              "UnloadUnused preserves CPU refs and owner phase with no readiness/request/load/acquire/release/renew/report fallback");
    }
    template <class Operation>
    void UnloadRefuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, "existing Reference Require maps every refused unload status to an exception");
            return;
        }
        Check(false, "refused unload status cannot return a provider-written boolean");
    }
    void UnloadUnusedChecks()
    {
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            const auto  issued = s.token;
            s.token = {29, 99, 88};
            const auto before = ObserveUnloadEffects(s);
            Check(!sdk.UnloadUnused() && s.owner.unloads == 1 && Same(s.owner.unloadedToken, issued),
                  "successful false is forwarded from exactly one owner call using the constructor's copied token");
            UnloadEffectsUnchanged(s, before);
            s.owner.unloadReply = true;
            Check(sdk.UnloadUnused() && s.owner.unloads == 2 && Same(s.owner.unloadedToken, issued),
                  "successful true is forwarded exactly without an extra capability or readiness query");
            UnloadEffectsUnchanged(s, before);
        }
        {
            Scenario s;
            for (unsigned cycle = 0; cycle != 2; ++cycle)
            {
                s.model.ModelAddRef(NON_BLOCKING, nullptr);
                s.model.RemoveRef();
            }
            const auto before = ObserveUnloadEffects(s);
            s.owner.unloadReply = true;
            Check(s.model.UnloadUnused() && s.owner.unloads == 1 && s.owner.unloadedToken.serial == 91 && Same(s.owner.unloadedToken, s.owner.current),
                  "unload after acknowledged renewal uses the current cycle token rather than the constructor's consumed serial");
            UnloadEffectsUnchanged(s, before);
        }
        {
            Scenario   s;
            const auto before = ObserveUnloadEffects(s);
            for (unsigned raw = 1; raw <= static_cast<unsigned>(ClockResult::OriginalException); ++raw)
            {
                s.owner.unloadStatus = static_cast<ClockResult>(raw);
                s.owner.unloadReply = (raw & 1) != 0;
                const auto calls = s.owner.unloads;
                UnloadRefuses([&] { static_cast<void>(s.model.UnloadUnused()); });
                Check(s.owner.unloads == calls + 1 && Same(s.owner.unloadedToken, s.owner.current),
                      "each Clock refusal performs exactly one current-token owner call despite a written output");
                UnloadEffectsUnchanged(s, before);
            }
        }
        {
            Scenario s;
            s.streamingOwner.loaded = false;
            s.model.ModelAddRef(NON_BLOCKING, nullptr);
            const auto before = ObserveUnloadEffects(s);
            Check(!s.model.UnloadUnused() && s.owner.unloads == 1 && s.owner.cpuAtUnload == 1,
                  "pending CPU ref does not replace the owner's actual unused-family decision");
            UnloadEffectsUnchanged(s, before);
            s.streamingOwner.loaded = true;
            Check(s.model.IsLoaded() && s.owner.acquires == 1 && s.model.GetRefCount() == 1,
                  "unload query leaves the actual pending acknowledgement intact for later IsLoaded");
        }
        {
            Scenario s;
            s.model.ModelAddRef(NON_BLOCKING, nullptr);
            s.owner.unloadStatus = ClockResult::OwnerConflict;
            s.owner.unloadReply = true;
            auto before = ObserveUnloadEffects(s);
            UnloadRefuses([&] { static_cast<void>(s.model.UnloadUnused()); });
            Check(s.owner.unloads == 1 && s.owner.cpuAtUnload == 1, "Held owner refusal is dispatched without changing positive CPU refs");
            UnloadEffectsUnchanged(s, before);
            s.owner.phase = Phase::FaultDebt;
            s.owner.unloadStatus = ClockResult::BindingChanged;
            before = ObserveUnloadEffects(s);
            UnloadRefuses([&] { static_cast<void>(s.model.UnloadUnused()); });
            Check(s.owner.unloads == 2, "faulted owner still owns the explicit refusal decision without SDK state substitution");
            UnloadEffectsUnchanged(s, before);
        }
        NativeModelBackingReferenceAccess missing;
        bool                              removed = true;
        Check(missing.UnloadUnusedModelFamily(Token{19, 71, 45}, removed) == ClockResult::NotInitialized && removed,
              "older optional owner refuses unload without overwriting its caller's output");
    }
}
int main()
{
    ConstructorAndCustomizationChecks();
    CycleChecks();
    PendingAndBoundedEdges();
    FailureChecks();
    UnloadUnusedChecks();
    std::puts("Actual MTA model unmodified UnloadUnused SDK fixture completed");
    std::puts("Actual MTA unmodified diagnostics composition SDK fixture completed");
    std::puts("Actual MTA model unmodified reference SDK fixture completed");
    std::printf("Passed %u actual MTA model unmodified reference SDK checks (%zu-bit; unmodified cycles)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
