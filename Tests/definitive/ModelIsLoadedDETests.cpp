/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual model readiness through streaming and opaque handles
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoRequestDE.h"
#include "../../Client/game_de/CStreamingDE.h"
#include <game/CGame.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached model readiness fixture\n", stderr);
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
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }
    struct Log
    {
        std::array<unsigned, 16> events{};
        std::size_t              count = 0;
        void                     Push(unsigned event) noexcept
        {
            if (count == events.size())
                fixture::Unqualified();
            events[count++] = event;
        }
        void Expect(std::initializer_list<unsigned> expected)
        {
            Check(count == expected.size(), "readiness has exactly the source-required effects");
            std::size_t index = 0;
            for (const auto event : expected)
                Check(events[index++] == event, "streaming/category/interface/render order is source faithful");
            count = 0;
        }
    };
    struct Classification final : NativeModelClassificationAccess
    {
        explicit Classification(Log& events) noexcept : log(events) {}
        ClockResult ReadModelInterface(std::uint32_t index, std::uintptr_t& output) noexcept override
        {
            log.Push(3);
            lastInterface = index;
            if (interfaceStatus != ClockResult::Ok)
                return interfaceStatus;
            output = interfaceTag;
            return ClockResult::Ok;
        }
        ClockResult ReadModelRender(std::uint32_t index, std::uintptr_t& output) noexcept override
        {
            log.Push(4);
            lastRender = index;
            if (renderStatus != ClockResult::Ok)
                return renderStatus;
            output = renderTag;
            return ClockResult::Ok;
        }
        // Raw readiness must not acquire archive/type/bundle prerequisites.
        ClockResult    ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept override { fixture::Unqualified(); }
        ClockResult    ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override { fixture::Unqualified(); }
        ClockResult    ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept override { fixture::Unqualified(); }
        Log&           log;
        std::uintptr_t interfaceTag = 1, renderTag = 1;
        std::uint32_t  lastInterface = 0, lastRender = 0;
        ClockResult    interfaceStatus = ClockResult::Ok, renderStatus = ClockResult::Ok;
    };
    struct StreamingOwner final : NativeStreamingModelAccess
    {
        explicit StreamingOwner(Log& events) noexcept : log(events) {}
        ClockResult ReadStreamingModelLoaded(std::uint32_t index, NativeModelLoadedReader::Value& output) noexcept override
        {
            log.Push(1);
            lastModel = index;
            if (status != ClockResult::Ok)
                return status;
            output = {};
            output.streamedIn = loaded;
            return ClockResult::Ok;
        }
        ClockResult ReadStreamingUpgradeLoaded(std::uint32_t index, bool& output) noexcept override
        {
            log.Push(5);
            lastModel = index;
            if (status != ClockResult::Ok)
                return status;
            output = loaded;
            return ClockResult::Ok;
        }
        Log&          log;
        std::uint32_t lastModel = 0;
        bool          loaded = false;
        ClockResult   status = ClockResult::Ok;
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
            log.Push(2);
            if (categoryRefused)
                throw std::runtime_error("current game file-ID owner refused");
            return txdBase;
        }
        Log&          log;
        CStreaming*   streaming = nullptr;
        std::uint32_t txdBase = 20000;
        bool          categoryRefused = false;
    };
    class Model final : public fixture::ModelFixtureBase
    {
    public:
        Model(std::uint32_t modelKey, NativeModelClassificationAccess& owner, CGame& game) noexcept : ModelFixtureBase(modelKey, owner, game) {}
        using CModelInfoRequestDE::DoIsLoaded;

    protected:
        // These four bridges remain mandatory in production. No request or
        // reference operation is needed to observe a fresh zero-ref facade.
        void RequestModelDependencies(EModelRequestType, const char*) override { fixture::Unqualified(); }
        void ReportBlockingModelLoad(bool, int, const char*) override { fixture::Unqualified(); }
        void IncrementNativeModelInterfaceReference() override { fixture::Unqualified(); }
        void RemoveNativeModelInterfaceReference() override { fixture::Unqualified(); }
    };
    static_assert(std::is_same_v<decltype(&Model::DoIsLoaded), bool (CModelInfoRequestDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::GetInterface), CBaseModelInfoSAInterface* (CModelInfoDE::*)()>);
    static_assert(std::is_same_v<decltype(&Model::GetRwObject), RwObject* (CModelInfoDE::*)()>);
    static_assert(std::is_abstract_v<CModelInfoRequestDE> && !std::is_abstract_v<Model>);
    template <class Operation>
    void Refuses(Operation operation)
    {
        try
        {
            operation();
        }
        catch (const std::runtime_error&)
        {
            Check(true, "owner refusal remains an exception, not false readiness");
            return;
        }
        Check(false, "refused readiness must not manufacture a boolean result");
    }
    struct Scenario
    {
        Log            log;
        Classification classification{log};
        StreamingOwner streamingOwner{log};
        Streaming      streaming{streamingOwner};
        Game           game{log};
        Model          model;
        explicit Scenario(std::uint32_t key = 45) : model(key, classification, game) { game.streaming = &streaming; }
        bool Loaded()
        {
            CModelInfo& sdk = model;
            return sdk.IsLoaded();
        }
    };
    void ReadinessChecks()
    {
        Scenario s;
        Check(s.log.count == 0, "construction has no hidden readiness/native query");
        s.game.categoryRefused = true;
        s.classification.interfaceStatus = ClockResult::BindingChanged;
        Check(!s.Loaded(), "unloaded result precedes category and DFF owner gates");
        s.log.Expect({1});
        s.streamingOwner.loaded = true;
        Refuses([&] { s.Loaded(); });
        s.log.Expect({1, 2});
        s.game.categoryRefused = false;
        s.classification.interfaceStatus = ClockResult::Ok;
        s.classification.interfaceTag = 0;
        Check(!s.Loaded(), "loaded DFF with null interface stays not ready");
        s.log.Expect({1, 2, 3});
        s.classification.interfaceTag = std::numeric_limits<std::uintptr_t>::max();
        s.classification.renderTag = 0;
        Check(!s.Loaded(), "loaded DFF with nonnull interface and null render stays not ready");
        s.log.Expect({1, 2, 3, 4});
        s.classification.renderTag = std::numeric_limits<std::uintptr_t>::max();
        Check(s.Loaded(), "opaque poison handles are tested for presence without target dereference");
        s.log.Expect({1, 2, 3, 4});
        Check(s.classification.lastInterface == 45 && s.classification.lastRender == 45, "both fresh borrows preserve the selected full model key");
        s.classification.interfaceTag = 0;
        Check(!s.Loaded(), "a later query re-admits current handles rather than a persistent CPU cache");
        s.log.Expect({1, 2, 3});
        s.game.streaming = nullptr;
        Refuses([&] { s.Loaded(); });
        s.log.Expect({});

        for (const std::uint32_t key : {0u, 999u, 1000u, 1193u, 1194u, 19999u, 20000u, 0xffffffffu})
        {
            Scenario       item(key);
            const unsigned event = key >= 1000 && key <= 1193 ? 5u : 1u;
            Check(!item.Loaded(), "ordinary and paired unloaded routes stop before metadata");
            item.log.Expect({event});
            item.streamingOwner.loaded = true;
            Check(item.Loaded(), "loaded DFF and nonDFF preserve real streaming predicate");
            if (key < item.game.txdBase)
                item.log.Expect({event, 2, 3, 4});
            else
                item.log.Expect({event, 2});
            Check(item.streamingOwner.lastModel == key, "ordinary/paired policy forwards full uint32 model unchanged");
        }
        Scenario category(45);
        category.streamingOwner.loaded = true;
        category.game.txdBase = 45;
        category.classification.interfaceStatus = ClockResult::InvalidArgument;
        Check(category.Loaded(), "actual game boundary, not a new hardcoded DFF limit, selects nonDFF path");
        category.log.Expect({1, 2});
        category.game.txdBase = 46;
        Refuses([&] { category.Loaded(); });
        category.log.Expect({1, 2, 3});
    }
    void FailureChecks()
    {
        for (unsigned raw = 1; raw <= static_cast<unsigned>(ClockResult::OriginalException); ++raw)
        {
            const auto status = static_cast<ClockResult>(raw);
            Scenario   s;
            s.streamingOwner.loaded = true;
            s.streamingOwner.status = status;
            Refuses([&] { s.Loaded(); });
            s.log.Expect({1});
            s.streamingOwner.status = ClockResult::Ok;
            s.classification.interfaceStatus = status;
            Refuses([&] { s.Loaded(); });
            s.log.Expect({1, 2, 3});
            s.classification.interfaceStatus = ClockResult::Ok;
            s.classification.renderStatus = status;
            Refuses([&] { s.Loaded(); });
            s.log.Expect({1, 2, 3, 4});
        }
        Scenario missing;
        missing.streamingOwner.loaded = true;
        NativeModelClassificationAccess oldOwner;
        Model                           actual(45, oldOwner, missing.game);
        Refuses([&] { actual.IsLoaded(); });
        missing.log.Expect({1, 2});
    }
}
int main()
{
    ReadinessChecks();
    FailureChecks();
    std::puts("Actual MTA model readiness SDK fixture completed");
    std::printf("Passed %u actual MTA model readiness SDK checks (%zu-bit; current opaque readiness)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
