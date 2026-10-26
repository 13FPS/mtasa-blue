/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual MTA dependency policy through its required SDK receivers
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoRequestDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <vector>
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached dependency fixture\n", stderr);
        std::abort();
    }
}
#include "ModelRequestFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool result, const char* reason)
    {
        ++checks;
        if (!result)
        {
            std::fprintf(stderr, "FAIL: %s\n", reason);
            std::exit(EXIT_FAILURE);
        }
    }
    enum class Kind
    {
        Streaming,
        Loaded,
        BaseTxd,
        Archive,
        Type,
        Animation,
        BaseIfp,
        Lookup,
        ChildLoaded,
        ChildRequest,
        Request,
        Load,
        Interface,
        Render,
        Report505
    };
    struct Event
    {
        Kind          kind;
        std::uint32_t key = 0, value = 0;
        const void*   identity = nullptr;
    };
    struct Trace
    {
        std::array<Event, 96> events{};
        std::size_t           count = 0;
        void                  Add(Kind kind, std::uint32_t key = 0, std::uint32_t value = 0, const void* identity = nullptr) noexcept
        {
            if (count == events.size())
                fixture::Unqualified();
            events[count++] = {kind, key, value, identity};
        }
        std::vector<Event> Of(Kind kind) const
        {
            std::vector<Event> found;
            for (std::size_t i = 0; i != count; ++i)
                if (events[i].kind == kind)
                    found.push_back(events[i]);
            return found;
        }
        void Is(std::initializer_list<Kind> expected) const
        {
            Check(count == expected.size(), "actual source sequence has no extra service or native request");
            std::size_t i = 0;
            for (auto kind : expected)
                Check(events[i++].kind == kind, "actual dependency observation order matches source");
        }
    };
    struct RequiredFailure : std::runtime_error
    {
        explicit RequiredFailure(const char* reason) : std::runtime_error(reason) {}
    };
    template <class Operation>
    void Refuses(Operation operation, const char* message)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error& failure)
        {
            Check(std::string(failure.what()).find(message) != std::string::npos, "original refusal survives the SDK boundary");
            return;
        }
        Check(false, "required dependency service cannot silently succeed");
    }
    class Owner final : public NativeModelClassificationAccess
    {
    public:
        explicit Owner(Trace& trace) noexcept : trace(trace) {}
        ClockResult ReadModelArchiveAllocated(std::uint32_t key, bool& output) noexcept override
        {
            trace.Add(Kind::Archive, key);
            output = archive;
            return ClockResult::Ok;
        }
        ClockResult ReadModelType(std::uint32_t key, NativeModelTypeObservation& output) noexcept override
        {
            trace.Add(Kind::Type, key);
            if (typeStatus != ClockResult::Ok)
                return typeStatus;
            output = {true, nativeType};
            return ClockResult::Ok;
        }
        ClockResult ReadModelAnimFileIndex(std::uint32_t key, std::uint32_t& output) noexcept override
        {
            trace.Add(Kind::Animation, key);
            if (animationStatus != ClockResult::Ok)
                return animationStatus;
            output = animation;
            return ClockResult::Ok;
        }
        ClockResult ReadModelInterface(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            trace.Add(Kind::Interface, key);
            output = 1;
            return ClockResult::Ok;
        }
        ClockResult ReadModelRender(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            trace.Add(Kind::Render, key);
            output = 3;
            return ClockResult::Ok;
        }
        bool          archive = true;
        unsigned char nativeType = 6;
        std::uint32_t animation = 0xffffffffu;
        ClockResult   animationStatus = ClockResult::Ok, typeStatus = ClockResult::Ok;

    private:
        Trace& trace;
    };
    class StreamingOwner final : public NativeStreamingModelAccess
    {
    public:
        explicit StreamingOwner(Trace& trace) noexcept : trace(trace) {}
        ClockResult ReadStreamingModelLoaded(std::uint32_t key, NativeModelLoadedReader::Value& output) noexcept override
        {
            trace.Add(Kind::Loaded, key);
            if (key == refusedLoadedKey)
                return ClockResult::OwnerConflict;
            NativeModelLoadedReader::Value value;
            for (auto ready : readyKeys)
                if (key == ready)
                    value.streamedIn = true;
            output = value;
            return ClockResult::Ok;
        }
        ClockResult RequestStreamingModel(std::uint32_t key, std::uint32_t flags) noexcept override
        {
            trace.Add(Kind::Request, key, flags);
            return key == refusedRequestKey ? ClockResult::OwnerConflict : ClockResult::Ok;
        }
        ClockResult LoadStreamingModels(bool priority) noexcept override
        {
            trace.Add(Kind::Load, 0, priority ? 1u : 0u);
            return ClockResult::Ok;
        }
        std::vector<std::uint32_t> readyKeys;
        std::uint32_t              refusedLoadedKey = 0xffffffffu, refusedRequestKey = 0xffffffffu;

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
            const auto value = baseCount < baseResults.size() ? baseResults[baseCount++] : baseIfp;
            trace.Add(Kind::BaseIfp, value);
            if (baseFailure)
                throw RequiredFailure("base IFP unavailable");
            return value;
        }
        CModelInfo* GetModelInfo(DWORD key, bool canBeInvalid) override
        {
            trace.Add(Kind::Lookup, key, canBeInvalid ? 1u : 0u);
            if (lookupFailure)
                throw RequiredFailure("model facade lookup refused");
            if (lookupCount == lookupResults.size())
                fixture::Unqualified();
            return lookupResults[lookupCount++];
        }
        CStreaming*                streaming = nullptr;
        std::uint32_t              baseIfp = 30000;
        std::vector<std::uint32_t> baseResults;
        std::size_t                baseCount = 0;
        bool                       baseFailure = false, lookupFailure = false;
        std::vector<CModelInfo*>   lookupResults;
        std::size_t                lookupCount = 0;

    private:
        Trace& trace;
    };
    class Model : public fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t key, NativeModelClassificationAccess& owner, CGame& game) noexcept : ModelFixtureBase(key, owner, game) {}
        using CModelInfoDE::GetAnimFileIndex;
        using CModelInfoRequestDE::RequestModelDependencies;

    protected:
        void ReportBlockingModelLoad(bool, int, const char*) override { fixture::Unqualified(); }
        void IncrementNativeModelInterfaceReference() override { fixture::Unqualified(); }
        void RemoveNativeModelInterfaceReference() override { fixture::Unqualified(); }
    };
    class ReportingModel final : public Model
    {
    public:
        ReportingModel(std::uint32_t key, NativeModelClassificationAccess& owner, CGame& game, Trace& trace) noexcept : Model(key, owner, game), trace(trace) {}
        bool refuseReport = false;

    protected:
        void ReportMissingAnimationModel(std::uint32_t key) override
        {
            trace.Add(Kind::Report505, key);
            if (refuseReport)
                throw RequiredFailure("actual Core505 reporter refused");
        }

    private:
        Trace& trace;
    };
    // The lookup returns an existing independent SDK facade. Its required
    // virtual methods record policy arguments without replacing the parent's
    // Request, dependency algorithm, loaded gate or native animation getter.
    class Dependency final : public fixture::DependencyFixtureBase
    {
    public:
        Dependency(Trace& trace, std::uint32_t identity) noexcept : trace(trace), identity(identity) {}
        bool IsLoaded() override
        {
            trace.Add(Kind::ChildLoaded, identity);
            if (refuseLoaded)
                throw RequiredFailure("dependency readiness refused");
            return loaded;
        }
        void Request(EModelRequestType type, const char* tag) override
        {
            trace.Add(Kind::ChildRequest, identity, static_cast<std::uint32_t>(type), tag);
            if (refuseRequest)
                throw RequiredFailure("dependency request refused");
        }
        bool loaded = false, refuseLoaded = false, refuseRequest = false;

    private:
        Trace&        trace;
        std::uint32_t identity;
    };
    struct Scenario
    {
        Trace          trace;
        Owner          owner{trace};
        StreamingOwner streamingOwner{trace};
        Streaming      streaming{streamingOwner};
        Game           game{trace};
        Model          model;
        explicit Scenario(std::uint32_t key = 400) noexcept : model(key, owner, game) { game.streaming = &streaming; }
    };
    static_assert(!std::is_abstract_v<Model> && !std::is_abstract_v<Dependency> && !std::is_abstract_v<Game> && !std::is_abstract_v<Streaming>);
    static_assert(std::is_same_v<decltype(&Model::Request), void (CModelInfoRequestDE::*)(EModelRequestType, const char*)>);
    static_assert(std::is_same_v<decltype(&Model::RequestModelDependencies), void (CModelInfoRequestDE::*)(EModelRequestType, const char*)>);
    static_assert(std::is_same_v<decltype(&Model::GetAnimFileIndex), std::uint32_t (CModelInfoDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::IsLoaded), bool (CModelInfoRequestDE::*)()>);

    void ReadyAndVehicleChecks()
    {
        const char tag[] = "caller-owned dependency tag";
        {
            Scenario s;
            s.streamingOwner.readyKeys = {400};
            s.model.Request(NON_BLOCKING, tag);
            s.trace.Is({Kind::Streaming, Kind::Loaded, Kind::BaseTxd, Kind::Interface, Kind::Render});
            Check(s.trace.Of(Kind::Animation).empty() && s.trace.Of(Kind::Request).empty(), "ready actual model skips dependency and parent native request");
        }
        {
            Scenario s;
            s.model.Request(NON_BLOCKING, tag);
            s.trace.Is({Kind::Streaming, Kind::Loaded, Kind::Archive, Kind::Type, Kind::Animation, Kind::Streaming, Kind::Request});
            const auto requests = s.trace.Of(Kind::Request);
            Check(requests[0].key == 400 && requests[0].value == 6, "FFFFFFFF native animation sentinel skips base/lookup then parent requests once");
        }
        for (auto type : {NON_BLOCKING, BLOCKING})
        {
            Scenario   s;
            Dependency child(s.trace, 91);
            s.owner.animation = 0x80000023u;
            s.game.baseIfp = 0x90000010u;
            s.game.lookupResults = {&child};
            s.model.RequestModelDependencies(type, tag);
            s.trace.Is({Kind::Archive, Kind::Type, Kind::Animation, Kind::BaseIfp, Kind::Lookup, Kind::ChildLoaded, Kind::ChildRequest});
            const auto lookup = s.trace.Of(Kind::Lookup)[0], request = s.trace.Of(Kind::ChildRequest)[0];
            Check(lookup.key == 0x10000033u && lookup.value == 0, "full uint32 addition wraps once and lookup uses original false default");
            Check(request.value == static_cast<std::uint32_t>(type) && request.identity == tag,
                  "dependency receives exact original type and borrowed tag pointer");
        }
        {
            Scenario   s;
            Dependency child(s.trace, 92);
            child.loaded = true;
            s.owner.animation = 42;
            s.game.lookupResults = {&child};
            s.model.Request(NON_BLOCKING, tag);
            s.trace.Is({Kind::Streaming, Kind::Loaded, Kind::Archive, Kind::Type, Kind::Animation, Kind::BaseIfp, Kind::Lookup, Kind::ChildLoaded,
                        Kind::Streaming, Kind::Request});
            Check(s.trace.Of(Kind::Lookup)[0].key == 30042, "existing loaded animation uses current IFP base");
            Check(s.trace.Of(Kind::ChildRequest).empty(), "already loaded dependency is not requested");
        }
        {
            Scenario s;
            Model    child(30042, s.owner, s.game);
            s.owner.animation = 42;
            s.game.lookupResults = {&child};
            s.model.Request(NON_BLOCKING, tag);
            s.trace.Is({Kind::Streaming, Kind::Loaded, Kind::Archive, Kind::Type, Kind::Animation, Kind::BaseIfp, Kind::Lookup, Kind::Streaming, Kind::Loaded,
                        Kind::Streaming, Kind::Loaded, Kind::Streaming, Kind::Request, Kind::Streaming, Kind::Request});
            const auto requests = s.trace.Of(Kind::Request);
            Check(requests.size() == 2 && requests[0].key == 30042 && requests[1].key == 400 && requests[0].value == 6 && requests[1].value == 6,
                  "actual nested child Request/IsLoaded runs before actual parent native request with original asynchronous flags");
        }
    }
    void MissingAnimationChecks()
    {
        const char tag[] = "missing animation tag";
        {
            Scenario s;
            s.owner.animation = 139;
            s.game.lookupResults = {nullptr};
            s.model.Request(NON_BLOCKING, tag);
            s.trace.Is({Kind::Streaming, Kind::Loaded, Kind::Archive, Kind::Type, Kind::Animation, Kind::BaseIfp, Kind::Lookup, Kind::BaseIfp, Kind::Streaming,
                        Kind::Request});
            Check(s.trace.Of(Kind::Report505).empty(), "only source baseIFP plus139 suppresses missing dependency report");
        }
        {
            Scenario s;
            s.owner.animation = 140;
            s.game.lookupResults = {nullptr};
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); }, "report505");
            s.trace.Is({Kind::Streaming, Kind::Loaded, Kind::Archive, Kind::Type, Kind::Animation, Kind::BaseIfp, Kind::Lookup, Kind::BaseIfp});
            Check(s.trace.Of(Kind::Request).empty(), "default missing505 hook refuses before parent request");
            std::puts("Actual default Core505 reporter refusal covered");
        }
        for (bool refuse : {false, true})
        {
            Scenario       s;
            ReportingModel model(400, s.owner, s.game, s.trace);
            model.refuseReport = refuse;
            s.owner.animation = 0xfffffff0u;
            s.game.baseIfp = 0x30;
            s.game.lookupResults = {nullptr};
            if (refuse)
                Refuses([&] { model.Request(NON_BLOCKING, tag); }, "Core505 reporter refused");
            else
                model.Request(NON_BLOCKING, tag);
            const auto reports = s.trace.Of(Kind::Report505);
            Check(reports.size() == 1 && reports[0].key == 0x20, "real overridable reporter receives wrapped resolved animation ID once");
            Check(s.trace.Of(Kind::BaseIfp).size() == 2, "missing dependency makes both source IFP base observations");
            Check(s.trace.Of(Kind::Request).size() == (refuse ? 0u : 1u), "parent request follows only accepted source diagnostic");
        }
        for (bool suppress : {false, true})
        {
            Scenario s;
            s.owner.animation = suppress ? 140u : 139u;
            s.game.baseResults = {30000u, suppress ? 30001u : 31000u};
            s.game.lookupResults = {nullptr};
            if (suppress)
                s.model.RequestModelDependencies(NON_BLOCKING, tag);
            else
                Refuses([&] { s.model.RequestModelDependencies(NON_BLOCKING, tag); }, "report505");
            Check(s.trace.Of(Kind::BaseIfp).size() == 2 && s.trace.Of(Kind::Lookup)[0].key == 30000u + s.owner.animation,
                  "missing139 comparison consumes fresh second source base rather than caching first observation");
        }
    }
    void SkinChecks()
    {
        const char tag[] = "parent skin tag";
        for (auto key : {0u, 7u, 288u, 289u})
        {
            Scenario s(key);
            s.owner.archive = false;
            Dependency skin(s.trace, 7);
            skin.loaded = true;
            if (key == 0 || key == 288)
                s.game.lookupResults = {&skin};
            s.model.Request(NON_BLOCKING, tag);
            const auto lookups = s.trace.Of(Kind::Lookup);
            Check(lookups.size() == ((key == 0 || key == 288) ? 1u : 0u), "skin7 workaround includes0/288 and excludes7/289");
            if (!lookups.empty())
                Check(lookups[0].key == 7 && lookups[0].value == 0, "skin lookup is exact model7 with default false");
            Check(s.trace.Of(Kind::Animation).empty() && s.trace.Of(Kind::Type).empty(),
                  "nonvehicle archive shortcircuit performs no animation/type owner query");
            Check(s.trace.Of(Kind::Request).size() == 1, "loaded/excluded skin policy permits one parent request");
        }
        for (auto type : {NON_BLOCKING, BLOCKING})
        {
            Scenario s(288);
            s.owner.archive = false;
            Dependency observed(s.trace, 71), requested(s.trace, 72);
            s.game.lookupResults = {&observed, &requested};
            s.model.RequestModelDependencies(type, tag);
            s.trace.Is({Kind::Archive, Kind::Lookup, Kind::ChildLoaded, Kind::Lookup, Kind::ChildRequest});
            const auto lookups = s.trace.Of(Kind::Lookup), reads = s.trace.Of(Kind::ChildLoaded), requests = s.trace.Of(Kind::ChildRequest);
            Check(lookups.size() == 2 && lookups[0].key == 7 && lookups[1].key == 7, "unloaded skin resolves model7 again at original request boundary");
            Check(reads[0].key == 71 && requests[0].key == 72, "second actual lookup receiver is used without caching first facade");
            Check(requests[0].value == static_cast<std::uint32_t>(type) && std::string(static_cast<const char*>(requests[0].identity)) == "Model 7",
                  "skin request preserves original type and replaces tag with source Model7 literal");
        }
        {
            Scenario s(288);
            s.owner.nativeType = 0;
            Dependency skin(s.trace, 7);
            skin.loaded = true;
            s.game.lookupResults = {&skin};
            s.model.RequestModelDependencies(NON_BLOCKING, tag);
            s.trace.Is({Kind::Archive, Kind::Type, Kind::Lookup, Kind::ChildLoaded});
            Check(s.trace.Of(Kind::Animation).empty(), "allocated nonvehicle type follows skin policy without animation getter");
        }
        // A missing receiver at either source lookup must stop the parent request.
        for (bool second : {false, true})
        {
            Scenario s(0);
            s.owner.archive = false;
            Dependency skin(s.trace, 7);
            s.game.lookupResults = second ? std::vector<CModelInfo*>{&skin, nullptr} : std::vector<CModelInfo*>{nullptr};
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); }, second ? "lost the real skin7" : "require the real skin7");
            Check(s.trace.Of(Kind::Lookup).size() == (second ? 2u : 1u), "missing required skin receiver aborts at the exact source resolution");
            Check(s.trace.Of(Kind::Request).empty() && s.trace.Of(Kind::ChildRequest).empty(), "missing skin receiver never permits parent native request");
        }
        for (bool request : {false, true})
        {
            Scenario s(288);
            s.owner.archive = false;
            Dependency skin(s.trace, 7);
            skin.refuseLoaded = !request;
            skin.refuseRequest = request;
            s.game.lookupResults = {&skin, &skin};
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); }, request ? "dependency request refused" : "dependency readiness refused");
            Check(s.trace.Of(Kind::Request).empty(), "refused required skin effect prevents parent native request");
        }
        {
            Scenario s(0);
            s.owner.archive = false;
            Model skin(7, s.owner, s.game);
            s.game.lookupResults = {&skin, &skin};
            s.model.Request(NON_BLOCKING, tag);
            const auto requests = s.trace.Of(Kind::Request);
            Check(requests.size() == 2 && requests[0].key == 7 && requests[1].key == 0, "actual model7 Request recursion terminates before parent request");
            Check(s.trace.Of(Kind::Lookup).size() == 2, "actual model7 child does not recursively resolve itself");
        }
    }
    void RefusalAndGetterChecks()
    {
        const char tag[] = "owner refusal tag";
        for (auto status :
             {ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::OwnerConflict, ClockResult::Stopped})
        {
            Scenario s(0xfedcba98u);
            s.owner.animationStatus = status;
            std::uint32_t sentinel = 0x91abcdefu;
            Refuses([&] { sentinel = s.model.GetAnimFileIndex(); }, "animation file index");
            Check(sentinel == 0x91abcdefu, "refused actual animation getter preserves caller output");
            s.trace.Is({Kind::Animation});
            Check(s.trace.events[0].key == 0xfedcba98u, "actual getter forwards complete immutable model key before owner admission");
        }
        for (auto animation : {0u, 0xffffffffu, 0x87654321u})
        {
            Scenario s(0xfedcba98u);
            s.owner.animation = animation;
            Check(s.model.GetAnimFileIndex() == animation, "actual protected getter preserves every raw uint32 result including sentinel");
            s.trace.Is({Kind::Animation});
        }
        {
            NativeModelClassificationAccess olderOwner;
            Trace                           trace;
            Game                            game(trace);
            Model                           model(0x10000u, olderOwner, game);
            std::uint32_t                   sentinel = 0x11223344u;
            Refuses([&] { sentinel = model.GetAnimFileIndex(); }, "animation file index");
            Check(sentinel == 0x11223344u && trace.count == 0, "older optional owner defaults to refusal without fabricated animation or game lookup");
            std::puts("Actual optional animation owner default refusal covered");
        }
        for (bool type : {false, true})
        {
            Scenario s;
            if (type)
                s.owner.typeStatus = ClockResult::OwnerConflict;
            else
                s.owner.animationStatus = ClockResult::OwnerConflict;
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); }, type ? "model type" : "animation file index");
            Check(s.trace.Of(Kind::Request).empty() && s.trace.Of(Kind::Lookup).empty(),
                  "refused required owner observation prevents parent request and dependency lookup");
        }
        for (bool lookup : {false, true})
        {
            Scenario s;
            s.owner.animation = 42;
            s.game.baseFailure = !lookup;
            s.game.lookupFailure = lookup;
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); }, lookup ? "facade lookup refused" : "base IFP unavailable");
            Check(s.trace.Of(Kind::Request).empty(), "refused current base/receiver service prevents parent native request");
        }
        for (bool request : {false, true})
        {
            Scenario   s;
            Dependency child(s.trace, 42);
            s.owner.animation = 42;
            s.game.lookupResults = {&child};
            child.refuseLoaded = !request;
            child.refuseRequest = request;
            Refuses([&] { s.model.Request(NON_BLOCKING, tag); }, request ? "dependency request refused" : "dependency readiness refused");
            Check(s.trace.Of(Kind::Request).empty(), "animation receiver refusal never falls through to parent native request");
        }
    }
}
int main()
{
    ReadyAndVehicleChecks();
    MissingAnimationChecks();
    SkinChecks();
    RefusalAndGetterChecks();
    std::puts("Production actual model dependency SDK fixture completed");
    std::printf("Passed %u actual model dependency SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
