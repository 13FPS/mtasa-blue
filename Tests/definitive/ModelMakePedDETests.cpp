/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve actual MakePedModel creation/publication-before-request order
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoRequestDE.h"
#include "../../Client/game_de/CGameModelPreparationDE.h"
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
#include <utility>

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
        std::array<unsigned, 512> events{};
        std::size_t               count = 0;
        void                      Add(unsigned event) noexcept
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
            if (requests == keys.size())
                fixture::Unqualified();
            keys[requests] = model;
            names[requests] = texture;
            channels[requests] = channel;
            ++requests;
            lastModel = model;
            lastTexture = texture;
            lastChannel = channel;
            return failAt == 0 || requests == failAt ? status : ClockResult::Ok;
        }
        ClockResult                    RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult                    RequestStreamingUpgrade(std::uint32_t, std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult                    ReadStreamingModelLoaded(std::uint32_t, NativeModelLoadedReader::Value&) noexcept override { fixture::Unqualified(); }
        ClockResult                    ReadStreamingUpgradeLoaded(std::uint32_t, bool&) noexcept override { fixture::Unqualified(); }
        ClockResult                    LoadStreamingModels(bool) noexcept override { fixture::Unqualified(); }
        Trace&                         trace;
        unsigned                       requests = 0, failAt = 0;
        std::array<std::uint32_t, 128> keys{}, channels{};
        std::array<const char*, 128>   names{};
        std::uint32_t                  lastModel = 0, lastChannel = 0xffffffffu;
        const char*                    lastTexture = nullptr;
        ClockResult                    status = ClockResult::Ok;
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
            const auto current = failAt == 0 || calls == failAt ? status : ClockResult::Ok;
            if (current == ClockResult::Ok || acknowledgePrefixBeforeRefusal)
                ++acknowledgedPrefixes;
            return current;
        }
        Trace&        trace;
        std::uint32_t lastModel = 0;
        unsigned      calls = 0, acknowledgedPrefixes = 0, failAt = 0;
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
    // The original active SA statements are the independent schedule oracle.
    constexpr std::array<std::pair<std::uint32_t, const char*>, 44> StartupModels{{
        {1, "TRUTH"},     {2, "MACCER"},   {3, "CDEPUT"},    {4, "SFPDM1"},    {5, "BB"},        {6, "WFYCRP"}, {8, "WMYCD2"},   {42, "SUZIE"},
        {65, "VWMYAP"},   {86, "VHFYST"},  {119, "LVPDM1"},  {265, "TENPEN"},  {266, "PULASKI"}, {267, "HERN"}, {268, "DWAYNE"}, {269, "SMOKE"},
        {270, "SWEET"},   {271, "RYDER"},  {272, "FORELLI"}, {273, "MEDIATR"}, {289, "SOMYAP"},  {290, "ROSE"}, {291, "PAUL"},   {292, "CESAR"},
        {293, "OGLOC"},   {294, "WUZIMU"}, {295, "TORINO"},  {296, "JIZZY"},   {297, "MADDOGG"}, {298, "CAT"},  {299, "CLAUDE"}, {300, "RYDER2"},
        {301, "RYDER3"},  {302, "EMMET"},  {303, "ANDRE"},   {304, "KENDL"},   {305, "JETHRO"},  {306, "ZERO"}, {307, "TBONE"},  {308, "SINDACO"},
        {309, "JANITOR"}, {310, "BBTHIN"}, {311, "SMOKEV"},  {312, "PSYCHO"},
    }};

    void ConcretePreparationChecks()
    {
        static_assert(!std::is_abstract_v<CGameModelPreparationDE>);
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            Trace                   trace;
            CreationOwner           creation(trace);
            StreamingOwner          requests(trace);
            Streaming               streaming(requests);
            CGameModelPreparationDE preparation(creation);
            Check(!creation.calls && !requests.requests && !trace.count, "concrete model preparation construction has no engine or lookup effect");
            unsigned resolutions = 0;
            auto     resolve = [&]() -> CStreaming*
            {
                trace.Add(2);
                ++resolutions;
                Check(creation.calls == resolutions && requests.requests + 1 == resolutions &&
                          creation.lastModel == StartupModels[(resolutions - 1) % StartupModels.size()].first,
                      "each late service lookup follows its corresponding actual native publication");
                return &streaming;
            };
            for (unsigned repeat = 0; repeat <= attempt; ++repeat)
                preparation.SetupSpecialCharacters(resolve);
            const auto expected = StartupModels.size() * (attempt + 1);
            Check(creation.calls == expected && resolutions == expected && requests.requests == expected && trace.count == expected * 3,
                  "actual shared44 schedule repeats only on explicit calls, with one create/lookup/request per entry");
            for (std::size_t index = 0; index < expected; ++index)
            {
                const auto& entry = StartupModels[index % StartupModels.size()];
                Check(trace.events[index * 3] == 1 && trace.events[index * 3 + 1] == 2 && trace.events[index * 3 + 2] == 3,
                      "each full native-owner/real SDK request tuple preserves original effect order");
                Check(requests.keys[index] == entry.first && std::string(requests.names[index]) == entry.second && requests.channels[index] == 0,
                      "concrete preparation preserves all44 exact keys, original names and special channel zero");
            }
        }
        constexpr std::array<ClockResult, 14> refusals{
            {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
             ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
             ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException}};
        for (const auto status : refusals)
        {
            Trace                   trace;
            CreationOwner           creation(trace);
            StreamingOwner          requests(trace);
            Streaming               streaming(requests);
            CGameModelPreparationDE preparation(creation);
            creation.status = status;
            creation.failAt = 23;
            creation.acknowledgePrefixBeforeRefusal = true;
            unsigned resolutions = 0;
            auto     resolve = [&]() -> CStreaming*
            {
                trace.Add(2);
                ++resolutions;
                return &streaming;
            };
            const std::string expected = status == ClockResult::NotInitialized
                                             ? "DE MakePedModel requires qualified native ped metadata creation and bank publication"
                                             : "DE model owner refused ped metadata creation/publication: " + std::to_string(static_cast<unsigned>(status));
            Refuses([&] { preparation.SetupSpecialCharacters(resolve); }, expected);
            Check(creation.calls == 23 && creation.acknowledgedPrefixes == 23 && resolutions == 22 && requests.requests == 22 && trace.count == 67,
                  "creation refusal retains every accepted native prefix and prevents this/later streaming without retry or rollback");
        }
        for (unsigned failure = 0; failure < 3; ++failure)
        {
            Trace                   trace;
            CreationOwner           creation(trace);
            StreamingOwner          requests(trace);
            Streaming               streaming(requests);
            CGameModelPreparationDE preparation(creation);
            unsigned                resolutions = 0;
            requests.failAt = 23;
            requests.status = failure == 2 ? ClockResult::Expired : ClockResult::Ok;
            auto resolve = [&]() -> CStreaming*
            {
                trace.Add(2);
                ++resolutions;
                if (resolutions == 23 && failure == 0)
                    throw Failure(2);
                return resolutions == 23 && failure == 1 ? nullptr : &streaming;
            };
            if (failure == 0)
                FailsAt([&] { preparation.SetupSpecialCharacters(resolve); }, 2);
            else
                Refuses([&] { preparation.SetupSpecialCharacters(resolve); },
                        failure == 1 ? "DE model request requires the qualified streaming service"
                                     : "DE streaming owner refused RequestSpecialModel: " + std::to_string(static_cast<unsigned>(ClockResult::Expired)));
            Check(creation.calls == 23 && creation.acknowledgedPrefixes == 23 && resolutions == 23 && requests.requests == (failure == 2 ? 23u : 22u),
                  "late getter/null/request refusal preserves source publication and stops the exact accepted itinerary prefix");
        }
        for (const auto key : std::array<std::uint32_t, 5>{{0u, 7u, 20000u, 0x80000000u, 0xffffffffu}})
        {
            Trace                   trace;
            CreationOwner           creation(trace);
            StreamingOwner          requests(trace);
            Streaming               streaming(requests);
            CGameModelPreparationDE preparation(creation);
            const char              texture[] = "original borrowed source texture";
            auto                    resolve = [&]() -> CStreaming*
            {
                trace.Add(2);
                return &streaming;
            };
            for (const char* original : std::array<const char*, 3>{{texture, "", nullptr}})
            {
                preparation.MakePedModel(key, original, resolve);
                trace.Expect({1, 2, 3});
                Check(creation.lastModel == key && requests.lastModel == key && requests.lastTexture == original && requests.lastChannel == 0,
                      "concrete assembly forwards full keys and original null/empty/borrowed pointer without policy invention");
            }
        }
        std::puts("Production concrete startup model preparation fixture completed");
    }

    class ChangingModel final : public DefaultModel
    {
    public:
        using DefaultModel::DefaultModel;
        DWORD    GetModel() override { return ++reads == 1 ? 7u : 0x80000000u; }
        unsigned reads = 0;
    };
    void OriginalBoundaryChecks()
    {
        Trace          trace;
        CreationOwner  creation(trace);
        StreamingOwner requests(trace);
        Streaming      streaming(requests);
        Game           game(trace);
        game.streaming = &streaming;
        ChangingModel model(0u, creation, game);
        model.MakePedModel("source boundary");
        trace.Expect({1, 2, 3});
        Check(model.reads == 2 && creation.lastModel == 7u && requests.lastModel == 0x80000000u,
              "shared ordering does not cache GetModel or resolve streaming before the original expression boundaries");
        bool second = false;
        // Keep refusal runtime-observable so /O2 cannot diagnose the intended
        // not-called second effect as unreachable code inside this fixture.
        volatile bool refuseFirst = true;
        FailsAt(
            [&]
            {
                MtaGame::CModelPedCreationPolicy::MakePedModel(
                    [&]
                    {
                        if (refuseFirst)
                            throw Failure(1);
                    },
                    [&] { second = true; });
            },
            1);
        Check(!second, "shared helper preserves the original first exception and prevents all later effects");
        unsigned prefix = 0;
        FailsAt([&] { MtaGame::CModelPedCreationPolicy::MakePedModel([&] { ++prefix; }, [] { throw Failure(2); }); }, 2);
        Check(prefix == 1, "shared helper preserves accepted publication on the original second exception without replay");
        std::puts("Production shared ped creation original boundaries fixture completed");
    }

}

int main()
{
    try
    {
        SuccessAndRepetitionChecks();
        PrefixAndRefusalChecks();
        CreationOwnerDispatchChecks();
        ConcretePreparationChecks();
        OriginalBoundaryChecks();
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
