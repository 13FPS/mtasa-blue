/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Test the actual shared later-initialization and DE SDK dispatch
 *****************************************************************************/
#include "../../Client/game_de/CGameInitializationDE.h"
#include "../../Client/game_de/CGaragesDE.h"
#include "../../Client/game_de/CWeaponStatManagerDE.h"
#include <game/CGarage.h>
#include <game/CGameInitializationPolicy.h>
#include <game/CGarageInitializationPolicy.h>
#include <game/CGameModelPreparationPolicy.h>
#include <game/CModelInfo.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <cstring>
#include <utility>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated game SDK operation reached initialization fixture\n", stderr);
        std::abort();
    }
}
#include "GameInitializationFixture.generated.h"

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
        explicit Failure(int at) : std::runtime_error("fixture existing owner refused"), step(at) {}
        int step;
    };
    template <class Operation>
    void FailsAt(Operation operation, int step)
    {
        try
        {
            operation();
        }
        catch (const Failure& failure)
        {
            Check(failure.step == step, "exact original owner exception propagates without translation/retry");
            return;
        }
        Check(false, "required owner failure propagated");
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
            Check(true, "unavailable required SDK receiver refuses");
            return;
        }
        Check(false, "unavailable required SDK receiver cannot silently succeed");
    }

    void SharedPolicyChecks()
    {
        for (int failed = -1; failed < 5; ++failed)
        {
            std::vector<int> calls;
            auto             step = [&](int index)
            {
                calls.push_back(index);
                if (index == failed)
                    throw Failure(index);
            };
            auto initialize = [&]
            { MtaGame::CGameInitializationPolicy::Initialize([&] { step(0); }, [&] { step(1); }, [&] { step(2); }, [&] { step(3); }, [&] { step(4); }); };
            if (failed < 0)
                initialize();
            else
                FailsAt(initialize, failed);
            const auto length = failed < 0 ? 5 : failed + 1;
            Check(calls.size() == static_cast<std::size_t>(length), "shared policy stops immediately at the failing call");
            for (int index = 0; index < length; ++index)
                Check(calls[static_cast<std::size_t>(index)] == index, "actual five-step policy preserves exact order and exception prefix");
            calls.clear();
        }
        std::vector<int> repeated;
        auto             record = [&](int index) { repeated.push_back(index); };
        for (int attempt = 0; attempt < 3; ++attempt)
            MtaGame::CGameInitializationPolicy::Initialize([&] { record(0); }, [&] { record(1); }, [&] { record(2); }, [&] { record(3); }, [&] { record(4); });
        Check(repeated.size() == 15, "shared policy retains repeated Initialize calls rather than adding global once state");
        for (std::size_t index = 0; index < repeated.size(); ++index)
            Check(repeated[index] == static_cast<int>(index % 5), "each explicit repeated call starts the original sequence");
        std::puts("Actual shared later CGame Initialize policy fixture completed");
    }

    class ClockOwner final : public NativeClockAccess
    {
    public:
        ClockResult Read(ClockValues&) noexcept override { fixture::Unqualified(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { fixture::Unqualified(); }
        ClockResult Freeze(bool) noexcept override { fixture::Unqualified(); }
        ClockResult Frozen(bool&) const noexcept override { fixture::Unqualified(); }
    };
    class ConstructorOwner final : public NativeGameTimingAccess
    {
    public:
        ClockResult SetTimeScale(float) noexcept override { fixture::Unqualified(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { fixture::Unqualified(); }
        ClockResult Pause(bool) noexcept override { fixture::Unqualified(); }
        ClockResult RequireConstructorWeapons(const CClock* clock, CWeaponStatManager*& output) noexcept override
        {
            ++constructorRequires;
            requestedClock = clock;
            if (result == ClockResult::Ok)
                output = manager;
            return result;
        }
        ClockResult PrepareConstructorWeapons(NativeWeaponConstructorProgress&) noexcept override
        {
            ++preparations;
            return ClockResult::NotInitialized;
        }
        unsigned            constructorRequires = 0, preparations = 0;
        const CClock*       requestedClock = nullptr;
        CWeaponStatManager* manager = nullptr;
        ClockResult         result = ClockResult::NotInitialized;
    };
    struct ExistingOwner
    {
        std::vector<int>                                     effects;
        std::vector<std::uint32_t>                           garageIds;
        std::vector<std::uint32_t>                           modelResolutions;
        std::vector<std::pair<std::uint32_t, std::string>>   modelAssignments;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> collisionRequests;
        std::uint32_t            failResolveModel = ~std::uint32_t{}, failMakeModel = ~std::uint32_t{}, failCollisionTarget = ~std::uint32_t{};
        std::function<void(int)> inside;
        unsigned                 preflightCalls = 0, garageReads = 0;
        unsigned                 generation = 1, admittedGeneration = 0;
        int                      failStep = -1;
        bool                     ready = true, insideEffect = false;
        std::uint32_t            missingId = MAX_GARAGES;

        void Preflight()
        {
            ++preflightCalls;
            // This is a test-only implementation of the mandatory owner gate,
            // not production lease/attempt bookkeeping in the shared policy.
            if (!ready || insideEffect)
                throw Failure(-1);
            admittedGeneration = generation;
        }
        void Current() const
        {
            if (!ready || admittedGeneration != generation)
                throw Failure(-2);
        }
        void Effect(int step)
        {
            Current();
            effects.push_back(step);
            struct Reset
            {
                bool& value;
                ~Reset() { value = false; }
            } reset{insideEffect};
            insideEffect = true;
            if (inside)
                inside(step);
            if (step == failStep)
                throw Failure(step);
            Current();
        }
    };
    class Garage final : public CGarage
    {
    public:
        Garage(ExistingOwner& existingOwner, std::uint32_t index) : owner(existingOwner), id(index) {}
        void SetType(BYTE value) override
        {
            owner.Current();
            type = value;
            ++writes;
            owner.garageIds.push_back(id);
            owner.Effect(0);
        }
        BYTE           GetType() const override { return type; }
        void           GetPosition(CVector&) const override { fixture::Unqualified(); }
        void           GetDirection(float&, float&, float&, float&) const override { fixture::Unqualified(); }
        void           GetSize(float&, float&, float&) const override { fixture::Unqualified(); }
        void           GetBoundingBox(float&, float&, float&, float&) const override { fixture::Unqualified(); }
        void           SetOpen(bool) override { fixture::Unqualified(); }
        bool           IsOpen() const override { fixture::Unqualified(); }
        ExistingOwner& owner;
        std::uint32_t  id;
        BYTE           type = 0;
        unsigned       writes = 0;
    };
    class Garages final : public CGaragesDE
    {
    public:
        explicit Garages(ExistingOwner& existingOwner) : owner(existingOwner)
        {
            for (std::uint32_t id = 0; id < MAX_GARAGES; ++id)
                values[id] = std::make_unique<Garage>(existingOwner, id);
        }
        CGarage* GetGarage(DWORD id) override
        {
            ++owner.garageReads;
            owner.Current();
            if (id >= values.size() || id == owner.missingId)
                return nullptr;
            return values[id].get();
        }
        ExistingOwner&                                   owner;
        std::array<std::unique_ptr<Garage>, MAX_GARAGES> values;
    };
    class Model final : public fixture::ModelPreparationFixtureBase
    {
    public:
        explicit Model(ExistingOwner& existingOwner) : owner(existingOwner) {}
        void MakePedModel(const char* name) override
        {
            owner.Current();
            if (selected == 1)
                owner.Effect(1);
            // A test-only stand-in for the still-mandatory native creation
            // effect: record accepted intent before a possible owner fault.
            owner.modelAssignments.emplace_back(selected, name);
            if (selected == owner.failMakeModel)
                throw Failure(6);
            owner.Current();
        }
        ExistingOwner& owner;
        std::uint32_t  selected = ~std::uint32_t{};
    };
    class Game final : public fixture::GameInitializationFixtureBase
    {
    public:
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeGameTimingAccess& access, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state, ExistingOwner& existingOwner) noexcept
            : GameInitializationFixtureBase(reader, clock, access, streaming, state), owner(existingOwner), model(existingOwner)
        {
        }
        Game(const NativeGameTimingReader& reader, CClockDE& clock, NativeGameTimingAccess& access, const NativeStreamingFileIdsReader& streaming,
             const NativeGameStateReader& state, ExistingOwner& existingOwner, ConstructorServices tag)
            : GameInitializationFixtureBase(reader, clock, access, streaming, state, tag), owner(existingOwner), model(existingOwner)
        {
        }
        CGarages* GetGarages() override
        {
            ++garageResolutions;
            owner.Current();
            return garages;
        }
        using GameInitializationFixtureBase::ConstructorServices;
        ExistingOwner& owner;
        Model          model;
        CGarages*      garages = nullptr;
        unsigned       garageResolutions = 0;

    protected:
        void        RequireInitializationReady() override { owner.Preflight(); }
        CModelInfo& ResolveStartupPedModel(std::uint32_t selected) override
        {
            owner.Current();
            owner.modelResolutions.push_back(selected);
            if (selected == owner.failResolveModel)
                throw Failure(5);
            model.selected = selected;
            return model;
        }
        void FixStartupModelCollision(std::uint32_t target, std::uint32_t source) override
        {
            owner.Current();
            owner.collisionRequests.emplace_back(target, source);
            if (target == 3118)
                owner.Effect(2);
            if (target == owner.failCollisionTarget)
                throw Failure(7);
            owner.Current();
        }
        void InitializeRenderer() override { owner.Effect(3); }
        void HideNativeMenu() override { owner.Effect(4); }
    };
    static_assert(!std::is_abstract_v<Game> && std::is_base_of_v<CGame, Game>);
    static_assert(std::is_abstract_v<CGameInitializationDE>);

    std::vector<std::uint32_t> ExpectedIds()
    {
        std::vector<std::uint32_t> result;
        MtaGame::CGarageInitializationPolicy::Initialize(
            [&](std::uint32_t id, std::uint8_t type)
            {
                Check(type == 1, "real garage initializer keeps raw type1");
                result.push_back(id);
            });
        return result;
    }
    void ActualSdkChecks()
    {
        NativeGameTimingReader       reader;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        state;
        ClockOwner                   clockOwner;
        CClockDE                     clock(clockOwner);
        ConstructorOwner             constructor;
        ExistingOwner                owner;
        Garages                      garages(owner);
        Game                         game(reader, clock, constructor, streaming, state, owner);
        game.garages = &garages;
        CGame&     sdk = game;
        const auto expectedIds = ExpectedIds();
        Check(owner.effects.empty() && !owner.preflightCalls && !constructor.constructorRequires && !constructor.preparations,
              "partial construction invokes no later initialization or constructor preparation");
        for (unsigned attempt = 0; attempt < 3; ++attempt)
        {
            owner.effects.clear();
            owner.garageIds.clear();
            sdk.Initialize();
            Check(owner.preflightCalls == attempt + 1 && game.garageResolutions == attempt + 1,
                  "actual CGame SDK virtual calls freshly preflight and resolve required service once per call");
            Check(owner.garageIds == expectedIds, "actual CGaragesDE executes MTA's exact16-ID schedule before the four later steps");
            Check(owner.effects.size() == expectedIds.size() + 4, "actual later initialization executes no extra effect");
            for (std::size_t index = 0; index < expectedIds.size(); ++index)
                Check(owner.effects[index] == 0 && garages.values[expectedIds[index]]->writes == attempt + 1,
                      "repeated Initialize repeats each required garage effect without a hidden once flag");
            for (int index = 0; index < 4; ++index)
                Check(owner.effects[expectedIds.size() + static_cast<std::size_t>(index)] == index + 1,
                      "actual special/broken/renderer/menu effects retain exact order");
        }
        owner.effects.clear();
        owner.ready = false;
        auto before = game.garageResolutions;
        FailsAt([&] { sdk.Initialize(); }, -1);
        Check(owner.effects.empty() && game.garageResolutions == before, "unready owner refuses before the first effect/service resolution");
        owner.ready = true;
        game.garages = nullptr;
        Refuses([&] { sdk.Initialize(); });
        Check(owner.effects.empty(), "successful null cannot skip required numeric garage initialization");
        game.garages = &garages;
        for (int failed = 0; failed < 5; ++failed)
        {
            owner.effects.clear();
            owner.garageIds.clear();
            owner.failStep = failed;
            FailsAt([&] { sdk.Initialize(); }, failed);
            const auto expected = failed == 0 ? 1 : expectedIds.size() + static_cast<std::size_t>(failed);
            Check(owner.effects.size() == expected && owner.effects.back() == failed,
                  "actual SDK effect exception preserves exactly its accepted prefix and calls no later step");
            Check(garages.values[expectedIds.front()]->type == 1, "accepted garage mutation survives a later owner exception");
        }
        owner.failStep = -1;
        owner.effects.clear();
        owner.garageIds.clear();
        owner.missingId = expectedIds[5];
        Refuses([&] { sdk.Initialize(); });
        Check(owner.effects.size() == 5 && owner.garageIds.size() == 5,
              "required missing numeric garage preserves earlier five writes without substituting a named/default facade");
        owner.missingId = MAX_GARAGES;
        owner.effects.clear();
        owner.inside = [&](int step)
        {
            if (step == 1)
                sdk.Initialize();
        };
        before = owner.preflightCalls;
        FailsAt([&] { sdk.Initialize(); }, -1);
        Check(owner.preflightCalls == before + 2 && owner.effects.size() == expectedIds.size() + 1 && !owner.insideEffect,
              "reentry reaches fresh existing-owner readiness and its refusal stops the outer call without nested effects");
        owner.inside = [&](int step)
        {
            if (step == 1)
                ++owner.generation;
        };
        owner.effects.clear();
        FailsAt([&] { sdk.Initialize(); }, -2);
        Check(owner.effects.size() == expectedIds.size() + 1, "owner generation revocation preserves prefix and refuses later effects");
        owner.inside = {};
        owner.effects.clear();
        sdk.Initialize();
        Check(owner.effects.size() == expectedIds.size() + 4,
              "a new explicit qualified call remains possible after failures; policy neither auto-replays nor creates terminal state");

        NativeWeaponInfoReader weaponReader;
        CWeaponInfoBankDE      bank(weaponReader, constructor, constructor);
        CWeaponStatManagerDE   manager(bank, constructor);
        constructor.manager = &manager;
        constructor.result = ClockResult::Ok;
        ExistingOwner preparedOwner;
        Garages       preparedGarages(preparedOwner);
        Game          prepared(reader, clock, constructor, streaming, state, preparedOwner, Game::ConstructorServices::PreparedWeapons);
        prepared.garages = &preparedGarages;
        Check(constructor.constructorRequires == 1 && constructor.requestedClock == &clock && preparedOwner.effects.empty() && !preparedOwner.preflightCalls,
              "tagged construction only consumes its actual complete manager/clock and does not run later Initialize");
        CGame& preparedSdk = prepared;
        preparedSdk.Initialize();
        Check(constructor.constructorRequires == 1 && !constructor.preparations && preparedOwner.effects.size() == expectedIds.size() + 4,
              "later Initialize never replays constructor weapon preparation or substitutes its readiness stage");
        Check(manager.GetStartupState() == CWeaponStatManagerDE::StartupState::NotStarted && !weaponReader.IsBound(),
              "CPU SDK fixture publishes no native preparation claim and performs no native bank read/write");
        Check(!reader.IsBound() && !streaming.IsBound() && !state.IsBound(), "actual initialization dispatch executes no native game code");
        std::puts("Actual later CGame Initialize SDK dispatch fixture completed");
    }

    // Independent golden input contract copied from the pre-extraction classic
    // active statements. Commented girlfriend/hot-coffee models stay excluded.
    constexpr std::array<std::pair<std::uint32_t, const char*>, 44> StartupModels{{
        {1, "TRUTH"},     {2, "MACCER"},   {3, "CDEPUT"},    {4, "SFPDM1"},    {5, "BB"},        {6, "WFYCRP"}, {8, "WMYCD2"},   {42, "SUZIE"},
        {65, "VWMYAP"},   {86, "VHFYST"},  {119, "LVPDM1"},  {265, "TENPEN"},  {266, "PULASKI"}, {267, "HERN"}, {268, "DWAYNE"}, {269, "SMOKE"},
        {270, "SWEET"},   {271, "RYDER"},  {272, "FORELLI"}, {273, "MEDIATR"}, {289, "SOMYAP"},  {290, "ROSE"}, {291, "PAUL"},   {292, "CESAR"},
        {293, "OGLOC"},   {294, "WUZIMU"}, {295, "TORINO"},  {296, "JIZZY"},   {297, "MADDOGG"}, {298, "CAT"},  {299, "CLAUDE"}, {300, "RYDER2"},
        {301, "RYDER3"},  {302, "EMMET"},  {303, "ANDRE"},   {304, "KENDL"},   {305, "JETHRO"},  {306, "ZERO"}, {307, "TBONE"},  {308, "SINDACO"},
        {309, "JANITOR"}, {310, "BBTHIN"}, {311, "SMOKEV"},  {312, "PSYCHO"},
    }};
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>      CollisionModels{{3118, 3059}, {3553, 3554}};

    void ModelPreparationChecks()
    {
        std::size_t next = 0;
        MtaGame::CGameModelPreparationPolicy::SetupSpecialCharacters(
            [&](std::uint32_t model, const char* name)
            {
                Check(next < StartupModels.size(), "shared model schedule has exactly the original active entries");
                Check(model == StartupModels[next].first && std::strcmp(name, StartupModels[next].second) == 0,
                      "shared model schedule preserves exact MTA numeric identity, name and order");
                ++next;
            });
        Check(next == StartupModels.size(), "all44 original special models are dispatched");
        std::vector<std::pair<std::uint32_t, std::uint32_t>> collisions;
        MtaGame::CGameModelPreparationPolicy::SetupBrokenModels([&](std::uint32_t target, std::uint32_t source) { collisions.emplace_back(target, source); });
        Check(collisions == CollisionModels, "shared collision schedule preserves both directed source/target pairs");
        std::puts("Actual shared startup model preparation policy fixture completed");

        NativeGameTimingReader       reader;
        NativeStreamingFileIdsReader streaming;
        NativeGameStateReader        state;
        ClockOwner                   clockOwner;
        CClockDE                     clock(clockOwner);
        ConstructorOwner             constructor;
        ExistingOwner                owner;
        Garages                      garages(owner);
        Game                         game(reader, clock, constructor, streaming, state, owner);
        game.garages = &garages;
        CGame& sdk = game;
        auto   reset = [&]
        {
            owner.effects.clear();
            owner.garageIds.clear();
            owner.modelResolutions.clear();
            owner.modelAssignments.clear();
            owner.collisionRequests.clear();
        };
        auto exactPrefix = [&](std::size_t count)
        {
            Check(owner.modelAssignments.size() == count, "actual SDK dispatch retains exactly the accepted special-model prefix");
            for (std::size_t index = 0; index < count; ++index)
                Check(owner.modelAssignments[index].first == StartupModels[index].first && owner.modelAssignments[index].second == StartupModels[index].second,
                      "actual CModelInfo MakePedModel receives the original key/name once in order");
        };
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            reset();
            sdk.Initialize();
            exactPrefix(StartupModels.size());
            Check(owner.modelResolutions.size() == StartupModels.size(), "each model is freshly resolved once, never loaded-only filtered or cached");
            for (std::size_t index = 0; index < StartupModels.size(); ++index)
                Check(owner.modelResolutions[index] == StartupModels[index].first, "qualified resolver retains exact MTA numeric model identity");
            Check(owner.collisionRequests == CollisionModels, "actual DE schedule dispatches mandatory conditional collision hooks in original order");
            Check(owner.effects.size() == 20 && owner.effects[16] == 1 && owner.effects[17] == 2 && owner.effects[18] == 3 && owner.effects[19] == 4,
                  "both repeated calls retain garage, all special models, collision, renderer, menu order");
        }
        reset();
        owner.failResolveModel = StartupModels[22].first;
        FailsAt([&] { sdk.Initialize(); }, 5);
        exactPrefix(22);
        Check(owner.modelResolutions.size() == 23 && owner.collisionRequests.empty() && owner.effects.size() == 17,
              "missing qualified facade stops before its MakePedModel and every later phase, preserving earlier effects");
        owner.failResolveModel = ~std::uint32_t{};
        reset();
        owner.failMakeModel = StartupModels[22].first;
        FailsAt([&] { sdk.Initialize(); }, 6);
        exactPrefix(23);
        Check(owner.modelResolutions.size() == 23 && owner.collisionRequests.empty() && owner.effects.size() == 17,
              "entered MakePedModel failure preserves its accepted prefix without rollback, fallback or replay");
        owner.failMakeModel = ~std::uint32_t{};
        reset();
        owner.failCollisionTarget = 3553;
        FailsAt([&] { sdk.Initialize(); }, 7);
        exactPrefix(44);
        Check(owner.collisionRequests == CollisionModels && owner.effects.size() == 18,
              "second collision-hook failure retains special models and first substitution without reaching renderer/menu");
        owner.failCollisionTarget = ~std::uint32_t{};
        reset();
        sdk.Initialize();
        exactPrefix(44);
        Check(owner.collisionRequests == CollisionModels && owner.effects.size() == 20,
              "a new explicit call repeats the same complete itinerary after repair; there is no hidden terminal or once state");
        Check(!constructor.preparations && !constructor.constructorRequires && !reader.IsBound() && !streaming.IsBound() && !state.IsBound(),
              "later model dispatch neither replays constructor weapons nor invents native creation/streaming readiness");
        std::puts("Actual startup model preparation SDK dispatch fixture completed");
    }
}

int main()
{
    ModelPreparationChecks();
    SharedPolicyChecks();
    ActualSdkChecks();
    std::printf("Passed %u actual later game initialization checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return EXIT_SUCCESS;
}
