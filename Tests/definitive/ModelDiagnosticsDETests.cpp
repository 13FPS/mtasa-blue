/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Test the actual Core-bound model diagnostic partial SDK layer
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDiagnosticsDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <functional>
#include <memory>
// These unchanged Core SDK headers normally inherit the classic PCH's
// warning policy. Contain that compatibility boundary here; adapter and test
// bodies retain the caller's strict warning level after the include returns.
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
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached Core-bound model diagnostic fixture\n", stderr);
        std::abort();
    }
}
#include "ModelDiagnosticsFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool result, const char* why)
    {
        ++checks;
        if (!result)
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }
    enum class Kind
    {
        Bind,
        Read,
        Streaming,
        Loaded,
        Archive,
        Type,
        Animation,
        BaseIfp,
        Lookup,
        Request,
        Load,
        BaseTxd,
        Core
    };
    struct Event
    {
        Kind          kind;
        std::uint32_t key = 0, value = 0;
    };
    struct Trace
    {
        std::array<Event, 96> events{};
        std::size_t           count = 0;
        void                  Add(Kind kind, std::uint32_t key = 0, std::uint32_t value = 0) noexcept
        {
            if (count == events.size())
                fixture::Unqualified();
            events[count++] = {kind, key, value};
        }
        unsigned Count(Kind kind) const noexcept
        {
            unsigned result = 0;
            for (std::size_t i = 0; i < count; ++i)
                if (events[i].kind == kind)
                    ++result;
            return result;
        }
    };
    struct CoreFailure : std::runtime_error
    {
        CoreFailure() : std::runtime_error("borrowed real Core reporter refused") {}
    };
    class Core final : public fixture::CoreFixtureBase
    {
    public:
        struct Entry
        {
            uint        debugId = 0, reportId = 0;
            std::string type, context, body;
        };
        explicit Core(Trace& trace) noexcept : trace(trace) {}
        void LogEvent(uint debugId, const char* type, const char* context, const char* body, uint reportId) override
        {
            trace.Add(Kind::Core, debugId, reportId);
            if (count == entries.size())
                fixture::Unqualified();
            entries[count++] = {debugId, reportId, type, context, body};
            if (fail)
                throw CoreFailure();
            if (assertWitness)
            {
                Check(debugId == 641 && reportId == 6641 && std::string(type) == "Blocking load fail" && std::string(context) == "Blocking load fail" &&
                          std::string(body) == "400 (assert scope)",
                      "failure assertion receives the actual Core event/report payload first");
                std::puts("Actual Core641 report6641 callback completed before dassert");
                std::fflush(stdout);
            }
        }
        std::array<Entry, 8> entries{};
        std::size_t          count = 0;
        bool                 fail = false, assertWitness = false;

    private:
        Trace& trace;
    };
    class Owner final : public NativeModelClassificationAccess, public NativeModelBackingReferenceAccess
    {
    public:
        explicit Owner(Trace& trace) noexcept : trace(trace) {}
        NativeModelBackingReferenceToken Issue(std::uint32_t key) noexcept
        {
            issued = {7, 11, key};
            return issued;
        }
        ClockResult BindModelBackingReference(const NativeModelBackingReferenceToken& token) noexcept override
        {
            trace.Add(Kind::Bind, token.modelIndex);
            return Same(token) ? ClockResult::Ok : ClockResult::OwnerConflict;
        }
        ClockResult ReadModelBackingReference(const NativeModelBackingReferenceToken& token, NativeModelBackingReferenceState& output) noexcept override
        {
            trace.Add(Kind::Read, token.modelIndex);
            if (!Same(token))
                return ClockResult::OwnerConflict;
            output.reference = token;
            output.phase = NativeModelBackingReferencePhase::Bound;
            return ClockResult::Ok;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t key, bool& output) noexcept override
        {
            trace.Add(Kind::Archive, key);
            output = true;
            return ClockResult::Ok;
        }
        ClockResult ReadModelType(std::uint32_t key, NativeModelTypeObservation& output) noexcept override
        {
            trace.Add(Kind::Type, key);
            output = {true, 6};
            return ClockResult::Ok;
        }
        ClockResult ReadModelAnimFileIndex(std::uint32_t key, std::uint32_t& output) noexcept override
        {
            trace.Add(Kind::Animation, key);
            output = 42;
            return ClockResult::Ok;
        }
        unsigned    acquires = 0;
        ClockResult AcquireModelBackingReference(const NativeModelBackingReferenceToken&) noexcept override
        {
            ++acquires;
            return ClockResult::NotInitialized;
        }

    private:
        bool Same(const NativeModelBackingReferenceToken& token) const noexcept
        {
            return token.ownerGeneration == issued.ownerGeneration && token.serial == issued.serial && token.modelIndex == issued.modelIndex;
        }
        Trace&                           trace;
        NativeModelBackingReferenceToken issued{};
    };
    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        explicit StreamingOwner(Trace& trace) noexcept : trace(trace) {}
        ClockResult ReadStreamingModelLoaded(std::uint32_t key, NativeModelLoadedReader::Value& output) noexcept override
        {
            trace.Add(Kind::Loaded, key);
            NativeModelLoadedReader::Value value;
            value.streamedIn = ready;
            output = value;
            return ClockResult::Ok;
        }
        ClockResult RequestStreamingModel(std::uint32_t key, std::uint32_t flags) noexcept override
        {
            trace.Add(Kind::Request, key, flags);
            return ClockResult::Ok;
        }
        ClockResult LoadStreamingModels(bool priority) noexcept override
        {
            trace.Add(Kind::Load, 0, priority ? 1u : 0u);
            ready = ++loads == 2;
            return ClockResult::Ok;
        }
        bool     ready = false;
        unsigned loads = 0;

    private:
        Trace& trace;
    };
    class Streaming final : public fixture::StreamingFixtureBase
    {
    public:
        explicit Streaming(NativeStreamingModelAccess& owner) noexcept : StreamingFixtureBase(owner) {}
    };
    class Game final : public fixture::GameFixtureBase
    {
    public:
        explicit Game(Trace& trace) noexcept : trace(trace) {}
        CStreaming* GetStreaming() override
        {
            trace.Add(Kind::Streaming);
            return streaming;
        }
        std::uint32_t GetBaseIDforTXD() override
        {
            trace.Add(Kind::BaseTxd);
            return 20000;
        }
        std::uint32_t GetBaseIDforIFP() override
        {
            trace.Add(Kind::BaseIfp);
            return 30000;
        }
        CModelInfo* GetModelInfo(DWORD key, bool invalid) override
        {
            trace.Add(Kind::Lookup, key, invalid ? 1u : 0u);
            return nullptr;
        }
        CStreaming* streaming = nullptr;

    private:
        Trace& trace;
    };
    class Model final : public fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t key, NativeModelClassificationAccess& classification, CGame& game, NativeModelBackingReferenceAccess& owner,
              const NativeModelBackingReferenceToken& reference, CCoreInterface& core)
            : ModelFixtureBase(key, classification, game, owner, reference, core)
        {
        }
        using CModelInfoDiagnosticsDE::ReportBlockingModelLoad;
        using CModelInfoDiagnosticsDE::ReportMissingAnimationModel;
