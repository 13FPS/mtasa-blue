/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual player/ped SDK composition over one DE implementation base
 ****************************************************************************/
// Reuse only the strict existing game/complete-ped fixture definitions. Its
// historical main remains compiled under another name and is never executed.
#pragma push_macro("main")
#undef main
#define main HistoricalClearWeaponDefinitionsMain
#include "PedClearWeaponDETests.cpp"
#undef main
#pragma pop_macro("main")
#include "../../Client/game_de/CPlayerPedWantedDE.h"
#include "../../Client/game_de/CPedRebuildDE.h"
#include <game/CWanted.h>
namespace ped_rebuild_fixture
{
    using ped_clear_weapon_fixture::Unqualified;
}
#include "PedRebuildFixture.generated.h"
#include <cstring>

namespace shared_player_base_fixture
{
    // The actual classic player uses this same virtual SDK diamond and
    // game_sa/StdInc.h disables its expected MSVC dominance warning. Keep
    // that compatibility local to this one composed test declaration.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4250)
#endif
    class Player final : public ped_rebuild_fixture::PedRebuildFixture, public mta::game_de::CPlayerPedWantedDE
    {
    public:
        Player(CGame& game, mta::game_de::NativePedVitalsAccess& vitals, mta::game_de::NativePedMetadataAccess& metadata,
               mta::game_de::NativePedMutationAccess& mutation, mta::game_de::NativePedObservationAccess& observation,
               mta::game_de::NativePedRotationAccess& rotation, mta::game_de::NativePedOxygenAccess& oxygen, mta::game_de::NativePedDuckingAccess& ducking,
               mta::game_de::NativePedBikeFlagsAccess& bike, mta::game_de::NativePedShotFlagsAccess& shot, mta::game_de::NativePedBleedingAccess& bleeding,
               mta::game_de::NativePedStayAccess& stay, mta::game_de::NativePedFightingStyleAccess& combat,
               mta::game_de::NativePedCurrentWeaponSlotAccess& selection, mta::game_de::NativePedPlacementAccess& placement,
               mta::game_de::NativePedWaterFlagsAccess& water, mta::game_de::NativePedWaterTaskAccess& waterTask,
               mta::game_de::NativePedOrientationAccess& orientation, mta::game_de::NativePedAreaAccess& area, mta::game_de::NativePedFireAccess& fire,
               mta::game_de::NativePedCollisionAccess& collision, mta::game_de::NativePedGogglesAccess& goggles,
               mta::game_de::NativePedVisibilityAccess& visibility, mta::game_de::NativePedOnScreenAccess& onScreen,
               mta::game_de::NativePedWeaponGrantAccess& grant, mta::game_de::NativePedWeaponClearAccess& clear, mta::game_de::NativePedClothesAccess& clothes,
               mta::game_de::NativePedRebuildAccess& rebuild, const mta::game_de::NativePedReferenceToken& token, std::uint32_t wrapperType,
               ped_rebuild_fixture::TaskScenario& scenario, ped_rebuild_fixture::WeaponLookup& lookup, CWanted* wanted) noexcept
            : PedRebuildFixture(game, vitals, metadata, mutation, observation, rotation, oxygen, ducking, bike, shot, bleeding, stay, combat, selection,
                                placement, water, waterTask, orientation, area, fire, collision, goggles, visibility, onScreen, grant, clear, clothes, rebuild,
                                token, wrapperType, scenario, lookup),
              CPlayerPedWantedDE(wanted)
        {
        }

        void      SetInitialState() override { ped_clear_weapon_fixture::Unqualified("Player::SetInitialState"); }
        eMoveAnim GetMoveAnim() override { ped_clear_weapon_fixture::Unqualified("Player::GetMoveAnim"); }
        void      SetMoveAnim(eMoveAnim value) override
        {
            (void)value;
            ped_clear_weapon_fixture::Unqualified("Player::SetMoveAnim");
        }
        CEntity* GetTargetedEntity() const override { ped_clear_weapon_fixture::Unqualified("Player::GetTargetedEntity"); }
        void     SetTargetedEntity(CEntity* value) override
        {
            (void)value;
            ped_clear_weapon_fixture::Unqualified("Player::SetTargetedEntity");
        }
    };
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
    static_assert(!std::is_abstract_v<Player>);
    // A nonvirtual CPedVitalsDE plus actual CPlayerPed fails this conversion:
    // the complete receiver would contain two independent CPed subobjects.
    static_assert(std::is_convertible_v<Player*, CPed*>);
    static_assert(std::is_convertible_v<const Player*, const CPed*>);
    static_assert(std::is_base_of_v<mta::game_de::CPedRebuildDE, Player>);
    static_assert(std::is_abstract_v<mta::game_de::CPedRebuildDE>);

