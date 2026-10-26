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
        Token         current{};
        Phase         phase = Phase::Bound;
        CModelInfo*   model = nullptr;
        std::uint64_t family = 0, acquiredSerial = 0;
        unsigned      binds = 0, claims = 0, reads = 0, acquires = 0, releases = 0, renews = 0, classificationReads = 0;
        int           cpuAtAcquire = -1, cpuAtRelease = -1;
        bool          claimed = false, policyPreserved = true, acquireFailure = false, releaseFailure = false;
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
        CStreaming*   GetStreaming() override { return streaming; }
        std::uint32_t GetBaseIDforTXD() override { return 20000; }
        bool          IsASyncLoadingEnabled(bool ignoreSuspend) override
        {
            Check(!ignoreSuspend, "actual async default argument remains false");
            return true;
        }
        CStreaming* streaming = nullptr;
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
}
int main()
{
    ConstructorAndCustomizationChecks();
    CycleChecks();
    PendingAndBoundedEdges();
    FailureChecks();
    std::puts("Actual MTA unmodified diagnostics composition SDK fixture completed");
    std::puts("Actual MTA model unmodified reference SDK fixture completed");
    std::printf("Passed %u actual MTA model unmodified reference SDK checks (%zu-bit; unmodified cycles)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
