/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual CPed GiveWeapon delegates MTA preparation and slot ownership
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CPedGiveWeaponDE.h"
#include <game/CGame.h>
#include <game/CModelInfo.h>
#include <game/CPedIK.h>
#include <game/CWeapon.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace ped_give_weapon_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified GiveWeapon fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "PedGiveWeaponFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    enum class Kind
    {
        Info,
        ModelId,
        Model,
        Request,
        Custom,
        Grant
    };
    struct Event
    {
        Kind          kind;
        std::uint32_t first, second;
    };
    struct Trace
    {
        std::array<Event, 64>    events{};
        std::size_t              count = 0;
        bool                     badTag = false;
        int                      throwKind = -1;
        NativePedReferenceToken* callerToken = nullptr;
        void                     Record(Kind kind, std::uint32_t first = 0, std::uint32_t second = 0) noexcept
        {
            if (count == events.size())
                std::abort();
            events[count++] = {kind, first, second};
        }
        void Prepared(Kind kind, std::uint32_t first = 0, std::uint32_t second = 0)
        {
            Record(kind, first, second);
            if (static_cast<int>(kind) == throwKind)
                throw std::runtime_error("preparation-prefix");
        }
        void Reset() noexcept
        {
            count = 0;
            badTag = false;
            throwKind = -1;
        }
    };
    struct Info final : ped_give_weapon_fixture::InfoBase
    {
        Trace&       trace;
        eWeaponModel model;
        Info(Trace& selected, eWeaponModel value) noexcept : trace(selected), model(value) {}
        eWeaponModel GetModel() override
        {
            trace.Prepared(Kind::ModelId, static_cast<std::uint32_t>(model));
            return model;
        }
    };
    struct Model final : ped_give_weapon_fixture::ModelBase
    {
        Trace&        trace;
        std::uint32_t id;
        Model(Trace& selected, std::uint32_t value) noexcept : trace(selected), id(value) {}
        void Request(EModelRequestType flags, const char* tag) override
        {
            trace.badTag = trace.badTag || !tag || std::strcmp(tag, "CPedSA::GiveWeapon") != 0;
            trace.Prepared(Kind::Request, id, static_cast<std::uint32_t>(flags));
        }
        void MakeCustomModel() override { trace.Prepared(Kind::Custom, id); }
    };
    struct Game final : ped_give_weapon_fixture::GameBase
    {
        Trace& trace;
        Info   ordinary, detonator;
        Model  ordinaryModel, detonatorModel;
        int    missingInfo = -1;
        bool   missingModels = false;
        explicit Game(Trace& selected) noexcept
            : trace(selected),
              ordinary(selected, static_cast<eWeaponModel>(348)),
              detonator(selected, static_cast<eWeaponModel>(364)),
              ordinaryModel(selected, 348),
              detonatorModel(selected, 364)
        {
        }
        CWeaponInfo* GetWeaponInfo(eWeaponType type, eWeaponSkill skill) override
        {
            trace.Prepared(Kind::Info, static_cast<std::uint32_t>(type), static_cast<std::uint32_t>(skill));
            if (trace.callerToken)
                *trace.callerToken = {};
            if (static_cast<int>(type) == missingInfo)
                return nullptr;
            return type == WEAPONTYPE_DETONATOR ? &detonator : &ordinary;
        }
        CModelInfo* GetModelInfo(DWORD model, bool canBeInvalid) override
        {
            trace.Prepared(Kind::Model, model, canBeInvalid ? 1u : 0u);
            if (missingModels)
                return nullptr;
            return model == 364 ? &detonatorModel : &ordinaryModel;
        }
    };
    struct OtherOwners : NativePedVitalsAccess,
                         NativePedMetadataAccess,
                         NativePedMutationAccess,
                         NativePedObservationAccess,
                         NativePedRotationAccess,
                         NativePedOxygenAccess,
                         NativePedDuckingAccess,
                         NativePedBikeFlagsAccess,
                         NativePedShotFlagsAccess,
                         NativePedBleedingAccess,
                         NativePedStayAccess,
                         NativePedFightingStyleAccess,
                         NativePedCurrentWeaponSlotAccess,
                         NativePedPlacementAccess,
                         NativePedWaterFlagsAccess,
                         NativePedWaterTaskAccess,
                         NativePedOrientationAccess,
                         NativePedAreaAccess,
                         NativePedFireAccess,
                         NativePedCollisionAccess,
                         NativePedGogglesAccess,
                         NativePedVisibilityAccess,
                         NativePedOnScreenAccess
    {
    };
    struct Grant final : NativePedWeaponGrantAccess
    {
        Trace&                                  trace;
        ClockResult                             result = ClockResult::Ok;
        int                                     refusedType = -1;
        std::uint32_t                           slot = 2, detonatorSlot = 12;
        unsigned                                calls = 0;
        std::array<NativePedReferenceToken, 32> received{};
        const NativePedReferenceToken*          forbiddenAddress = nullptr;
        bool                                    aliasedToken = false;
        explicit Grant(Trace& selected) noexcept : trace(selected) {}
        ClockResult GrantPedWeapon(const NativePedReferenceToken& token, eWeaponType type, std::uint32_t ammo, std::uint32_t& output) noexcept override
        {
            if (calls == received.size())
                std::abort();
            received[calls++] = token;
            aliasedToken = aliasedToken || &token == forbiddenAddress;
            trace.Record(Kind::Grant, static_cast<std::uint32_t>(type), ammo);
            // A staged slot on refusal must never reach the still-pure bank
            // lookup. Already accepted model requests belong to their owner.
            output = type == WEAPONTYPE_DETONATOR ? detonatorSlot : slot;
            return refusedType == -1 || refusedType == static_cast<int>(type) ? result : ClockResult::Ok;
        }
        void Reset() noexcept
        {
            calls = 0;
            aliasedToken = false;
        }
    };
    struct UnavailableGrant final : NativePedWeaponGrantAccess
    {
    };
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second) noexcept
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    ped_give_weapon_fixture::PedGiveWeaponFixture MakePed(CGame& game, OtherOwners& other, NativePedWeaponGrantAccess& grant,
                                                          const NativePedReferenceToken& token, ped_give_weapon_fixture::TaskScenario& tasks,
                                                          ped_give_weapon_fixture::WeaponLookup& lookup)
    {
        return ped_give_weapon_fixture::PedGiveWeaponFixture(game, other, other, other, other, other, other, other, other, other, other, other, other, other,
                                                             other, other, other, other, other, other, other, other, other, other, grant, token, 7u, tasks,
                                                             lookup);
    }
    void ExpectKinds(const Trace& trace, std::initializer_list<Kind> expected)
    {
        Check(trace.count == expected.size(), "actual SDK retained the expected preparation/native prefix length");
        std::size_t index = 0;
        for (const auto kind : expected)
            Check(trace.events[index++].kind == kind, "actual SDK reused MTA callback ordering");
    }
    bool Refused(CPed& ped, eWeaponType type, ClockResult expected, CWeapon*& output)
    {
        try
        {
            output = ped.GiveWeapon(type, 0xffffffffu, WEAPONSKILL_STD);
        }
        catch (const std::runtime_error& error)
        {
            const std::string text(error.what());
            return text.find("GiveWeapon") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(expected))) != std::string::npos;
        }
        return false;
    }
}

