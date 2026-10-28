/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Test actual later Initialize consuming the owned texture registry
 ****************************************************************************/
#include "../../Client/game_de/CGameRenderPreparationDE.h"
#include "../../Client/game_de/CGaragesDE.h"
#include <game/CGarage.h>
#include <game/CStreaming.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated game SDK operation reached renderer preparation fixture\n", stderr);
        std::abort();
    }
}
#include "GameRenderPreparationFixture.generated.h"

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
    struct Failure : std::runtime_error
    {
        explicit Failure(int phase) : std::runtime_error("required test receiver refused"), phase(phase) {}
        int phase;
    };
    template <class Action>
    void FailsAt(Action action, int phase)
    {
        try
        {
            action();
        }
        catch (const Failure& failure)
        {
            Check(failure.phase == phase, "original required receiver exception propagates without translation");
            return;
        }
        Check(false, "required receiver failure cannot become successful initialization");
    }
    template <class Action>
    void Refuses(Action action)
    {
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            Check(true, "native owner refusal propagates through actual SDK Initialize");
            return;
        }
        Check(false, "native owner refusal cannot skip into registry/menu work");
    }
    struct Context
    {
        std::array<unsigned, 256> events{};
        std::size_t               count = 0;
        unsigned                  creates = 0, requests = 0, copies = 0, menus = 0, preflights = 0;
        std::uint32_t             lastCreated = 0;
        CRenderWarePreparationDE* preparation = nullptr;
        STexInfo*                 priorRegistry = nullptr;
        bool                      ready = true, failMenu = false;
        std::uint32_t             failCreate = 0xffffffffu, failCollision = 0xffffffffu;

        void Add(unsigned phase) noexcept
        {
            Check(count < events.size(), "fixed test trace has capacity for one complete explicit initialization");
            events[count++] = phase;
        }
        void BeforeRenderer() const noexcept
        {
            Check(preparation && preparation->GetNoTexture() == priorRegistry,
                  "all earlier startup effects preserve the previous registry identity; the first call has none yet");
        }
        void ResetTrace() noexcept { count = creates = requests = copies = menus = 0; }
        void CheckComplete() const
        {
            Check(count == 107 && creates == 44 && requests == 44 && copies == 2 && menus == 1,
                  "actual Initialize executes 16 garages, 44 native creations/requests, two copies and one final menu effect");
            std::size_t next = 0;
            for (unsigned index = 0; index < 16; ++index)
                Check(events[next++] == 1, "actual numeric garage schedule precedes model work");
            for (unsigned index = 0; index < 44; ++index)
                Check(events[next++] == 2 && events[next++] == 3, "each retained native creator precedes its actual streaming receiver");
            Check(events[next++] == 4 && events[next++] == 4 && events[next] == 5,
                  "both broken-model copies precede renderer preparation and the required native menu stage");
        }
    };
    class ClockOwner final : public NativeClockAccess
    {
    public:
        ClockResult Read(ClockValues&) noexcept override { fixture::Unqualified(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { fixture::Unqualified(); }
        ClockResult Freeze(bool) noexcept override { fixture::Unqualified(); }
        ClockResult Frozen(bool&) const noexcept override { fixture::Unqualified(); }
    };
    class NativeOwner final : public NativeGameTimingAccess
    {
    public:
        explicit NativeOwner(Context& context) noexcept : context(context) {}
        ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
        ClockResult CreateAndPublishPedModelMetadata(std::uint32_t model) noexcept override
        {
            context.BeforeRenderer();
            context.Add(2);
            ++context.creates;
            context.lastCreated = model;
            return model == context.failCreate ? ClockResult::Expired : ClockResult::Ok;
        }
        ClockResult CopyStartupModelCollision(std::uint32_t target, std::uint32_t source) noexcept override
        {
            context.BeforeRenderer();
            Check((context.copies == 0 && target == 3118 && source == 3059) || (context.copies == 1 && target == 3553 && source == 3554),
                  "actual collision stage keeps the two directed MTA pairs before renderer initialization");
            context.Add(4);
            ++context.copies;
            return target == context.failCollision ? ClockResult::WriteFailure : ClockResult::Ok;
        }
        Context& context;
    };
    class Garage final : public CGarage
    {
    public:
        explicit Garage(Context& context) noexcept : context(context) {}
        void SetType(BYTE type) override
        {
            context.BeforeRenderer();
            Check(type == 1, "actual CGaragesDE retains the raw source default type");
            context.Add(1);
        }
        BYTE     GetType() const override { fixture::Unqualified(); }
        void     GetPosition(CVector&) const override { fixture::Unqualified(); }
        void     GetDirection(float&, float&, float&, float&) const override { fixture::Unqualified(); }
        void     GetSize(float&, float&, float&) const override { fixture::Unqualified(); }
        void     GetBoundingBox(float&, float&, float&, float&) const override { fixture::Unqualified(); }
        void     SetOpen(bool) override { fixture::Unqualified(); }
        bool     IsOpen() const override { fixture::Unqualified(); }
        Context& context;
    };
    class Garages final : public CGaragesDE
    {
    public:
        explicit Garages(Context& context) : context(context)
        {
            for (auto& garage : garages)
                garage = std::make_unique<Garage>(context);
        }
        CGarage* GetGarage(DWORD id) override
        {
            constexpr std::array<DWORD, 16> ids{{7, 8, 10, 11, 12, 15, 18, 19, 24, 27, 32, 33, 36, 40, 41, 47}};
            Check(context.count < ids.size() && id == ids[context.count], "actual garage SDK lookup preserves its independent 16-entry golden order");
            return garages[id].get();
        }
        Context&                                         context;
        std::array<std::unique_ptr<Garage>, MAX_GARAGES> garages;
    };
    class Streaming final : public fixture::StreamingPreparationFixtureBase
    {
    public:
        explicit Streaming(Context& context) noexcept : context(context) {}
        void RequestSpecialModel(DWORD model, const char* name, DWORD channel) override
        {
            context.BeforeRenderer();
            Check(model == context.lastCreated && name && *name && channel == 0,
                  "actual model preparation resolves the same full created key and original special channel");
            context.Add(3);
            ++context.requests;
        }
        Context& context;
    };
    class Game final : public fixture::GameRenderPreparationFixtureBase
    {
    public:
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeOwner& native, const NativeStreamingFileIdsReader& layout,
             const NativeGameStateReader& state, Context& context)
            : GameRenderPreparationFixtureBase(reader, clock, native, layout, state), context(context), garages(context), streaming(context)
        {
            context.preparation = &GetRenderPreparation();
        }
        CGarages* GetGarages() override
        {
            context.BeforeRenderer();
            return &garages;
        }
        CStreaming* GetStreaming() override
        {
            context.BeforeRenderer();
            return &streaming;
        }
        Context&  context;
        Garages   garages;
        Streaming streaming;

    protected:
        void RequireInitializationReady() override
        {
            ++context.preflights;
            if (!context.ready)
                throw Failure(0);
        }
        void HideNativeMenu() override
        {
            auto* texture = GetRenderPreparation().GetNoTexture();
            Check(texture && texture->pAssociatedTexNameInfo && texture->strTextureName == "unnamed" &&
                      texture->pAssociatedTexNameInfo->usedByTexInfoList.size() == 1,
                  "the real inherited InitializeRenderer publishes and inserts its owned actual registry before native menu handling");
            Check(!context.priorRegistry || context.priorRegistry == texture, "explicit repeated initialization preserves the same registry record identity");
            context.priorRegistry = texture;
            context.Add(5);
            ++context.menus;
            if (context.failMenu)
                throw Failure(5);
        }
    };
    static_assert(std::is_abstract_v<CGameRenderPreparationDE> && std::is_base_of_v<CGameInitializationDE, CGameRenderPreparationDE>);
    static_assert(!std::is_abstract_v<Game> && std::is_base_of_v<CGame, Game>);
    static_assert(std::is_same_v<decltype(&Game::Initialize), void (CGameInitializationDE::*)()>);
    struct Scenario
    {
        Context                      context;
        NativeGameTimingReader       reader;
        NativeStreamingFileIdsReader layout;
        NativeGameStateReader        state;
        ClockOwner                   clockOwner;
        CClockDE                     clock{clockOwner};
        NativeOwner                  native{context};
        Game                         game{reader, clock, native, layout, state, context};
    };
    void SuccessfulInitialization()
    {
        Scenario scenario;
        auto&    context = scenario.context;
        Check(!scenario.game.GetRenderPreparation().GetNoTexture() && !context.count && !context.preflights,
              "construction owns empty CPU metadata without running later initialization or native effects");
        for (unsigned attempt = 0; attempt < 3; ++attempt)
        {
            context.ResetTrace();
            CGame& sdk = scenario.game;
            sdk.Initialize();
            context.CheckComplete();
            Check(context.preflights == attempt + 1, "every explicit Initialize retains the mandatory current owner admission");
        }
        Check(!scenario.reader.IsBound() && !scenario.layout.IsBound() && !scenario.state.IsBound(),
              "actual SDK fixture performs no native image reads or engine calls");
        std::puts("Actual owned renderer preparation Initialize SDK fixture completed");
    }
    void RefusalPrefixes()
    {
        {
            Scenario scenario;
            scenario.context.ready = false;
            FailsAt([&] { scenario.game.Initialize(); }, 0);
            Check(!scenario.context.count && !scenario.game.GetRenderPreparation().GetNoTexture(),
                  "unready mandatory admission prevents every effect and registry creation");
        }
        {
            Scenario scenario;
            scenario.context.failCreate = 291;
            Refuses([&] { scenario.game.Initialize(); });
            Check(scenario.context.creates == 23 && scenario.context.requests == 22 && !scenario.context.copies && !scenario.context.menus &&
                      !scenario.game.GetRenderPreparation().GetNoTexture(),
                  "creator refusal preserves its request prefix and prevents collision, registry and menu stages");
        }
        {
            Scenario scenario;
            scenario.context.failCollision = 3553;
            Refuses([&] { scenario.game.Initialize(); });
            Check(scenario.context.creates == 44 && scenario.context.requests == 44 && scenario.context.copies == 2 && !scenario.context.menus &&
                      !scenario.game.GetRenderPreparation().GetNoTexture(),
                  "second collision refusal retains all earlier effects without reaching registry or menu work");
        }
        {
            Scenario scenario;
            scenario.context.failMenu = true;
            FailsAt([&] { scenario.game.Initialize(); }, 5);
            auto* accepted = scenario.game.GetRenderPreparation().GetNoTexture();
            Check(accepted && accepted == scenario.context.priorRegistry && scenario.context.menus == 1,
                  "later native menu refusal cannot roll back the actually registered renderer preparation prefix");
            scenario.context.failMenu = false;
            scenario.context.ResetTrace();
            scenario.game.Initialize();
            scenario.context.CheckComplete();
            Check(scenario.game.GetRenderPreparation().GetNoTexture() == accepted,
                  "a later explicit call repeats the source itinerary while actual registry membership keeps initialization idempotent");
        }
        std::puts("Actual owned renderer preparation refusal prefix fixture completed");
    }
}

int main()
{
    SuccessfulInitialization();
    RefusalPrefixes();
    std::printf("Passed %u actual renderer preparation initialization checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