    struct WantedCounters
    {
        unsigned constructed = 0, destroyed = 0, methods = 0;
    };
    class WantedView final : public CWanted
    {
    public:
        explicit WantedView(WantedCounters& value) noexcept : counters(value) { ++counters.constructed; }
        ~WantedView() { ++counters.destroyed; }
        void SetWantedLevel(DWORD value) override
        {
            (void)value;
            ++counters.methods;
            ped_clear_weapon_fixture::Unqualified("Wanted::SetWantedLevel");
        }
        char GetWantedLevel() override
        {
            ++counters.methods;
            ped_clear_weapon_fixture::Unqualified("Wanted::GetWantedLevel");
        }
        void SetWantedLevelNoFlash(DWORD value) override
        {
            (void)value;
            ++counters.methods;
            ped_clear_weapon_fixture::Unqualified("Wanted::SetWantedLevelNoFlash");
        }

    private:
        WantedCounters& counters;
    };
    struct Clothes final : mta::game_de::NativePedClothesAccess
    {
    };
    struct Rebuild final : mta::game_de::NativePedRebuildAccess
    {
    };
    static_assert(std::is_abstract_v<mta::game_de::CPlayerPedWantedDE>);
    static_assert(std::is_same_v<decltype(&mta::game_de::CPlayerPedWantedDE::GetWanted), CWanted* (mta::game_de::CPlayerPedWantedDE::*)()>);

    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    float Float(std::uint32_t bits) noexcept
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    struct Vitals final : mta::game_de::NativePedVitalsAccess
    {
        unsigned                              reads = 0, writes = 0;
        mta::game_de::NativePedVitalField     field = mta::game_de::NativePedVitalField::Health;
        mta::game_de::NativePedReferenceToken token{};
        mta::game_de::ClockResult             status = mta::game_de::ClockResult::Ok;
        std::uint32_t                         payload = 0x80000000u;
        mta::game_de::ClockResult ReadPedVital(const mta::game_de::NativePedReferenceToken& selected, mta::game_de::NativePedVitalField selectedField,
                                               float& output) noexcept override
        {
            ++reads;
            token = selected;
            field = selectedField;
            if (status == mta::game_de::ClockResult::Ok)
                output = Float(payload);
            return status;
        }
        mta::game_de::ClockResult SetPedVital(const mta::game_de::NativePedReferenceToken& selected, mta::game_de::NativePedVitalField selectedField,
                                              float value) noexcept override
        {
            ++writes;
            token = selected;
            field = selectedField;
            payload = Bits(value);
            return status;
        }
    };
}