static_assert(std::is_abstract_v<mta::game_de::CPedGiveWeaponDE>);
static_assert(std::is_base_of_v<mta::game_de::CPedOnScreenDE, mta::game_de::CPedGiveWeaponDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CPedGiveWeaponDE::GiveWeapon),
                             CWeapon* (mta::game_de::CPedGiveWeaponDE::*)(eWeaponType, std::uint32_t, eWeaponSkill)>);
static_assert(sizeof(eWeaponType) == 4 && sizeof(eWeaponSlot) == 4 && sizeof(eWeaponSkill) == 1);

int main()
{
    try
    {
        Trace                                 trace;
        Game                                  game(trace);
        OtherOwners                           other;
        Grant                                 first(trace), second(trace);
        ped_give_weapon_fixture::TaskScenario tasks;
        ped_give_weapon_fixture::WeaponLookup lookup;
        ped_give_weapon_fixture::Weapon       weapon, detonator;
        lookup.bank[2] = &weapon;
        lookup.bank[12] = &detonator;
        NativePedReferenceToken     callerToken{0x123456789abcdef0ull, 0xfedcba9876543210ull, 17};
        const auto                  expected = callerToken;
        NativePedWeaponGrantAccess* selected = &first;
        auto                        ped = MakePed(game, other, *selected, callerToken, tasks, lookup);
        selected = &second;
        first.forbiddenAddress = ped.FixtureTokenAddress();
        trace.callerToken = &callerToken;
        Check(!trace.count && !first.calls && !lookup.count && !tasks.eventCount,
              "constructing an abstract SDK test receiver borrows owners without preparing models or native grants");
        CPed& sdk = ped;
        Check(sdk.GiveWeapon(WEAPONTYPE_DESERT_EAGLE, 0xffffffffu, static_cast<eWeaponSkill>(255)) == &weapon,
              "actual CPed dispatch returns the pointer selected by the existing slot lookup");
        ExpectKinds(trace, {Kind::Info, Kind::ModelId, Kind::Model, Kind::Request, Kind::Custom, Kind::Grant});
        Check(trace.events[0].first == WEAPONTYPE_DESERT_EAGLE && trace.events[0].second == 255 && trace.events[2].first == 348 && !trace.events[2].second &&
                  trace.events[3].second == BLOCKING && trace.events[5].second == 0xffffffffu && !trace.badTag,
              "genuine skill, model lookup default, typed BLOCKING, original request tag and uncapped ammo survive SDK wiring");
        Check(first.calls == 1 && SameToken(first.received[0], expected) && !first.aliasedToken && !second.calls && lookup.count == 1 && lookup.slots[0] == 2 &&
                  !tasks.eventCount,
              "grant uses its borrowed owner and a local immutable token snapshot despite caller-token mutation; no task/equip fallback");
        trace.callerToken = nullptr;

        for (const std::uint32_t slot : {0u, 12u, 13u, 0x80000000u, 0xffffffffu})
        {
            trace.Reset();
            first.Reset();
            lookup.count = 0;
            first.slot = slot;
            const auto returned = static_cast<CPedGiveWeaponDE&>(ped).GiveWeapon(WEAPONTYPE_UNARMED, 0x80000000u, WEAPONSKILL_POOR);
            Check(returned == (slot < lookup.bank.size() ? lookup.bank[slot] : nullptr) && lookup.count == 1 && lookup.slots[0] == slot && first.calls == 1 &&
                      trace.events[0].second == 0x80000000u,
                  "raw returned DWORD reaches actual eWeaponSlot lookup once, including null/out-of-range; SDK invents no wrapper or ammo clamp");
            ExpectKinds(trace, {Kind::Grant});
        }

        trace.Reset();
        first.Reset();
        lookup.count = 0;
        first.slot = 2;
        Check(sdk.GiveWeapon(WEAPONTYPE_REMOTE_SATCHEL_CHARGE, 999999u, WEAPONSKILL_PRO) == &weapon,
              "outer Satchel returns its own wrapper after recursive detonator completion");
        ExpectKinds(trace, {Kind::Info, Kind::ModelId, Kind::Model, Kind::Request, Kind::Custom, Kind::Info, Kind::ModelId, Kind::Model, Kind::Request,
                            Kind::Custom, Kind::Grant, Kind::Grant});
        Check(trace.events[5].first == WEAPONTYPE_DETONATOR && trace.events[5].second == WEAPONSKILL_STD && trace.events[10].first == WEAPONTYPE_DETONATOR &&
                  trace.events[10].second == 1 && trace.events[11].first == WEAPONTYPE_REMOTE_SATCHEL_CHARGE && trace.events[11].second == 999999u &&
                  lookup.count == 2 && lookup.slots[0] == 12 && lookup.slots[1] == 2 && first.calls == 2 && SameToken(first.received[0], expected) &&
                  SameToken(first.received[1], expected) && !first.aliasedToken,
              "actual recursive SDK call uses detonator1/STD and the same immutable registration before the outer raw grant");

        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            trace.Reset();
            first.Reset();
            lookup.count = 0;
            first.result = result;
            CWeapon* output = &detonator;
            Check(Refused(sdk, WEAPONTYPE_DESERT_EAGLE, result, output) && output == &detonator && first.calls == 1 && !lookup.count && !second.calls,
                  "each owner refusal preserves caller output and accepted preparation, names its status, and skips wrapper lookup/retry");
            ExpectKinds(trace, {Kind::Info, Kind::ModelId, Kind::Model, Kind::Request, Kind::Custom, Kind::Grant});
        }
        trace.Reset();
        first.Reset();
        lookup.count = 0;
        first.refusedType = WEAPONTYPE_DETONATOR;
        first.result = ClockResult::BindingChanged;
        CWeapon* output = &detonator;
        Check(Refused(sdk, WEAPONTYPE_REMOTE_SATCHEL_CHARGE, first.result, output) && output == &detonator && first.calls == 1 && !lookup.count &&
                  trace.events[trace.count - 1].first == WEAPONTYPE_DETONATOR,
              "recursive refusal retains both accepted model preparations and stops before the outer grant without a fake detonator wrapper");

        first.result = ClockResult::Ok;
        first.refusedType = -1;
        for (const auto stage : {Kind::Info, Kind::ModelId, Kind::Model, Kind::Request, Kind::Custom})
        {
            trace.Reset();
            first.Reset();
            lookup.count = 0;
            trace.throwKind = static_cast<int>(stage);
            output = &detonator;
            bool prefix = false;
            try
            {
                output = sdk.GiveWeapon(WEAPONTYPE_DESERT_EAGLE, 1, WEAPONSKILL_STD);
            }
            catch (const std::runtime_error& error)
            {
                prefix = std::strcmp(error.what(), "preparation-prefix") == 0;
            }
            Check(prefix && output == &detonator && !first.calls && !lookup.count && trace.events[trace.count - 1].kind == stage,
                  "actual game/info/model preparation exceptions keep their exact prefix and never trigger a grant, rollback or wrapper lookup");
        }
        trace.Reset();
        first.Reset();
        lookup.count = 0;
        Check(sdk.GiveWeapon(WEAPONTYPE_DESERT_EAGLE, 7, WEAPONSKILL_STD) == &weapon && first.calls == 1 && lookup.count == 1,
              "a fresh accepted invocation after refusal returns its own current slot rather than stale staged output");

        Trace                                 anotherTrace;
        Game                                  anotherGame(anotherTrace);
        Grant                                 anotherGrant(anotherTrace);
        ped_give_weapon_fixture::WeaponLookup anotherLookup;
        anotherLookup.bank[2] = &detonator;
        const NativePedReferenceToken nextToken{91, 92, 93};
        auto                          another = MakePed(anotherGame, other, anotherGrant, nextToken, tasks, anotherLookup);
        const auto                    firstBefore = first.calls;
        Check(static_cast<CPed&>(another).GiveWeapon(WEAPONTYPE_UNARMED, 3, WEAPONSKILL_STD) == &detonator && anotherGrant.calls == 1 &&
                  SameToken(anotherGrant.received[0], nextToken) && first.calls == firstBefore,
              "separate wrapper/game/grant/bank ownership cannot be replaced by a global current-player receiver");
        UnavailableGrant unavailable;
        auto             missing = MakePed(game, other, unavailable, expected, tasks, lookup);
        trace.Reset();
        lookup.count = 0;
        output = &detonator;
        Check(Refused(missing, WEAPONTYPE_UNARMED, ClockResult::NotInitialized, output) && output == &detonator && !lookup.count && !trace.count,
              "default native grant refuses explicitly and publishes no readiness flag, native weapon or fallback bank");
        std::puts("Actual ped GiveWeapon SDK fixture completed");
        std::printf("Passed %u actual ped GiveWeapon SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped GiveWeapon SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
