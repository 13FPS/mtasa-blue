/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual inherited movement setter with real block/request facades
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include "../../Client/game_de/CAnimManagerDE.h"
#include "../../Client/game_de/CStreamingDE.h"
// Reuse strict definitions and constructors only. None of their historical
// matrices is executed by this focused setter executable.
#define main HistoricalMoveAnimGetterMain
#include "PlayerMoveAnimDETests.cpp"
#undef main
#include "AnimationBlockFactoryFixture.generated.h"
#include <algorithm>
#include <iostream>
#include <vector>

namespace setter_fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        // An unrelated strict-fixture call must fail the process, rather than
        // being mistaken for the production refusal a negative case expects.
        std::fputs("Unqualified setter fixture service\n", stderr);
        std::abort();
    }
}
#include "PlayerMoveAnimSetterFixture.generated.h"

namespace setter_fixture
{
    using namespace mta::game_de;
    using Manager = AnimationBlockFactoryManagerReceiver;
    using Player = player_move_anim_fixture::PlayerMoveAnimFixture;
    constexpr std::uint64_t           Generation = 73;
    constexpr NativePedReferenceToken Token{19, Generation, 1};

    struct Owner final : NativeGameTimingAccess, NativeAnimationAssociationGroupAccess, NativeAnimationStaticAssociationAccess
    {
        std::vector<std::string> events;
        std::string              fail, blockName;
        unsigned                 failCount = 0, finds = 0, identities = 0, blockReads = 0, refs = 0, validReads = 0;
        bool                     present = true, loaded = false, loadCreatesGroup = true, missingBlock = false, nullHierarchy = false;
        bool                     poisonedGroup = false, poisonedChild = false, allTokens = true;
        bool                     childrenPresent = true;
        ClockResult              childrenResult = ClockResult::Ok;
        std::int32_t             nativeGroup = 17, snapshot = 0;
        std::uint32_t            requestedModel = 0, requestedFlags = 0;
        const CPed*              state = nullptr;
        bool                     cacheAtWrite = true, cacheAtReapply = true;
        ClockResult              Hit(const std::string& name) noexcept
        {
            events.push_back(name);
            if (name == fail && failCount++ == 0)
                return ClockResult::BindingChanged;
            return ClockResult::Ok;
        }
        bool Current(const NativePedReferenceToken& token) noexcept
        {
            allTokens = allTokens && SameToken(token, Token);
            return allTokens;
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult ReadPedMoveAnim(const NativePedReferenceToken& token, std::int32_t& output) noexcept override
        {
            Current(token);
            output = nativeGroup;
            return Hit("read-group");
        }
        ClockResult ResolveAnimationAssociationGroup(std::uint64_t generation, std::uint32_t group,
                                                     NativeAnimationAssociationGroupReferenceToken& output) noexcept override
        {
            allTokens = allTokens && generation == Generation;
            output = {generation, group + (poisonedGroup ? 1u : 0u), 0x8100u};
            return Hit("resolve");
        }
        ClockResult ReadAnimationAssociationGroupBlock(const NativeAnimationAssociationGroupReferenceToken& token,
                                                       NativeAnimationAssociationGroupBlock&                output) noexcept override
        {
            allTokens = allTokens && token.ownerGeneration == Generation && token.locator == 0x8100u;
            output.interfaceAddress = present ? 0x8200u : 0;
            return Hit("group-block");
        }
        ClockResult ReadAnimationAssociationGroupChildArrayPresent(const NativeAnimationAssociationGroupReferenceToken& token, bool& output) noexcept override
        {
            allTokens = allTokens && token.ownerGeneration == Generation && token.locator == 0x8100u;
            output = childrenPresent;
            const auto result = Hit("children");
            return result == ClockResult::Ok ? childrenResult : result;
        }
        ClockResult ResolveAnimationAssociationGroupStaticById(const NativeAnimationAssociationGroupReferenceToken& token, std::uint32_t id,
                                                               NativeAnimationStaticAssociationReferenceToken& output) noexcept override
        {
            output = {token.ownerGeneration, token.index, id, static_cast<std::uintptr_t>(0x9000u + id * 32u)};
            return Hit("id:" + std::to_string(id));
        }
        ClockResult ReadAnimationStaticAssociation(const NativeAnimationStaticAssociationReferenceToken& token,
                                                   NativeAnimationStaticAssociationValues&               output) noexcept override
        {
            ++validReads;
            output.interfaceAddress = token.locator + (poisonedChild ? 1u : 0u);
            output.hierarchyAddress = nullHierarchy ? 0 : 0xa000u;
            return Hit("hierarchy:" + std::to_string(token.index));
        }
        ClockResult FindAnimationBlockIndex(std::uint64_t generation, const NativeAnimationBlockName& name, std::int32_t& output) noexcept override
        {
            ++finds;
            allTokens = allTokens && generation == Generation;
            blockName = reinterpret_cast<const char*>(name.data());
            output = missingBlock ? -1 : 17;
            return Hit("find:" + blockName);
        }
        ClockResult ReadAnimationBlockIdentity(std::uint64_t generation, std::uint32_t index, NativeAnimationBlockIdentity& output) noexcept override
        {
            ++identities;
            allTokens = allTokens && generation == Generation && index == 17;
            output = {};
            output.ownerGeneration = generation;
            output.index = index;
            std::copy(blockName.begin(), blockName.end(), output.name.begin());
            return Hit("identity");
        }
        ClockResult ReadAnimationBlock(const NativeAnimationBlockIdentity& identity, NativeAnimationBlockValues& output) noexcept override
        {
            ++blockReads;
            allTokens = allTokens && identity.ownerGeneration == Generation && identity.index == 17;
            output = {};
            output.index = identity.index;
            output.loaded = loaded ? 1 : 0;
            return Hit("block");
        }
        ClockResult AddAnimationBlockRef(const NativeAnimationBlockIdentity& identity) noexcept override
        {
            allTokens = allTokens && identity.ownerGeneration == Generation && identity.index == 17;
            const auto result = Hit("ref");
            if (result == ClockResult::Ok)
                ++refs;
            return result;
        }
        ClockResult RequestStreamingModel(std::uint32_t model, std::uint32_t flags) noexcept override
        {
            requestedModel = model;
            requestedFlags = flags;
            return Hit("request");
        }
        ClockResult LoadStreamingModels(bool priority) noexcept override
        {
            allTokens = allTokens && priority;
            const auto result = Hit("load");
            if (result == ClockResult::Ok && loadCreatesGroup)
                present = true;
            return result;
        }
        ClockResult ReadStreamingModelLoaded(std::uint32_t model, NativeModelLoadedReader::Value& output) noexcept override
        {
            output.modelIndex = model;
            output.streamedIn = loaded;
            return Hit("stream-loaded");
        }
        ClockResult EnablePlayerMoveAnimHooks() noexcept override { return Hit("enable"); }
        ClockResult PublishPlayerMoveAnimOverride(const NativePedReferenceToken& token, std::int32_t group) noexcept override
        {
            Current(token);
            const auto result = Hit("publish");
            if (result == ClockResult::Ok)
                snapshot = group;
            return result;
        }
        ClockResult WithdrawPlayerMoveAnimOverride(const NativePedReferenceToken& token) noexcept override
        {
            Current(token);
            const auto result = Hit("withdraw");
            if (result == ClockResult::Ok)
                snapshot = 0;
            return result;
        }
        ClockResult WriteCurrentPlayerMoveAnimGroup(const NativePedReferenceToken& token, std::int32_t group) noexcept override
        {
            Current(token);
            cacheAtWrite = cacheAtWrite && state && state->GetCustomMoveAnim() == snapshot && snapshot == group;
            const auto result = Hit("write");
            if (result == ClockResult::Ok)
                nativeGroup = group;
            return result;
        }
        ClockResult ReapplyCurrentPlayerMoveAnim(const NativePedReferenceToken& token) noexcept override
        {
            Current(token);
            cacheAtReapply = cacheAtReapply && state && state->GetCustomMoveAnim() == snapshot && nativeGroup == snapshot;
            return Hit("reapply");
        }
        unsigned                 Count(const std::string& value) const { return static_cast<unsigned>(std::count(events.begin(), events.end(), value)); }
        bool                     Has(const std::string& value) const { return std::find(events.begin(), events.end(), value) != events.end(); }
        std::vector<std::string> Effects() const
        {
            std::vector<std::string> result;
            for (const auto& event : events)
                if (event == "request" || event == "ref" || event == "load" || event == "enable" || event == "publish" || event == "write" ||
                    event == "reapply" || event == "withdraw")
                    result.push_back(event);
            return result;
        }
    };
    struct Game final : GameBase
    {
        Owner&    owner;
        Streaming streaming;
        unsigned  modelLookups = 0;
        explicit Game(Owner& owner) : owner(owner), streaming(owner) {}
        CStreaming* GetStreaming() override
        {
            owner.events.push_back("get-streaming");
            return &streaming;
        }
        CModelInfo* GetModelInfo(DWORD model, bool invalid = false) override
        {
            ++modelLookups;
            if (model != 25017 || invalid)
                Unqualified();
            owner.events.push_back("model-lookup");
            return nullptr;
        }
        std::uint32_t GetBaseIDforIFP() override
        {
            owner.events.push_back("base");
            return 25000;
        }
    };
    struct Fixture
    {
        Owner                               owner;
        Game                                game{owner};
        Manager                             manager{owner, owner, game, Generation};
        OtherOwners                         other;
        Grant                               grant;
        Clear                               clear;
        shared_player_base_fixture::Clothes clothes;
        shared_player_base_fixture::Rebuild rebuild;
        ped_rebuild_fixture::TaskScenario   tasks;
        ped_rebuild_fixture::WeaponLookup   lookup;
        Player                              player;
        explicit Fixture(bool configure = true)
            : player(player_move_anim_sdk_tests::Make(game, other, grant, clear, clothes, rebuild, owner, Token, tasks, lookup, nullptr))
        {
            if (configure)
                player.FixtureConfigure(owner, manager);
            owner.state = &player;
        }
        void Set(unsigned group) { static_cast<CPlayerPed&>(player).SetMoveAnim(static_cast<eMoveAnim>(group)); }
        bool Refused(unsigned group)
        {
            try
            {
                Set(group);
            }
            catch (const std::runtime_error&)
            {
                return true;
            }
            return false;
        }
    };
    template <class Action>
    bool Invalid(Action action)
    {
        try
        {
            action();
        }
        catch (const std::invalid_argument&)
        {
            return true;
        }
        return false;
    }
    void InvalidDefault()
    {
        Fixture raw(false);
        raw.Set(255);
        Check(raw.owner.events.empty(), "invalid request requires no configured services and performs no owner operation");
        Check(raw.Refused(0) && raw.owner.events.empty(), "valid default requires the real configured owner rather than silently accepting");
        raw.player.FixtureConfigure(raw.owner, raw.manager);
        raw.player.FixtureCustomMoveAnim(55);
        raw.owner.snapshot = 55;
        raw.Set(0);
        Check(raw.owner.Effects() == std::vector<std::string>{"withdraw"}, "default only withdraws snapshot, with no group/ref/request/reapply effect");
        Check(raw.player.GetCustomMoveAnim() == 0 && raw.owner.snapshot == 0 && raw.owner.nativeGroup == 17,
              "default clears actual protected CPU and snapshot together while preserving native current group");
        raw.player.FixtureCustomMoveAnim(56);
        raw.owner.snapshot = 56;
        raw.owner.fail = "withdraw";
        Check(raw.Refused(0), "withdraw refusal is visible");
        Check(raw.player.GetCustomMoveAnim() == 56 && raw.owner.snapshot == 56 && raw.owner.nativeGroup == 17,
              "refused clear preserves both custom values and current native group");
        const auto count = raw.owner.events.size();
        raw.Set(255);
        Check(raw.owner.events.size() == count, "configured invalid request also has no effects");
        Check(raw.owner.allTokens && !raw.grant.calls && !raw.clear.calls && !raw.tasks.eventCount, "setter never delegates an unrelated ped service");
    }
    void Loaded()
    {
        Fixture fixture;
        Check(fixture.owner.events.empty(), "configuration only borrows exact typed endpoints");
        fixture.Set(55);
        Check(fixture.owner.blockReads == 0 && !fixture.owner.loaded, "nonnull group block admits setter without reading the separate block loaded byte");
        Check(fixture.owner.validReads == 5, "all five real validity IDs use fresh static association reads");
        Check(fixture.owner.finds == 1 && fixture.owner.identities == 1 && fixture.owner.blockName == "fat",
              "actual manager returns the requested named block facade");
        Check(fixture.owner.Effects() == std::vector<std::string>{"ref", "enable", "publish", "write", "reapply"},
              "native effect order preserves real ref/publication/store/reapply itinerary");
        Check(fixture.player.GetCustomMoveAnim() == 55 && fixture.owner.snapshot == 55 && fixture.owner.nativeGroup == 55 &&
                  static_cast<CPlayerPed&>(fixture.player).GetMoveAnim() == static_cast<eMoveAnim>(55),
              "actual protected cache, snapshot and inherited native getter agree after success");
        Check(fixture.owner.cacheAtWrite && fixture.owner.cacheAtReapply && fixture.owner.allTokens,
              "CPU publication precedes group write and reapply with the same copied registered token");
        fixture.Set(58);
        Check(fixture.owner.refs == 1 && fixture.owner.finds == 2 && fixture.owner.Count("reapply") == 2,
              "same module name keeps accepted reference intent but still creates a fresh facade and reapplies");
    }
    void Request(bool completes)
    {
        Fixture fixture;
        fixture.owner.present = false;
        fixture.owner.loadCreatesGroup = completes;
        fixture.Set(138);
        Check(fixture.owner.requestedModel == 25017 && fixture.owner.requestedFlags == 0x14,
              "real block Request translates index through actual CGame streaming path");
        Check(fixture.game.modelLookups == 1 && fixture.owner.Count("stream-loaded") == 0,
              "blocking-allowed-fail retains unused model lookup and elides completion assertion read");
        Check(fixture.owner.refs == 1 && fixture.owner.Count("load") == 1, "Request supplies exactly one native reference before priority load");
        Check(!fixture.owner.loaded, "group recheck is independent of the still-false block loaded byte");
        if (completes)
        {
            Check(fixture.owner.finds == 2 && fixture.owner.identities == 2, "two separate real manager block facade lookups bracket loading and validity");
            Check(fixture.owner.Effects() == std::vector<std::string>{"request", "ref", "load", "enable", "publish", "write", "reapply"},
                  "load reference is recorded and not added twice through final facade");
            Check(fixture.player.GetCustomMoveAnim() == 138 && fixture.owner.nativeGroup == 138 && fixture.owner.snapshot == 138,
                  "successful actual request continues through inherited setter");
        }
        else
        {
            Check(fixture.owner.finds == 1 && fixture.owner.validReads == 0, "missing group after load stops before validity and final facade");
            Check(fixture.owner.Effects() == std::vector<std::string>{"request", "ref", "load"},
                  "failed-load presence recheck preserves accepted request prefix");
            Check(fixture.player.GetCustomMoveAnim() == 0 && fixture.owner.nativeGroup == 17 && fixture.owner.snapshot == 0,
                  "missing group never publishes custom/native state");
        }
        Check(fixture.owner.allTokens, "request and validation preserve exact borrowed owner identities");
    }
    void Prefix(const std::string& phase)
    {
        Fixture fixture;
        fixture.owner.fail = phase;
        if (phase == "request" || phase == "load")
            fixture.owner.present = false;
        Check(fixture.Refused(56), "actual owner refusal reaches the SDK caller");
        const std::vector<std::string> full = fixture.owner.present || phase == "load"
                                                  ? (phase == "load" ? std::vector<std::string>{"request", "ref", "load"}
                                                                     : std::vector<std::string>{"ref", "enable", "publish", "write", "reapply"})
                                                  : std::vector<std::string>{"request"};
        auto                           expected = full;
        const auto                     stop = std::find(expected.begin(), expected.end(), phase);
        if (stop != expected.end())
            expected.erase(stop + 1, expected.end());
        Check(fixture.owner.Effects() == expected, "refusal keeps exact native effect prefix with no replay or rollback");
        const bool published = phase == "write" || phase == "reapply";
        Check(fixture.player.GetCustomMoveAnim() == (published ? 56 : 0) && fixture.owner.snapshot == (published ? 56 : 0),
              "actual CPU cache follows successful publication, including later failures");
        Check(fixture.owner.nativeGroup == (phase == "reapply" ? 56 : 17), "native group changes only after its own successful acknowledgement");
        if (phase == "ref")
        {
            Check(fixture.owner.refs == 0, "failed AddRef has no fabricated successful native ref");
            fixture.Set(59);
            Check(fixture.owner.Count("ref") == 1 && fixture.owner.refs == 0 && fixture.owner.nativeGroup == 59,
                  "module name intent was inserted before failed AddRef and is not undone by the SDK");
        }
        else
        {
            Check(!fixture.owner.Has("withdraw"), "no inverse override operation is invented after failure");
            Check(fixture.owner.allTokens, "failure prefix still uses the exact registered token");
        }
    }
    void Observation(const std::string& kind)
    {
        Fixture fixture;
        fixture.owner.nullHierarchy = kind == "null-hierarchy";
        fixture.owner.poisonedGroup = kind == "wrong-group";
        fixture.owner.poisonedChild = kind == "wrong-child";
        fixture.owner.missingBlock = kind == "missing-block";
        const bool childPresenceCase = kind == "null-children" || kind == "children-uninitialized" || kind == "children-changed";
        if (childPresenceCase)
            fixture.owner.childrenPresent = false;
        if (kind == "children-uninitialized")
            fixture.owner.childrenResult = ClockResult::NotInitialized;
        if (kind == "children-changed")
            fixture.owner.childrenResult = ClockResult::BindingChanged;
        if (kind == "refused-group")
            fixture.owner.fail = "resolve";
        const bool refused = fixture.Refused(55);
        Check(refused ==
                  (kind == "wrong-group" || kind == "wrong-child" || kind == "refused-group" || kind == "children-uninitialized" || kind == "children-changed"),
              "malformed/refused owner facts never become an ordinary validity miss");
        Check(fixture.owner.Effects().empty(), "invalid/missing facts refuse or return before any native effect");
        Check(fixture.player.GetCustomMoveAnim() == 0 && fixture.owner.nativeGroup == 17 && fixture.owner.snapshot == 0,
              "observation failures preserve all real setter state");
        Check(!fixture.owner.Has("reapply") && !fixture.owner.Has("enable"), "unqualified group cannot enable hooks or run the original");
        if (childPresenceCase)
            Check(fixture.owner.Count("children") == 1 && fixture.owner.validReads == 0 && !fixture.owner.Has("id:0"),
                  "explicit absent child array or refused presence channel stops before any static-ID resolution");
    }
    void Affinity()
    {
        Fixture fixture(false), foreign(false);
        Check(Invalid([&] { fixture.player.FixtureConfigure(foreign.owner, foreign.manager); }), "foreign movement endpoint is rejected before any effect");
        Check(Invalid([&] { fixture.player.FixtureConfigure(fixture.owner, foreign.manager); }),
              "same numeric generation cannot hide foreign manager endpoints");
        Manager wrongGeneration(fixture.owner, fixture.owner, fixture.game, Generation + 1);
        Check(Invalid([&] { fixture.player.FixtureConfigure(fixture.owner, wrongGeneration); }), "foreign manager generation is rejected");
        Check(Invalid([&] { fixture.player.FixtureConfigureWithState(fixture.owner, fixture.manager, foreign.player); }),
              "different actual CPed state wrapper is rejected despite same owner shape");
        Check(fixture.owner.events.empty() && foreign.owner.events.empty(), "all constructor/configuration refusals occur before owner operations");
        fixture.player.FixtureConfigure(fixture.owner, fixture.manager);
        Check(Invalid([&] { fixture.player.FixtureConfigure(fixture.owner, fixture.manager); }), "configured context cannot be rebound");
        Check(static_cast<CPed*>(static_cast<CPlayerPed*>(&fixture.player)) == static_cast<CPed*>(static_cast<CPedStateDE*>(&fixture.player)),
              "actual setter and state share one virtual CPed subobject");
        fixture.Set(0);
        Check(fixture.owner.Effects() == std::vector<std::string>{"withdraw"}, "failed affinity probes do not poison later valid configured use");
    }
    void ModuleIntent()
    {
        {
            Fixture first;
            first.Set(55);
            Check(first.owner.refs == 1 && first.owner.finds == 1, "first real manager facade adds the module name reference once");
            Check(first.owner.snapshot == 55 && first.player.GetCustomMoveAnim() == 55, "first real wrapper publishes both custom values");
            first.Set(0);
            Check(first.owner.snapshot == 0 && first.owner.nativeGroup == 55, "clear withdraws custom identity without resetting native group");
            Check(first.owner.refs == 1, "clear does not undo accepted block reference intent");
        }
        Fixture second;
        Check(second.owner.events.empty(), "separate wrapper and manager lifetimes begin without effects");
        second.Set(58);
        Check(second.owner.finds == 1 && second.owner.identities == 1 && second.owner.refs == 0,
              "new manager's fresh real block facade sees module-wide name intent from the destroyed wrapper");
        Check(second.owner.Effects() == std::vector<std::string>{"enable", "publish", "write", "reapply"},
              "persistent name intent does not suppress fresh publication or whole reapply");
        Check(second.owner.snapshot == 58 && second.player.GetCustomMoveAnim() == 58 && second.owner.nativeGroup == 58,
              "separate actual wrapper state is synchronized independently");
    }
    void AlreadyLoadedBlock()
    {
        Fixture fixture;
        fixture.owner.present = false;
        fixture.owner.loaded = true;
        fixture.Set(138);
        Check(fixture.owner.finds == 1 && fixture.owner.identities == 1, "absent group pointer still creates the initial actual block facade");
        Check(fixture.owner.blockReads == 1, "initial facade checks its own loaded byte exactly once");
        Check(fixture.owner.Effects().empty(), "already loaded block cannot invent a request, reference, or group publication");
        Check(fixture.owner.Count("group-block") == 2 && fixture.owner.validReads == 0, "group pointer is rechecked and remains the eligibility fact");
        Check(fixture.owner.snapshot == 0 && fixture.player.GetCustomMoveAnim() == 0 && fixture.owner.nativeGroup == 17,
              "loaded byte alone never changes movement state");
        Check(fixture.game.modelLookups == 0 && fixture.owner.allTokens, "early return avoids streaming and retains owner identity");
    }
    void RequestRefusal()
    {
        Fixture fixture;
        fixture.owner.present = false;
        fixture.owner.fail = "ref";
        Check(fixture.Refused(138), "real Request AddRef refusal reaches the setter caller");
        Check(fixture.owner.Effects() == std::vector<std::string>{"request", "ref"}, "Request reference refusal leaves only the accepted request prefix");
        Check(fixture.owner.refs == 0, "failed Request AddRef has no fabricated successful reference");
        Check(fixture.owner.snapshot == 0 && fixture.player.GetCustomMoveAnim() == 0 && fixture.owner.nativeGroup == 17,
              "Request failure precedes all custom/native group state");
        Check(!fixture.owner.Has("load") && !fixture.owner.Has("enable"), "Request failure cannot advance to load or override publication");
        // This is a second explicit caller request against an acknowledging
        // double, not an SDK retry or a claim that uncertain owners may replay.
        fixture.Set(138);
        Check(fixture.owner.refs == 1 && fixture.owner.Count("ref") == 2, "Request name intent is recorded only after the actual Request returns successfully");
        Check(fixture.owner.finds == 3 && fixture.owner.identities == 3, "second explicit request uses fresh real initial and final block facades");
        Check(fixture.owner.snapshot == 138 && fixture.player.GetCustomMoveAnim() == 138 && fixture.owner.nativeGroup == 138 && fixture.owner.allTokens,
              "successful later Request synchronizes the same registered wrapper without an extra final-facade reference");
    }
}
static_assert(std::is_abstract_v<mta::game_de::CPlayerPedMoveAnimDE>);
static_assert(std::is_abstract_v<mta::game_de::CAnimManagerDE>);
static_assert(std::is_abstract_v<mta::game_de::CStreamingDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPlayerPedMoveAnimDE::SetMoveAnim), void (mta::game_de::CPlayerPedMoveAnimDE::*)(eMoveAnim)>);