#ifdef MTA_DEBUG
        using CModelInfoDiagnosticsDE::ReportRequestingAnimationModel;
#endif
    protected:
        void RemoveNativeModelInterfaceReference() override { fixture::Unqualified(); }
    };
    struct Scenario
    {
        Trace                            trace;
        Core                             core{trace};
        Owner                            owner{trace};
        StreamingOwner                   streamingOwner{trace};
        Streaming                        streaming{streamingOwner};
        Game                             game{trace};
        NativeModelBackingReferenceToken reference;
        Model                            model;
        explicit Scenario(std::uint32_t key = 400) : reference(owner.Issue(key)), model(key, owner, game, owner, reference, core)
        {
            game.streaming = &streaming;
        }
    };
    static_assert(!std::is_abstract_v<Model> && !std::is_abstract_v<Core> && !std::is_abstract_v<Game> && !std::is_abstract_v<Streaming>);
    static_assert(std::is_same_v<decltype(&Model::Request), void (CModelInfoRequestDE::*)(EModelRequestType, const char*)>);
    static_assert(std::is_same_v<decltype(&Model::ReportMissingAnimationModel), void (CModelInfoDiagnosticsDE::*)(std::uint32_t)>);
    static_assert(std::is_same_v<decltype(&Model::ReportBlockingModelLoad), void (CModelInfoDiagnosticsDE::*)(bool, int, const char*)>);
    template <class Operation>
    void CoreRefuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const CoreFailure&)
        {
            Check(true, "original Core refusal propagates without a retry or fallback logger");
            return;
        }
        Check(false, "Core refusal cannot be converted to a successful report");
    }
    void PayloadChecks()
    {
        Scenario s;
        Check(s.trace.count == 1 && s.trace.events[0].kind == Kind::Bind && s.core.count == 0,
              "construction only binds original reference owner and never queries or logs");
        s.model.ReportMissingAnimationModel(0xffffffffu);
        const auto& missing = s.core.entries[0];
        Check(
            missing.debugId == 505 && missing.reportId == 0 && missing.type == "Model no anim" && missing.context.empty() && missing.body == "400 (4294967295)",
            "actual missing animation diagnostic uses Core event505 with full unsigned animation payload and no reportID");
        const char tag[] = "tag%literal/UTF8-\xc3\xa9";
        s.model.ReportBlockingModelLoad(true, 11, tag);
        const auto& success = s.core.entries[1];
        Check(success.debugId == 642 && success.reportId == 6642 && success.type == "Blocking load" && success.context == "Blocking load" &&
                  success.body == std::string("400 (") + tag + ") (Took 11 attempts)",
              "success preserves event/report IDs, full tag and attempt count in one actual Core call");
        s.model.ReportBlockingModelLoad(true, -1, nullptr);
        Check(s.core.entries[2].body == "400 () (Took -1 attempts)", "diagnostic formatting does not interpret tag format specifiers or invent a missing tag");
#ifdef MTA_DEBUG
        s.model.ReportRequestingAnimationModel(30042);
        const auto& debug = s.core.entries[3];
        Check(debug.debugId == 0 && debug.reportId == 0 && debug.type == "Models" && debug.context.empty() &&
                  debug.body == "Requesting anim file 30042 for model 400",
              "conditional requesting-animation notice uses the existing unfiltered Core diagnostic service");
#endif
        Check(s.trace.Count(Kind::Read) == 0 && s.trace.Count(Kind::Request) == 0 && s.owner.acquires == 0,
              "borrowed reporter grants no model lifetime, query, request or native reference right");
    }
    void RefusalChecks()
    {
        for (unsigned operation = 0; operation != 3; ++operation)
        {
            Scenario s;
            s.core.fail = true;
            CoreRefuses(
                [&]
                {
                    if (operation == 0)
                        s.model.ReportMissingAnimationModel(30042);
                    else
                        s.model.ReportBlockingModelLoad(operation == 1, 11, "refused tag");
                });
            Check(s.core.count == 1 && s.trace.Count(Kind::Core) == 1, "entered Core failure is never replayed by the partial SDK layer");
            Check(s.owner.acquires == 0 && s.trace.Count(Kind::Read) == 0, "Core callback failure supplies no native operation authority");
        }
        Scenario   s;
        Owner      differentOwner(s.trace);
        const auto token = differentOwner.Issue(400);
        try
        {
            Model invalid(400, s.owner, s.game, differentOwner, token, s.core);
            Check(false, "Core borrow must not bypass complete issuing owner identity");
        }
        catch (const std::invalid_argument&)
        {
            Check(s.core.count == 0 && s.trace.Count(Kind::Bind) == 1,
                  "existing reference-token owner validation still precedes partial diagnostics construction");
        }
    }
    void PublicRequestChecks()
    {
        for (bool refuse : {false, true})
        {
            Scenario s;
            s.trace.count = 0;
            s.core.fail = refuse;
            if (refuse)
                CoreRefuses([&] { s.model.Request(NON_BLOCKING, "missing dependency"); });
            else
                s.model.Request(NON_BLOCKING, "missing dependency");
            Check(s.core.count == 1 && s.core.entries[0].debugId == 505 && s.core.entries[0].body == "400 (30042)",
                  "actual inherited dependency policy reaches real bound Core505 override");
            Check(s.trace.Count(Kind::Request) == (refuse ? 0u : 1u), "parent native request occurs only after actual missing diagnostic returns");
            Check(s.trace.events[s.trace.count - 1].kind == (refuse ? Kind::Core : Kind::Request),
                  "Core callback is the exact failure prefix or precedes parent native request");
            Check(s.trace.Count(Kind::Read) == 1 && s.owner.acquires == 0, "real ReferenceDE owner gate remains active without acquiring a reference");
        }
        {
            Scenario s(30000);
            s.trace.count = 0;
            s.model.Request(BLOCKING, "selected delayed load");
            Check(s.trace.Count(Kind::Request) == 1 && s.trace.Count(Kind::Load) == 2,
                  "one selected actual blocking Request reaches source reporting after one delayed completion");
            for (std::size_t i = 0; i < s.trace.count; ++i)
            {
                if (s.trace.events[i].kind == Kind::Request)
                    Check(s.trace.events[i].key == 30000 && s.trace.events[i].value == 0x16, "actual parent blocking native request retains original flags");
                if (s.trace.events[i].kind == Kind::Load)
                    Check(s.trace.events[i].value == 1, "selected source load completion retains priority intent");
            }
            Check(s.core.count == 1 && s.core.entries[0].debugId == 642 && s.core.entries[0].reportId == 6642 &&
                      s.core.entries[0].body == "30000 (selected delayed load) (Took 2 attempts)",
                  "actual request report preserves postincrement attempt semantics and borrowed tag");
            Check(s.trace.events[s.trace.count - 1].kind == Kind::Core && s.model.GetRefCount() == 0 && s.owner.acquires == 0,
                  "source Core report ends request without manufacturing wrapper/native references");
        }
    }
}
int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--debug-assert") == 0)
    {
#ifdef MTA_DEBUG
        // Keep the genuine CRT assertion while making this owned negative
        // process noninteractive. The parent verifies Core's flushed witness.
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        Scenario s;
        s.core.assertWitness = true;
        s.model.ReportBlockingModelLoad(false, 11, "assert scope");
        std::puts("FAIL: genuine failed-load dassert returned");
        return EXIT_FAILURE;
#else
        return EXIT_FAILURE;
#endif
    }
    if (argc != 1)
        return EXIT_FAILURE;
    PayloadChecks();
    RefusalChecks();
    PublicRequestChecks();
    std::puts("Production actual Core-bound model diagnostics SDK fixture completed");
    std::printf("Passed %u actual model diagnostics SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
