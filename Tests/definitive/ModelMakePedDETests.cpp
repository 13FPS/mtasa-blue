/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MakePedModel creation/publication-before-request order
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoRequestDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <game/CGame.h>
#include <algorithm>
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
        std::fputs("FAIL: unrelated SDK operation reached MakePedModel fixture\n", stderr);
        std::abort();
    }
}
#include "ModelRequestFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    struct Failure : std::runtime_error
    {
        explicit Failure(unsigned where) : std::runtime_error("required creation/streaming receiver refused"), where(where) {}
        unsigned where;
    };
    template <class Action>
    void FailsAt(Action action, unsigned where)
    {
        try
        {
            action();
        }
        catch (const Failure& failure)
        {
            Check(failure.where == where, "original creation/service failure propagates from its exact boundary");
            return;
        }
        Check(false, "required creation/service failure cannot become success");
    }
    template <class Action>
    void Refuses(Action action, const std::string& expected)
    {
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            Check(error.what() == expected, "explicit required-owner refusal retains its exact diagnostic and status");
            return;
        }
        Check(false, "missing current-owner authority cannot be replaced by a request-only path");
    }
    struct Trace
    {
        std::array<unsigned, 32> events{};
        std::size_t              count = 0;
        void                     Add(unsigned event) noexcept
        {
            if (count == events.size())
                fixture::Unqualified();
            events[count++] = event;
        }
        void Expect(std::initializer_list<unsigned> expected)
        {
            Check(count == expected.size() && std::equal(expected.begin(), expected.end(), events.begin()),
                  "source create/publication precedes service lookup and one special request without fallback");
            count = 0;
        }
    };
    struct StreamingOwner final : NativeStreamingModelAccess
    {
        explicit StreamingOwner(Trace& trace) : trace(trace) {}
        ClockResult RequestStreamingSpecialModel(std::uint32_t model, const char* texture, std::uint32_t channel) noexcept override
        {
            trace.Add(3);
            ++requests;
            lastModel = model;
            lastTexture = texture;
            lastChannel = channel;
            return status;
        }
        ClockResult   RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult   RequestStreamingUpgrade(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult   ReadStreamingModelLoaded(std::uint32_t, NativeModelLoadedReader::Value&) noexcept override { fixture::Unqualified(); }
        ClockResult   ReadStreamingUpgradeLoaded(std::uint32_t, bool&) noexcept override { fixture::Unqualified(); }
        ClockResult   LoadStreamingModels(bool) noexcept override { fixture::Unqualified(); }
        Trace&        trace;
        unsigned      requests = 0;
        std::uint32_t lastModel = 0, lastChannel = 0xffffffffu;
        const char*   lastTexture = nullptr;
        ClockResult   status = ClockResult::Ok;
    };
    struct Streaming final : fixture::StreamingFixtureBase
    {
        explicit Streaming(NativeStreamingModelAccess& owner) noexcept : StreamingFixtureBase(owner) {}
    };
    struct Game final : fixture::GameFixtureBase
    {
        explicit Game(Trace& trace) : trace(trace) {}
        CStreaming* GetStreaming() override
        {
            trace.Add(2);
            ++reads;
            if (fail)
                throw Failure(2);
            return streaming;
        }
        Trace&      trace;
        CStreaming* streaming = nullptr;
        unsigned    reads = 0;
        bool        fail = false;
    };
    class DefaultModel : public fixture::ModelFixtureBase
    {
    public:
        DefaultModel(std::uint32_t model, NativeModelClassificationAccess& owner, CGame& game) noexcept : ModelFixtureBase(model, owner, game) {}

    protected:
        void ReportBlockingModelLoad(bool, int, const char*) override { fixture::Unqualified(); }
        void IncrementNativeModelInterfaceReference() override { fixture::Unqualified(); }
        void RemoveNativeModelInterfaceReference() override { fixture::Unqualified(); }
    };
    class Model final : public DefaultModel
    {
    public:
        Model(std::uint32_t model, NativeModelClassificationAccess& owner, CGame& game, Trace& trace) noexcept : DefaultModel(model, owner, game), trace(trace)
        {
        }
        unsigned      creationCalls = 0, acknowledgedPublications = 0;
        std::uint32_t lastCreatedModel = 0;
        bool          refuseCreation = false;

    protected:
        void CreateAndPublishNativePedModel(std::uint32_t modelIndex) override
        {
            trace.Add(1);
            ++creationCalls;
            lastCreatedModel = modelIndex;
            if (refuseCreation)
                throw Failure(1);
            // This is only the explicitly required provider seam, not a native
            // factory or a copied allocation journal. Its accepted prefix must
            // survive a later failure in the real streaming SDK wrapper.
            ++acknowledgedPublications;
        }

    private:
        Trace& trace;
    };
    struct Scenario
    {
        Trace                           trace;
        NativeModelClassificationAccess classification;
        StreamingOwner                  owner{trace};
        Streaming                       streaming{owner};
        Game                            game{trace};
        Model                           model;
        explicit Scenario(std::uint32_t key = 7) : model(key, classification, game, trace) { game.streaming = &streaming; }
    };
    static_assert(std::is_abstract_v<CModelInfoRequestDE>);
    static_assert(!std::is_abstract_v<Model> && !std::is_abstract_v<DefaultModel>);
    static_assert(!std::is_abstract_v<Streaming> && std::is_base_of_v<CStreaming, Streaming>);
    static_assert(std::is_same_v<decltype(&Model::MakePedModel), void (CModelInfoRequestDE::*)(const char*)>);
    static_assert(std::is_same_v<decltype(&Streaming::RequestSpecialModel), void (CStreamingDE::*)(DWORD, const char*, DWORD)>);

    struct CreationOwner final : NativeModelClassificationAccess
    {
        explicit CreationOwner(Trace& trace) : trace(trace) {}
        ClockResult CreateAndPublishPedModelMetadata(std::uint32_t modelIndex) noexcept override
        {
            trace.Add(1);
            ++calls;
            lastModel = modelIndex;
            // This observes the new optional owner channel only. The actual
            // native creation journal is not reimplemented in this SDK spy.
            if (status == ClockResult::Ok || acknowledgePrefixBeforeRefusal)
                ++acknowledgedPrefixes;
            return status;
        }
        Trace&        trace;
        std::uint32_t lastModel = 0;
        unsigned      calls = 0, acknowledgedPrefixes = 0;
        ClockResult   status = ClockResult::Ok;
        bool          acknowledgePrefixBeforeRefusal = false;
    };
    class OwnerModel final : public DefaultModel
    {
    public:
        using CModelInfoRequestDE::CreateAndPublishNativePedModel;
        using DefaultModel::DefaultModel;
    };
    static_assert(std::is_same_v<decltype(&OwnerModel::CreateAndPublishNativePedModel), void (CModelInfoRequestDE::*)(std::uint32_t)>);
    static_assert(std::is_same_v<decltype(&OwnerModel::MakePedModel), void (CModelInfoRequestDE::*)(const char*)>);

    void CreationOwnerDispatchChecks()
    {
        const char texture[] = "original owner-dispatch texture";
        for (const auto key : std::array<std::uint32_t, 5>{{0u, 7u, 20000u, 0x80000000u, 0xffffffffu}})
        {
            Trace          trace;
            CreationOwner  classification(trace);
            StreamingOwner streamingOwner(trace);
            Streaming      streaming(streamingOwner);
            Game           game(trace);
            game.streaming = &streaming;
            OwnerModel model(key, classification, game);
            Check(!classification.calls && !game.reads && !streamingOwner.requests,
                  "same-owner construction binding performs no hidden creation, classification or request");
            CModelInfo& sdk = model;
            sdk.MakePedModel(texture);
            trace.Expect({1, 2, 3});
            Check(classification.calls == 1 && classification.lastModel == key && classification.acknowledgedPrefixes == 1 && streamingOwner.requests == 1 &&
                      streamingOwner.lastModel == key && streamingOwner.lastTexture == texture && streamingOwner.lastChannel == 0 && !sdk.GetRefCount(),
                  "actual inherited creator forwards the complete unsigned key once to its original owner before the real streaming request");
            streamingOwner.status = ClockResult::Expired;
            Refuses([&] { sdk.MakePedModel(texture); },
                    "DE streaming owner refused RequestSpecialModel: " + std::to_string(static_cast<unsigned>(ClockResult::Expired)));
            trace.Expect({1, 2, 3});
            Check(classification.calls == 2 && classification.acknowledgedPrefixes == 2 && streamingOwner.requests == 2 && !sdk.GetRefCount(),
                  "a later request failure preserves the owner-acknowledged publication prefix and source repetition");
        }
        constexpr std::array<ClockResult, 14> refusals{
            {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
             ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
             ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (const auto status : refusals)
        {
            Trace          trace;
            CreationOwner  classification(trace);
            StreamingOwner streamingOwner(trace);
            Streaming      streaming(streamingOwner);
            Game           game(trace);
            game.streaming = &streaming;
            OwnerModel model(0xffffffffu, classification, game);
            classification.status = status;
            const std::string expected = status == ClockResult::NotInitialized
                                             ? "DE MakePedModel requires qualified native ped metadata creation and bank publication"
                                             : "DE model owner refused ped metadata creation/publication: " + std::to_string(static_cast<unsigned>(status));
            Refuses([&] { model.MakePedModel(texture); }, expected);
            trace.Expect({1});
            Check(classification.calls == 1 && classification.lastModel == 0xffffffffu && !classification.acknowledgedPrefixes && !game.reads &&
                      !streamingOwner.requests,
                  "new owner refusal remains explicit and blocks every streaming receiver without raw-key narrowing or fallback");
        }
        {
            Trace          trace;
            CreationOwner  classification(trace);
            StreamingOwner streamingOwner(trace);
            Streaming      streaming(streamingOwner);
            Game           game(trace);
            game.streaming = &streaming;
            OwnerModel model(7u, classification, game);
            classification.status = ClockResult::BindingChanged;
            classification.acknowledgePrefixBeforeRefusal = true;
            Refuses([&] { model.MakePedModel(texture); },
                    "DE model owner refused ped metadata creation/publication: " + std::to_string(static_cast<unsigned>(ClockResult::BindingChanged)));
            trace.Expect({1});
            Check(classification.calls == 1 && classification.acknowledgedPrefixes == 1 && !game.reads && !streamingOwner.requests,
                  "late native-owner refusal preserves its accepted prefix without SDK rollback, retry or a premature special request");
        }
        std::puts("Production actual model ped creation owner dispatch SDK fixture completed");
    }

    void SuccessAndRepetitionChecks()
    {
        const char                       empty[] = "";
        const char                       name[] = "borrowed special ped texture";
        const char                       high[] = {'m', static_cast<char>(0xe9), 'X', '\0'};
        const std::array<const char*, 4> names{{nullptr, empty, name, high}};
        for (const auto key : std::array<std::uint32_t, 5>{{0u, 7u, 20000u, 0x80000000u, 0xffffffffu}})
        {
            Scenario s(key);
            Check(s.model.GetRefCount() == 0 && !s.model.creationCalls && !s.game.reads && !s.owner.requests,
                  "construction and CPU wrapper count do not request/create native metadata");
            for (const auto* texture : names)
            {
                CModelInfo& sdk = s.model;
                const auto  before = s.model.creationCalls;
                sdk.MakePedModel(texture);
                s.trace.Expect({1, 2, 3});
                Check(s.model.creationCalls == before + 1 && s.model.acknowledgedPublications == before + 1 && s.model.lastCreatedModel == key &&
                          s.owner.lastModel == key && s.owner.lastTexture == texture && s.owner.lastChannel == 0u,
                      "full model key, original null/empty/high-byte pointer and channel zero reach the real special-model wrapper once");
                Check(s.owner.requests == before + 1 && sdk.GetRefCount() == 0,
                      "explicit later calls repeat the source creation/request instead of once/load/reference policy");
            }
        }
    }

    void PrefixAndRefusalChecks()
    {
        const char texture[] = "source pointer retained across a failed request";
        {
            Scenario s;
            s.model.refuseCreation = true;
            FailsAt([&] { s.model.MakePedModel(texture); }, 1);
            s.trace.Expect({1});
            Check(s.model.creationCalls == 1 && !s.model.acknowledgedPublications && !s.game.reads && !s.owner.requests,
                  "creation/publication refusal precedes every game receiver and request");
            s.model.refuseCreation = false;
            s.model.MakePedModel(texture);
            s.trace.Expect({1, 2, 3});
            Check(s.model.creationCalls == 2 && s.model.acknowledgedPublications == 1 && s.owner.requests == 1,
                  "a later explicit call repeats the source sequence without hidden retry state");
        }
        {
            Scenario s;
            s.game.fail = true;
            FailsAt([&] { s.model.MakePedModel(texture); }, 2);
            s.trace.Expect({1, 2});
            Check(s.model.acknowledgedPublications == 1 && !s.owner.requests,
                  "creation/publication remains accepted when resolving the required streaming receiver fails");
        }
        {
            Scenario s;
            s.game.streaming = nullptr;
            Refuses([&] { s.model.MakePedModel(texture); }, "DE model request requires the qualified streaming service");
            s.trace.Expect({1, 2});
            Check(s.model.acknowledgedPublications == 1 && !s.owner.requests, "missing streaming cannot undo the already accepted source publication");
        }
        constexpr std::array<ClockResult, 14> refusals{
            {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
             ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
             ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (const auto status : refusals)
        {
            Scenario s(0xffffffffu);
            s.owner.status = status;
            Refuses([&] { s.model.MakePedModel(texture); }, "DE streaming owner refused RequestSpecialModel: " + std::to_string(static_cast<unsigned>(status)));
            s.trace.Expect({1, 2, 3});
            Check(s.model.creationCalls == 1 && s.model.acknowledgedPublications == 1 && s.owner.requests == 1 && s.owner.lastTexture == texture &&
                      s.owner.lastModel == 0xffffffffu && !s.model.GetRefCount(),
                  "special request refusal retains the irreversible prefix without rollback, load, dependency or wrapper-reference effects");
            s.owner.status = ClockResult::Ok;
            s.model.MakePedModel(texture);
            s.trace.Expect({1, 2, 3});
            Check(s.model.creationCalls == 2 && s.model.acknowledgedPublications == 2 && s.owner.requests == 2,
                  "explicit later calls preserve source repetition after request failure");
        }
        {
            Scenario     s;
            DefaultModel missing(7u, s.classification, s.game);
            // The real default boundary must refuse before even discovering
            // streaming. A request alone cannot create/publish the ped model.
            Refuses([&] { missing.MakePedModel(texture); }, "DE MakePedModel requires qualified native ped metadata creation and bank publication");
            Check(!s.game.reads && !s.owner.requests, "missing native creator refuses before streaming lookup/request");
            s.trace.Expect({});
        }
    }
}

int main()
{
    try
    {
        SuccessAndRepetitionChecks();
        PrefixAndRefusalChecks();
        CreationOwnerDispatchChecks();
        std::puts("Production actual model MakePedModel SDK fixture completed");
        std::printf("Passed %u actual model MakePedModel SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
