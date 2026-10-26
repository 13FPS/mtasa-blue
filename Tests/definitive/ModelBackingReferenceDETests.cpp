/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual acquire-side model reference SDK bridge
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoReferenceDE.h"
#include "../../Client/game_de/CStreamingDE.h"
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
        std::fputs("FAIL: unrelated SDK operation reached model backing reference fixture\n", stderr);
        std::abort();
    }
}
#include "ModelBackingReferenceFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    using Token = NativeModelBackingReferenceToken;
    using State = NativeModelBackingReferenceState;
    unsigned checks = 0;

    void Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
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
        catch (const std::runtime_error&)
        {
            Check(true, "required owner refusal propagates through the real SDK method");
            return;
        }
        Check(false, "required owner refusal cannot manufacture SDK success");
    }

    template <class Operation>
    void InvalidConstruction(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::invalid_argument&)
        {
            Check(true, "invalid capability construction is rejected locally");
            return;
        }
        Check(false, "invalid construction cannot reach a provider callback");
    }

    enum Event : unsigned
    {
        Bind = 1,
        Read,
        StreamingRead,
        Category,
        Interface,
        Render,
        Acquire,
        Async,
        Dependencies,
        StreamingRequest,
        Report
    };

    struct Log
    {
        std::array<unsigned, 64> events{};
        std::size_t              count = 0;
        void                     Push(unsigned event) noexcept
        {
            if (count == events.size())
                fixture::Unqualified();
            events[count++] = event;
        }
        void Expect(std::initializer_list<unsigned> expected)
        {
            Check(count == expected.size(), "the actual policy has exactly the expected effects");
            std::size_t index = 0;
            for (const auto event : expected)
                Check(events[index++] == event, "backing validation precedes actual readiness and acquire preserves policy order");
            count = 0;
        }
    };

    bool Same(const Token& left, const Token& right) noexcept
    {
        return left.ownerGeneration == right.ownerGeneration && left.serial == right.serial && left.modelIndex == right.modelIndex;
    }

    // This spy supplies boundary responses only. Actual SDK counters, request
    // policy and streaming dispatch are linked from the production sources.
    class Owner final : public NativeModelClassificationAccess, public NativeModelBackingReferenceAccess
    {
    public:
        explicit Owner(Log& events) noexcept : log(events) {}
        ClockResult BindModelBackingReference(const Token& token) noexcept override
        {
            log.Push(Bind);
            ++binds;
            boundToken = token;
            return bindStatus;
        }
        ClockResult ReadModelBackingReference(const Token& token, State& output) noexcept override
        {
            log.Push(Read);
            ++reads;
            tokensPreserved = tokensPreserved && Same(token, boundToken);
            if (retired)
                return ClockResult::BindingChanged;
            if (readStatus != ClockResult::Ok)
                return readStatus;
            output = {};
            output.reference = boundToken;
            if (reportedGeneration)
                output.reference.ownerGeneration = reportedGeneration;
            output.phase = phase;
            return ClockResult::Ok;
        }
        ClockResult AcquireModelBackingReference(const Token& token) noexcept override
        {
            log.Push(Acquire);
            ++acquires;
            acquiredToken = token;
            tokensPreserved = tokensPreserved && Same(token, boundToken);
            cpuAtAcquire = observedModel ? observedModel->GetRefCount() : -1;
            if (acceptedLostAcknowledgement)
            {
                // Simulate the real owner's accepted-effect/retired state,
                // not its native counter transaction. A later read must refuse
                // before the SDK can replay an unacknowledged acquire.
                retired = true;
                return ClockResult::OriginalException;
            }
            if (acquireStatus == ClockResult::Ok)
                phase = NativeModelBackingReferencePhase::Held;
            return acquireStatus;
        }
        ClockResult ReadModelInterface(std::uint32_t model, std::uintptr_t& output) noexcept override
        {
            log.Push(Interface);
            lastInterface = model;
            output = 1;
            return ClockResult::Ok;
        }
        ClockResult ReadModelRender(std::uint32_t model, std::uintptr_t& output) noexcept override
        {
            log.Push(Render);
            lastRender = model;
            output = 1;
            return ClockResult::Ok;
        }
        Log&                             log;
        Token                            boundToken{}, acquiredToken{};
        CModelInfo*                      observedModel = nullptr;
        unsigned                         binds = 0, reads = 0, acquires = 0;
        std::uint32_t                    lastInterface = 0, lastRender = 0;
        std::uint64_t                    reportedGeneration = 0;
        int                              cpuAtAcquire = -1;
        bool                             tokensPreserved = true, retired = false, acceptedLostAcknowledgement = false;
        NativeModelBackingReferencePhase phase = NativeModelBackingReferencePhase::Bound;
        ClockResult                      bindStatus = ClockResult::Ok, readStatus = ClockResult::Ok, acquireStatus = ClockResult::Ok;
    };

    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        explicit StreamingOwner(Log& events) noexcept : log(events) {}
        ClockResult ReadStreamingModelLoaded(std::uint32_t model, NativeModelLoadedReader::Value& output) noexcept override
        {
            log.Push(StreamingRead);
            lastModel = model;
            output = {};
            output.streamedIn = loaded;
            return ClockResult::Ok;
        }
        ClockResult RequestStreamingModel(std::uint32_t model, std::uint32_t flags) noexcept override
        {
            log.Push(StreamingRequest);
            ++requests;
            lastModel = model;
            lastFlags = flags;
            return ClockResult::Ok;
        }
        Log&          log;
        std::uint32_t lastModel = 0, lastFlags = 0;
        unsigned      requests = 0;
        bool          loaded = false;
    };

    class Streaming final : public fixture::StreamingFixtureBase
    {
    public:
        explicit Streaming(NativeStreamingModelAccess& owner) noexcept : StreamingFixtureBase(owner) {}
    };

    class Game final : public fixture::GameFixtureBase
    {
    public:
        explicit Game(Log& events) noexcept : log(events) {}
        CStreaming*   GetStreaming() override { return streaming; }
        std::uint32_t GetBaseIDforTXD() override
        {
            log.Push(Category);
            return 20000;
        }
        bool IsASyncLoadingEnabled(bool ignoreSuspend) override
        {
            Check(!ignoreSuspend, "actual async decision keeps the original default argument");
            log.Push(Async);
            return true;
        }
        Log&        log;
        CStreaming* streaming = nullptr;
    };

    class ModelWithoutRelease : public fixture::ModelFixtureBase
    {
    public:
        ModelWithoutRelease(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game,
                            NativeModelBackingReferenceAccess& referenceOwner, const Token& token)
            : ModelFixtureBase(model, classification, game, referenceOwner, token)
        {
        }

    protected:
        void RequestModelDependencies(EModelRequestType, const char*) override { fixture::Unqualified(); }
        void ReportBlockingModelLoad(bool, int, const char*) override { fixture::Unqualified(); }
    };

    class Model final : public fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game, NativeModelBackingReferenceAccess& referenceOwner,
              const Token& token, Log& events)
            : ModelFixtureBase(model, classification, game, referenceOwner, token), log(events)
        {
        }

    protected:
        void RequestModelDependencies(EModelRequestType type, const char* tag) override
        {
            log.Push(Dependencies);
            Check(type == NON_BLOCKING && tag == expectedTag, "focused pending request preserves caller type and borrowed tag");
        }
        void ReportBlockingModelLoad(bool, int, const char*) override { fixture::Unqualified(); }
        // Final release is intentionally unsupported by this acquire-only
        // product; this fixture trap merely makes the unrelated SDK concrete.
        void RemoveNativeModelInterfaceReference() override { fixture::Unqualified(); }

    public:
        Log&        log;
        const char* expectedTag = nullptr;
    };

    static_assert(std::is_abstract_v<CModelInfoReferenceDE> && std::is_abstract_v<ModelWithoutRelease>);
    static_assert(!std::is_abstract_v<Model> && std::is_base_of_v<CModelInfo, Model>);
    static_assert(!std::is_copy_constructible_v<Model> && !std::is_move_constructible_v<Model>);
    static_assert(std::is_same_v<decltype(&Model::Request), void (CModelInfoRequestDE::*)(EModelRequestType, const char*)>);
    static_assert(std::is_same_v<decltype(&Model::IsLoaded), bool (CModelInfoRequestDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::ModelAddRef), void (CModelInfoRequestDE::*)(EModelRequestType, const char*)>);
    static_assert(std::is_same_v<decltype(&Model::RemoveRef), void (CModelInfoRequestDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::GetRefCount), int (CModelInfoRequestDE::*)()>);

    struct Scenario
    {
        Log            log;
        Owner          owner{log};
        StreamingOwner streamingOwner{log};
        Streaming      streaming{streamingOwner};
        Game           game{log};
        Token          token{19, 71, 45};
        Model          model{45, owner, game, owner, token, log};
        Scenario()
        {
            game.streaming = &streaming;
            owner.observedModel = &model;
            log.Expect({Bind});
        }
    };

    void ConstructorChecks()
    {
        Log   log;
        Owner owner(log), other(log);
        Game  game(log);
        Token token{19, 71, 45};
        InvalidConstruction([&] { Model rejected(45, owner, game, other, token, log); });
        Check(owner.binds == 0 && other.binds == 0, "different complete providers are rejected before either bind callback");
        log.Expect({});
        for (const Token bad : {Token{0, 71, 45}, Token{19, 0, 45}, Token{19, 71, 46}})
        {
            InvalidConstruction([&] { Model rejected(45, owner, game, owner, bad, log); });
            Check(owner.binds == 0, "malformed or model-mismatched tokens are rejected before bind");
            log.Expect({});
        }
        owner.bindStatus = ClockResult::BindingChanged;
        Refuses([&] { Model rejected(45, owner, game, owner, token, log); });
        Check(owner.binds == 1 && owner.reads == 0 && owner.acquires == 0, "bind refusal propagates once without native observation or acquisition");
        log.Expect({Bind});
        owner.bindStatus = ClockResult::Ok;
        Model valid(45, owner, game, owner, token, log);
        Check(dynamic_cast<void*>(static_cast<NativeModelClassificationAccess*>(&owner)) ==
                  dynamic_cast<void*>(static_cast<NativeModelBackingReferenceAccess*>(&owner)),
              "distinct capability subobjects admit the same complete provider identity");
        Check(owner.binds == 2 && valid.GetRefCount() == 0 && owner.reads == 0 && owner.acquires == 0,
              "construction binds only CPU owner metadata and starts the inherited CPU count at zero");
        log.Expect({Bind});
    }

    void LoadedAcquireChecks()
    {
        Scenario    s;
        CModelInfo& sdk = s.model;
        const Token original = s.token;
        s.token = Token{29, 99, 88};
        s.streamingOwner.loaded = true;
        sdk.ModelAddRef(NON_BLOCKING, nullptr);
        Check(sdk.GetRefCount() == 1 && s.owner.acquires == 1 && s.owner.cpuAtAcquire == 0,
              "first loaded AddRef acquires backing before the actual CPU counter commits");
        s.log.Expect({Read, StreamingRead, Category, Interface, Render, Read, StreamingRead, Category, Interface, Render, Acquire});
        sdk.ModelAddRef(BLOCKING, nullptr);
        Check(sdk.GetRefCount() == 2 && s.owner.acquires == 1 && s.streamingOwner.requests == 0,
              "one genuine backing acquisition supports the second inherited CPU reference");
        s.log.Expect({Read, StreamingRead, Category, Interface, Render});
        Check(Same(s.owner.boundToken, original) && Same(s.owner.acquiredToken, original) && s.owner.tokensPreserved,
              "constructor copies the immutable token rather than borrowing caller storage");
        Check(s.owner.lastInterface == 45 && s.owner.lastRender == 45 && s.streamingOwner.lastModel == 45,
              "the acquire layer preserves actual model/streaming/readiness receivers");
        Check(sdk.GetRefCount() == 2, "GetRefCount reports only the inherited CPU count");
        s.log.Expect({});
        sdk.RemoveRef();
        Check(sdk.GetRefCount() == 1 && s.owner.acquires == 1, "interior inherited CPU decrement needs no backing release provider");
        s.log.Expect({});
    }

    void PendingChecks()
    {
        Scenario    s;
        CModelInfo& sdk = s.model;
        const char  tag[] = "pending backing acquire";
        s.model.expectedTag = tag;
        sdk.ModelAddRef(NON_BLOCKING, tag);
        Check(sdk.GetRefCount() == 1 && s.owner.acquires == 0 && s.streamingOwner.requests == 1 && s.streamingOwner.lastFlags == 0x06,
              "unloaded actual request commits a pending CPU reference without backing acquisition");
        s.log.Expect({Read, StreamingRead, Async, Read, StreamingRead, Dependencies, StreamingRequest, Read, StreamingRead});
        s.streamingOwner.loaded = true;
        Check(sdk.IsLoaded() && sdk.GetRefCount() == 1 && s.owner.acquires == 1 && s.owner.cpuAtAcquire == 1,
              "actual IsLoaded acquires pending backing while retaining the existing CPU count");
        s.log.Expect({Read, StreamingRead, Category, Interface, Render, Acquire});
        Check(sdk.IsLoaded() && s.owner.acquires == 1, "successful pending acknowledgement is consumed once by the inherited policy");
        s.log.Expect({Read, StreamingRead, Category, Interface, Render});
    }

    void RefusalChecks()
    {
        {
            Scenario s;
            s.streamingOwner.loaded = true;
            s.owner.reportedGeneration = 20;
            Refuses([&] { static_cast<void>(s.model.IsLoaded()); });
            Check(s.owner.acquires == 0 && s.model.GetRefCount() == 0, "successful read with a different generation is not a usable facade capability");
            s.log.Expect({Read});
            s.owner.reportedGeneration = 0;
            s.owner.phase = NativeModelBackingReferencePhase::FaultDebt;
            Refuses([&] { s.model.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(s.owner.acquires == 0 && s.model.GetRefCount() == 0, "successful read with unresolved fault debt cannot enter readiness or acquire");
            s.log.Expect({Read});
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            s.streamingOwner.loaded = true;
            s.owner.readStatus = ClockResult::BindingChanged;
            Refuses([&] { static_cast<void>(sdk.IsLoaded()); });
            s.log.Expect({Read});
            Refuses([&] { sdk.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(sdk.GetRefCount() == 0 && s.owner.acquires == 0 && s.streamingOwner.requests == 0,
                  "stale generation refusal precedes readiness, native acquire and CPU increment");
            s.log.Expect({Read});
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            s.streamingOwner.loaded = true;
            s.owner.acceptedLostAcknowledgement = true;
            Refuses([&] { sdk.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(sdk.GetRefCount() == 0 && s.owner.acquires == 1 && s.owner.cpuAtAcquire == 0 && s.owner.retired,
                  "accepted lost acknowledgement preserves first-acquire failure before CPU increment");
            s.log.Expect({Read, StreamingRead, Category, Interface, Render, Read, StreamingRead, Category, Interface, Render, Acquire});
            Refuses([&] { sdk.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(s.owner.acquires == 1 && sdk.GetRefCount() == 0, "retired owner prevents acquire replay after a lost first-add acknowledgement");
            s.log.Expect({Read});
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, nullptr);
            s.log.Expect({Read, StreamingRead, Async, Read, StreamingRead, Dependencies, StreamingRequest, Read, StreamingRead});
            s.streamingOwner.loaded = true;
            s.owner.acceptedLostAcknowledgement = true;
            Refuses([&] { static_cast<void>(sdk.IsLoaded()); });
            Check(sdk.GetRefCount() == 1 && s.owner.acquires == 1 && s.owner.cpuAtAcquire == 1 && s.owner.retired,
                  "pending acquire exception preserves the preceding CPU count and unacknowledged owner effect");
            s.log.Expect({Read, StreamingRead, Category, Interface, Render, Acquire});
            Refuses([&] { static_cast<void>(sdk.IsLoaded()); });
            Check(s.owner.acquires == 1 && sdk.GetRefCount() == 1, "retired owner stops pending acknowledgement replay without rolling back the CPU count");
            s.log.Expect({Read});
            Refuses([&] { sdk.ModelAddRef(NON_BLOCKING, nullptr); });
            Check(sdk.GetRefCount() == 1 && s.owner.acquires == 1, "stale positive-count AddRef cannot increment CPU state or replay acquisition");
            s.log.Expect({Read});
            Check(sdk.GetRefCount() == 1, "CPU count remains queryable after owner retirement");
            s.log.Expect({});
        }
    }

    void DefaultCapabilityChecks()
    {
        NativeModelBackingReferenceAccess missing;
        Token                             token{19, 71, 45};
        const Token                       previous = token;
        State                             state;
        state.reference = token;
        state.phase = NativeModelBackingReferencePhase::Held;
        Check(missing.ReserveModelBackingReference(19, 45, token) == ClockResult::NotInitialized && Same(token, previous),
              "missing reserve authority refuses without minting or overwriting a token");
        Check(missing.BindModelBackingReference(token) == ClockResult::NotInitialized &&
                  missing.AcquireModelBackingReference(token) == ClockResult::NotInitialized,
              "missing backing authority cannot accept bind or native acquisition");
        Check(missing.ReadModelBackingReference(token, state) == ClockResult::NotInitialized && Same(state.reference, previous) &&
                  state.phase == NativeModelBackingReferencePhase::Held,
              "missing read authority preserves the caller's previous state");
        Check(missing.HasPendingModelBackingReferences(), "missing owner defaults conservatively to pending obligations");
    }
}

int main()
{
    DefaultCapabilityChecks();
    ConstructorChecks();
    LoadedAcquireChecks();
    PendingChecks();
    RefusalChecks();
    std::puts("Actual MTA model backing reference SDK fixture completed");
    std::printf("Passed %u actual MTA model backing reference SDK checks (%zu-bit; acquire only)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
