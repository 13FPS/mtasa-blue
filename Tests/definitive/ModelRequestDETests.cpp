/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Test actual model request/ref SDK dispatch without native stand-ins
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoRequestDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached model request fixture\n", stderr);
        std::abort();
    }
}
#include "ModelRequestFixture.generated.h"

namespace
{
    using namespace mta::game_de;
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
    struct Failure : std::runtime_error
    {
        explicit Failure(int value) : std::runtime_error("existing native model owner refused"), phase(value) {}
        int phase;
    };
    template <class Operation>
    void FailsAt(Operation operation, int phase)
    {
        try
        {
            operation();
        }
        catch (const Failure& failure)
        {
            Check(failure.phase == phase, "original required backend failure propagates without translation or retry");
            return;
        }
        Check(false, "mandatory backend effect failure cannot silently succeed");
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
            Check(true, "real SDK refusal propagates");
            return;
        }
        Check(false, "missing/refused real SDK service cannot be bypassed");
    }
    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        ClockResult ReadStreamingModelLoaded(std::uint32_t model, NativeModelLoadedReader::Value& output) noexcept override
        {
            ++queries;
            lastModel = model;
            if (queryStatus != ClockResult::Ok)
                return queryStatus;
            NativeModelLoadedReader::Value value;
            value.streamedIn = loaded;
            output = value;
            return ClockResult::Ok;
        }
        ClockResult ReadStreamingUpgradeLoaded(std::uint32_t model, bool& output) noexcept override
        {
            ++upgradeQueries;
            lastModel = model;
            if (queryStatus != ClockResult::Ok)
                return queryStatus;
            output = loaded;
            return ClockResult::Ok;
        }
        ClockResult RequestStreamingModel(std::uint32_t model, std::uint32_t flags) noexcept override
        {
            ++requests;
            lastModel = model;
            lastFlags = flags;
            return requestStatus;
        }
        ClockResult RequestStreamingUpgrade(std::uint32_t model, std::uint32_t flags) noexcept override
        {
            ++upgradeRequests;
            lastModel = model;
            lastFlags = flags;
            return requestStatus;
        }
        ClockResult LoadStreamingModels(bool priority) noexcept override
        {
            priorities.push_back(priority);
            if (loadStatus != ClockResult::Ok)
                return loadStatus;
            if (priorities.size() >= completeAfterLoads)
                loaded = true;
            return ClockResult::Ok;
        }
        bool              loaded = false;
        std::uint32_t     lastModel = 0, lastFlags = 0;
        unsigned          requests = 0, queries = 0, upgradeRequests = 0, upgradeQueries = 0;
        std::size_t       completeAfterLoads = static_cast<std::size_t>(-1);
        std::vector<bool> priorities;
        ClockResult       requestStatus = ClockResult::Ok, queryStatus = ClockResult::Ok, loadStatus = ClockResult::Ok;
    };
    class Streaming final : public fixture::StreamingFixtureBase
    {
    public:
        explicit Streaming(NativeStreamingModelAccess& owner) : StreamingFixtureBase(owner) {}
    };
    class Game final : public fixture::GameFixtureBase
    {
    public:
        CStreaming* GetStreaming() override
        {
            ++streamingReads;
            return streaming;
        }
        bool IsASyncLoadingEnabled(bool ignoreSuspend) override
        {
            Check(!ignoreSuspend, "actual async decision uses the original default false argument");
            ++asyncReads;
            if (asyncFailure)
                throw Failure(8);
            return async;
        }
        CStreaming* streaming = nullptr;
        bool        async = true, asyncFailure = false;
        unsigned    streamingReads = 0, asyncReads = 0;
    };
    struct NativeContext
    {
        // Test-only implementations of REQUIRED backend gates. No native
        // factory, render pointer, classic field or sparse map is fabricated.
        bool              renderReady = true, retired = false;
        int               failPhase = 0;
        unsigned          usableReads = 0, dependencies = 0, increments = 0, removals = 0, reports = 0;
        EModelRequestType dependencyType = NON_BLOCKING;
        const char*       dependencyTag = nullptr;
        const char*       reportTag = nullptr;
        bool              reportLoaded = false;
        int               reportAttempts = -1;
        std::vector<int>  effects;
    };
    class Model final : public fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t model, NativeModelClassificationAccess& classification, CGame& game, CStreaming& streaming, NativeContext& native) noexcept
            : ModelFixtureBase(model, classification, game), streaming(streaming), native(native)
        {
        }

    protected:
        bool DoIsLoaded() override
        {
            ++native.usableReads;
            if (native.retired || native.failPhase == 1)
                throw Failure(1);
            // Streaming state alone deliberately cannot bypass the separately
            // mandatory DFF usable-render-data gate, represented here only by
            // test context. The real native implementation remains pure.
            return streaming.HasModelLoaded(GetModel()) && native.renderReady;
        }
        void RequestModelDependencies(EModelRequestType type, const char* tag) override
        {
            ++native.dependencies;
            native.dependencyType = type;
            native.dependencyTag = tag;
            native.effects.push_back(2);
            if (native.failPhase == 2)
                throw Failure(2);
        }
        void ReportBlockingModelLoad(bool loaded, int attempts, const char* tag) override
        {
            ++native.reports;
            native.reportLoaded = loaded;
            native.reportAttempts = attempts;
            native.reportTag = tag;
            native.effects.push_back(5);
            if (native.failPhase == 5)
                throw Failure(5);
        }
        void IncrementNativeModelInterfaceReference() override
        {
            ++native.increments;
            native.effects.push_back(3);
            if (native.failPhase == 3)
            {
                // The existing native owner must retire an entered failed
                // mutation. Later required observations then refuse, preventing
                // a pending acknowledgement from replaying that native effect.
                native.retired = true;
                throw Failure(3);
            }
        }
        void RemoveNativeModelInterfaceReference() override
        {
            ++native.removals;
            native.effects.push_back(4);
            if (native.failPhase == 4)
            {
                native.retired = true;
                throw Failure(4);
            }
        }

    private:
        CStreaming&    streaming;
        NativeContext& native;
    };
    struct Scenario
    {
        NativeModelClassificationAccess classification;
        StreamingOwner                  streamingOwner;
        Streaming                       streaming{streamingOwner};
        Game                            game;
        NativeContext                   native;
        Model                           model;
        explicit Scenario(std::uint32_t id = 45) : model(id, classification, game, streaming, native) { game.streaming = &streaming; }
    };
    static_assert(!std::is_abstract_v<Model> && std::is_base_of_v<CModelInfo, Model>);
    static_assert(!std::is_abstract_v<Streaming> && std::is_base_of_v<CStreaming, Streaming>);
    static_assert(std::is_abstract_v<CModelInfoRequestDE>);
    static_assert(!std::is_copy_constructible_v<Model> && !std::is_move_constructible_v<Model>);
    static_assert(std::is_nothrow_constructible_v<Model, std::uint32_t, NativeModelClassificationAccess&, CGame&, CStreaming&, NativeContext&>);

    void RequestChecks()
    {
        const char tag[] = "borrowed tag identity";
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            Check(sdk.GetRefCount() == 0 && s.native.effects.empty() && !s.native.usableReads && !s.game.streamingReads && !s.game.asyncReads,
                  "model construction/refcount query performs no native work and starts original wrapper count at zero");
            sdk.Request(NON_BLOCKING, tag);
            Check(s.native.dependencies == 1 && s.native.dependencyType == NON_BLOCKING && s.native.dependencyTag == tag && s.streamingOwner.requests == 1 &&
                      s.streamingOwner.lastModel == 45 && s.streamingOwner.lastFlags == 0x06 && s.streamingOwner.priorities.empty() && !s.native.reports &&
                      sdk.GetRefCount() == 0,
                  "actual Request dispatches dependencies then the real streaming SDK once with exact nonblocking flags/tag, without acquiring wrapper "
                  "references");
            s.streamingOwner.loaded = true;
            const auto requests = s.streamingOwner.requests;
            const auto reads = s.game.streamingReads;
            sdk.Request(BLOCKING, tag);
            Check(s.streamingOwner.requests == requests && s.game.streamingReads == reads && s.native.dependencies == 1,
                  "already usable model returns before dependencies, service lookup, requests or blocking loads");
            s.native.renderReady = false;
            Check(!sdk.IsLoaded(), "genuine raw streaming readiness cannot replace the required usable-render-data gate");
        }
        {
            Scenario    s(1000);
            CModelInfo& sdk = s.model;
            sdk.Request(NON_BLOCKING, tag);
            Check(s.streamingOwner.upgradeQueries == 1 && s.streamingOwner.upgradeRequests == 1 && !s.streamingOwner.requests &&
                      s.streamingOwner.lastModel == 1000 && s.streamingOwner.lastFlags == 0x06,
                  "model request uses actual CStreamingDE paired upgrade dispatch rather than an ordinary selected-row substitute");
        }
        {
            Scenario s;
            s.streamingOwner.completeAfterLoads = 1;
            CModelInfo& sdk = s.model;
            sdk.Request(BLOCKING, tag);
            Check(s.streamingOwner.lastFlags == 0x16 && s.streamingOwner.priorities == std::vector<bool>{true} && !s.native.reports,
                  "immediate blocking completion uses exact request flags and priority load once without delayed report");
        }
        {
            Scenario s;
            s.streamingOwner.completeAfterLoads = 4;
            CModelInfo& sdk = s.model;
            sdk.Request(BLOCKING, tag);
            Check(s.streamingOwner.priorities == std::vector<bool>({true, true, true, true}) && s.native.reports == 1 && s.native.reportLoaded &&
                      s.native.reportAttempts == 4 && s.native.reportTag == tag,
                  "actual delayed blocking completion preserves shared postincrement report and exact tag identity");
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            sdk.Request(BLOCKING, nullptr);
            Check(s.streamingOwner.priorities == std::vector<bool>({true, true, true, true, false, true, false, true, false, true, false}) &&
                      s.native.reports == 1 && !s.native.reportLoaded && s.native.reportAttempts == 11 && !s.native.reportTag &&
                      s.streamingOwner.queries == 13 && sdk.GetRefCount() == 0,
                  "actual exhausted blocking request preserves ten retries, priority sequence, short-circuit queries and11-attempt report");
        }
        for (int failed : {1, 2})
        {
            Scenario s;
            s.native.failPhase = failed;
            FailsAt([&] { static_cast<CModelInfo&>(s.model).Request(NON_BLOCKING, tag); }, failed);
            Check(!s.streamingOwner.requests && s.streamingOwner.priorities.empty() && !s.native.reports && s.model.GetRefCount() == 0,
                  "required readiness/dependency refusal stops before selected streaming effect and leaves CPU count unchanged");
        }
        {
            Scenario s;
            s.game.streaming = nullptr;
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); });
            Check(s.native.dependencies == 1 && !s.streamingOwner.requests && !s.native.reports,
                  "missing real game's streaming receiver retains dependency prefix and cannot silently succeed");
        }
        {
            Scenario s;
            s.streamingOwner.requestStatus = ClockResult::BindingChanged;
            Refuses([&] { s.model.Request(BLOCKING, tag); });
            Check(s.streamingOwner.requests == 1 && s.streamingOwner.priorities.empty() && !s.native.reports,
                  "real streaming request refusal exits once before any load/retry/report");
        }
        {
            Scenario s;
            s.streamingOwner.loadStatus = ClockResult::OriginalException;
            Refuses([&] { s.model.Request(BLOCKING, tag); });
            Check(s.streamingOwner.requests == 1 && s.streamingOwner.priorities.size() == 1 && !s.native.reports,
                  "entered real streaming completion refusal is not replayed by blocking retry logic");
        }
        {
            Scenario s;
            s.native.failPhase = 5;
            FailsAt([&] { s.model.Request(BLOCKING, tag); }, 5);
            Check(s.streamingOwner.priorities.size() == 11 && s.native.reports == 1 && s.model.GetRefCount() == 0,
                  "required report failure preserves the completed request/load prefix and propagates once");
        }
    }

    void ReferenceChecks()
    {
        const char tag[] = "reference tag";
        {
            Scenario s;
            s.streamingOwner.loaded = true;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            sdk.ModelAddRef(BLOCKING, tag);
            Check(sdk.GetRefCount() == 2 && s.native.increments == 1 && !s.game.asyncReads && !s.streamingOwner.requests,
                  "one mandatory native interface reference backs two wrapper refs; SDKcount does not expose native counter value");
            auto reads = s.native.usableReads;
            sdk.RemoveRef();
            Check(sdk.GetRefCount() == 1 && !s.native.removals && s.native.usableReads == reads,
                  "interior wrapper decrement needs no nativecount/removal/query effect");
            sdk.RemoveRef();
            Check(sdk.GetRefCount() == 0 && s.native.removals == 1,
                  "final non-CJ wrapper decrement invokes required custom-aware backend removal once after loaded query");
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            Check(sdk.GetRefCount() == 1 && !s.native.increments && s.streamingOwner.requests == 1 && s.streamingOwner.lastFlags == 0x06 &&
                      s.game.asyncReads == 1 && s.native.dependencyTag == tag,
                  "unloaded async AddRef commits pending CPU reference after actual dependency/request acceptance");
            s.streamingOwner.loaded = true;
            Check(sdk.IsLoaded() && s.native.increments == 1 && sdk.GetRefCount() == 1,
                  "usable transition acknowledges the pending genuine interface increment before clearing pending");
            Check(sdk.IsLoaded() && s.native.increments == 1, "pending acknowledgement is consumed once and does not replay native increment");
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            auto reads = s.native.usableReads;
            sdk.RemoveRef();
            Check(sdk.GetRefCount() == 0 && !s.native.increments && !s.native.removals && s.native.usableReads == reads,
                  "pending cancellation clears CPU reference before any native observation/removal");
            s.streamingOwner.loaded = true;
            Check(sdk.IsLoaded() && !s.native.increments, "cancelled pending reference cannot later acquire a native counter");
        }
        {
            Scenario s;
            s.game.async = false;
            s.streamingOwner.completeAfterLoads = 1;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            Check(sdk.GetRefCount() == 1 && s.native.increments == 1 && s.streamingOwner.lastFlags == 0x16 && s.native.dependencyType == BLOCKING &&
                      s.streamingOwner.priorities == std::vector<bool>{true},
                  "actual borrowed CGame async policy forces genuine blocking Request before committing first wrapper reference");
        }
        {
            Scenario s(0);
            s.streamingOwner.loaded = true;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            auto reads = s.native.usableReads;
            sdk.RemoveRef();
            Check(sdk.GetRefCount() == 0 && !s.native.removals && s.native.usableReads == reads,
                  "actual shared CJ/model0 exception bypasses the final loaded/native removal path");
        }
        {
            Scenario s;
            s.streamingOwner.loaded = true;
            s.native.failPhase = 3;
            CModelInfo& sdk = s.model;
            FailsAt([&] { sdk.ModelAddRef(NON_BLOCKING, tag); }, 3);
            Check(sdk.GetRefCount() == 0 && s.native.increments == 1 && s.native.retired,
                  "entered native increment failure precedes CPU countcommit and retains accepted backend effect/retiredowner");
            FailsAt([&] { sdk.ModelAddRef(NON_BLOCKING, tag); }, 1);
            Check(s.native.increments == 1 && sdk.GetRefCount() == 0, "subsequent required owner refusal prevents replay after entered increment failure");
        }
        {
            Scenario    s;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            s.streamingOwner.loaded = true;
            s.native.failPhase = 3;
            FailsAt([&] { static_cast<void>(sdk.IsLoaded()); }, 3);
            Check(sdk.GetRefCount() == 1 && s.native.increments == 1 && s.native.retired,
                  "pending acknowledgement failure preserves CPU reference and original pending/nativecommit boundary");
            FailsAt([&] { static_cast<void>(sdk.IsLoaded()); }, 1);
            Check(s.native.increments == 1, "retired required owner prevents pending acknowledgement replay");
        }
        {
            Scenario s;
            s.streamingOwner.loaded = true;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            s.native.failPhase = 4;
            FailsAt([&] { sdk.RemoveRef(); }, 4);
            Check(sdk.GetRefCount() == 0 && s.native.removals == 1 && s.native.retired,
                  "native removal failure keeps the preceding classic CPUdecrement and accepted effect without rollback");
            FailsAt([&] { sdk.RemoveRef(); }, 1);
            Check(s.native.removals == 1, "retired backend prevents replay of failed custom-aware removal");
        }
        {
            Scenario s;
            s.streamingOwner.loaded = true;
            CModelInfo& sdk = s.model;
            sdk.RemoveRef();
            Check(sdk.GetRefCount() == 0 && s.native.removals == 1 && !s.native.increments,
                  "zero CPU count is not a hidden once gate: preserve the classic non-CJ loaded removal condition");
        }
        {
            Scenario s;
            s.streamingOwner.loaded = true;
            CModelInfo& sdk = s.model;
            sdk.ModelAddRef(NON_BLOCKING, tag);
            s.native.failPhase = 1;
            FailsAt([&] { sdk.RemoveRef(); }, 1);
            Check(sdk.GetRefCount() == 0 && !s.native.removals,
                  "required loaded-query refusal retains the preceding classic CPUdecrement rather than adding a new early gate");
        }
        {
            Scenario s;
            s.game.asyncFailure = true;
            FailsAt([&] { s.model.ModelAddRef(NON_BLOCKING, tag); }, 8);
            Check(s.model.GetRefCount() == 0 && !s.native.dependencies && !s.streamingOwner.requests && !s.native.increments,
                  "borrowed game's async policy refusal precedes request and CPU count commit without defaulting to blocking/success");
        }
        {
            Scenario s;
            s.streamingOwner.requestStatus = ClockResult::NotInitialized;
            Refuses([&] { s.model.ModelAddRef(NON_BLOCKING, tag); });
            Check(s.model.GetRefCount() == 0 && !s.native.increments && !s.native.removals,
                  "request refusal precedes pending/wrappercount commit and invokes no native reference shortcut");
        }
    }
}

int main()
{
    RequestChecks();
    ReferenceChecks();
    std::puts("Production actual model request/reference SDK fixture completed");
    std::printf("Passed %u actual model request SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