int main()
{
    try
    {
        using namespace shared_player_base_fixture;
        ped_rebuild_fixture::Game         game;
        ped_rebuild_fixture::TaskScenario tasks;
        ped_rebuild_fixture::WeaponLookup lookup;
        OtherOwners                       other;
        Grant                             grant;
        Clear                             clear;
        Vitals                            vitals;
        NativePedReferenceToken           caller{0xfedcba9876543210ull, 0x8000000000000001ull, 37u};
        const auto                        expected = caller;
        Clothes                           clothes;
        Rebuild                           rebuild;
        WantedCounters                    wantedCounters;
        WantedView                        wantedView(wantedCounters);
        const auto                        makePlayer = [&](CWanted* view)
        {
            return Player(game, vitals, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other,
                          other, other, other, other, other, grant, clear, clothes, rebuild, caller, 7u, tasks, lookup, view);
        };
        Player            player = makePlayer(&wantedView);
        CPed* const       throughImplementation = static_cast<CPed*>(static_cast<mta::game_de::CPedRebuildDE*>(&player));
        CPlayerPed* const playerSdk = &player;
        CPed* const       throughPlayer = playerSdk;
        // These are explicitly supplied CPU views. The fixture does not
        // resolve a local native wanted object for either player or supply a
        // production player factory; every wanted operation remains a rejector.
        Check(playerSdk->GetWanted() == &wantedView && dynamic_cast<CPlayerPed*>(throughImplementation)->GetWanted() == &wantedView &&
                  wantedCounters.constructed == 1 && !wantedCounters.methods && !wantedCounters.destroyed && !vitals.reads && !vitals.writes && !clear.calls,
              "the actual player wanted layer exposes only its explicit borrowed CPU association through the same shared ped identity");
        {
            auto absent = makePlayer(nullptr);
            auto peer = makePlayer(&wantedView);
            Check(static_cast<CPlayerPed&>(absent).GetWanted() == nullptr && static_cast<CPlayerPed&>(peer).GetWanted() == &wantedView &&
                      playerSdk->GetWanted() == &wantedView && !wantedCounters.methods && !wantedCounters.destroyed,
                  "explicit null and shared views remain per-wrapper associations without invoking wanted methods or a global native lookup");
        }
        Check(!wantedCounters.destroyed && !wantedCounters.methods && playerSdk->GetWanted() == &wantedView,
              "retiring peer/null wrappers neither destroys their borrowed wanted view nor changes another player's CPU association");
        Check(static_cast<CPed*>(&player) == throughImplementation && throughImplementation == throughPlayer,
              "the actual complete test player has one unambiguous common CPed identity through both SDK branches");
        Check(static_cast<CPhysical*>(throughImplementation) == static_cast<CPhysical*>(throughPlayer) &&
                  static_cast<CEntity*>(throughImplementation) == static_cast<CEntity*>(throughPlayer),
              "the common physical/entity virtual bases are shared through both player and implementation paths");
        Check(dynamic_cast<CPlayerPed*>(throughImplementation) == playerSdk &&
                  dynamic_cast<mta::game_de::CPedVitalsDE*>(throughPlayer) == static_cast<mta::game_de::CPedVitalsDE*>(&player) &&
                  dynamic_cast<mta::game_de::CPedRebuildDE*>(throughPlayer) == static_cast<mta::game_de::CPedRebuildDE*>(&player),
              "actual common SDK RTTI reaches each sibling implementation from the single adjusted ped identity");
        Check(dynamic_cast<void*>(throughPlayer) == static_cast<void*>(&player), "the adjusted virtual SDK pointer retains its complete C++ receiver identity");
        Check(!vitals.reads && !vitals.writes && !clear.calls && !grant.calls && !tasks.eventCount && !lookup.count,
              "composition borrows existing owners without executing native initialization, grants, task work or weapon lookups");
        caller = {};
        Check(Bits(throughPlayer->GetHealth()) == 0x80000000u && vitals.reads == 1 && vitals.field == NativePedVitalField::Health &&
                  SameToken(vitals.token, expected),
              "the player SDK path dispatches actual vitals with the implementation's immutable copied token and raw negative zero");
        vitals.payload = 0x7f800000u;
        Check(Bits(throughImplementation->GetArmor()) == 0x7f800000u && vitals.reads == 2 && vitals.field == NativePedVitalField::Armor &&
                  SameToken(vitals.token, expected),
              "the implementation SDK path reaches that same borrowed vitals owner and preserves raw infinity");
        throughPlayer->SetHealth(Float(0x7fc12345u));
        Check(vitals.writes == 1 && vitals.payload == 0x7fc12345u && vitals.field == NativePedVitalField::Health && SameToken(vitals.token, expected),
              "a player-side setter uses the existing actual vitals implementation without a separate player cache or clamp");
        int storedIdentity = 0;
        throughImplementation->SetStoredPointer(&storedIdentity);
        Check(throughPlayer->GetStoredPointer() == &storedIdentity && throughPlayer->GetType() == 7u && throughImplementation->GetType() == 7u,
              "both SDK branches observe the same real implementation bookkeeping and wrapper type");
        clear.forbiddenBaseAddress = player.FixtureTokenAddress();
        clear.forbiddenCallerAddress = &caller;
        throughPlayer->ClearWeapon(WEAPONTYPE_CAMERA);
        Check(clear.calls == 1 && clear.type == static_cast<std::uint32_t>(WEAPONTYPE_CAMERA) && SameToken(clear.received, expected) && !clear.aliased,
              "the player path dispatches the actual ClearWeapon chain once with its copied owner token");
        throughImplementation->ClearWeapon(WEAPONTYPE_PISTOL);
        Check(clear.calls == 2 && clear.type == static_cast<std::uint32_t>(WEAPONTYPE_PISTOL) && SameToken(clear.received, expected) && !grant.calls &&
                  !lookup.count,
              "the implementation path reaches the same clear owner without granting or manufacturing a weapon wrapper");
        vitals.status = ClockResult::ReadFailure;
        bool refused = false;
        try
        {
            (void)throughPlayer->GetHealth();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find("GetHealth") != std::string::npos;
        }
        Check(refused && vitals.reads == 3 && SameToken(vitals.token, expected),
              "a refused real owner dispatch remains explicit through the common player view without another lookup or retry");
        clear.result = ClockResult::Busy;
        Check(Refused(*throughPlayer, WEAPONTYPE_CAMERA, ClockResult::Busy) && clear.calls == 3,
              "the actual clear refusal crosses the shared player view unchanged after one attempt");
        throughPlayer->SetStoredPointer(nullptr);
        Check(throughImplementation->GetStoredPointer() == nullptr && !grant.calls && !tasks.eventCount && !lookup.count,
              "the composition adds no independent bookkeeping, factory, native lifetime, task or weapon-bank activity");
        Check(playerSdk->GetWanted() == &wantedView && !wantedCounters.methods && !wantedCounters.destroyed && vitals.reads == 3 && clear.calls == 3,
              "native-operation refusal does not turn a cached CPU wanted association into an engine lease or call its methods");
        std::puts("Actual borrowed player wanted association fixture completed");
        std::puts("Actual shared player CPed base fixture completed");
        std::printf("Passed %u actual shared player CPed base checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