int main(int argc, char** argv)
{
    try
    {
        if (checks != 0 || argc != 2)
            throw std::runtime_error("Expected one independent setter case and no historical cases");
        const std::string name(argv[1]);
        if (name == "invalid-default")
            setter_fixture::InvalidDefault();
        else if (name == "loaded")
            setter_fixture::Loaded();
        else if (name == "request")
            setter_fixture::Request(true);
        else if (name == "request-incomplete")
            setter_fixture::Request(false);
        else if (name == "affinity")
            setter_fixture::Affinity();
        else if (name == "module-intent")
            setter_fixture::ModuleIntent();
        else if (name == "already-loaded-block")
            setter_fixture::AlreadyLoadedBlock();
        else if (name == "request-refusal-ref")
            setter_fixture::RequestRefusal();
        else if (name.rfind("prefix-", 0) == 0)
            setter_fixture::Prefix(name.substr(7));
        else if (name.rfind("observe-", 0) == 0)
            setter_fixture::Observation(name.substr(8));
        else
            throw std::runtime_error("Unknown focused setter case");
        std::cout << "Actual inherited player move animation setter SDK case completed: " << name << '\n';
        std::cout << "Passed " << checks << " player move animation setter SDK checks (" << sizeof(void*) * 8 << "-bit; no native engine code executed)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
