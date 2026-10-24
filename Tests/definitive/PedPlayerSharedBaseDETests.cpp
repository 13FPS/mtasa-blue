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
#include <game/CPlayerPed.h>
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
    class Player final : public ped_clear_weapon_fixture::PedClearWeaponFixture, public virtual CPlayerPed
    {
    public:
        using PedClearWeaponFixture::PedClearWeaponFixture;
        CWanted*  GetWanted() override { ped_clear_weapon_fixture::Unqualified("Player::GetWanted"); }
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
    static_assert(std::is_base_of_v<mta::game_de::CPedClearWeaponDE, Player>);
    static_assert(std::is_abstract_v<mta::game_de::CPedClearWeaponDE>);

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
        ped_clear_weapon_fixture::Game         game;
        ped_clear_weapon_fixture::TaskScenario tasks;
        ped_clear_weapon_fixture::WeaponLookup lookup;
        OtherOwners                            other;
        Grant                                  grant;
        Clear                                  clear;
        Vitals                                 vitals;
        NativePedReferenceToken                caller{0xfedcba9876543210ull, 0x8000000000000001ull, 37u};
        const auto                             expected = caller;
        Player      player(game, vitals, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other, other,
                           other, other, other, other, other, grant, clear, caller, 7u, tasks, lookup);
        CPed* const throughImplementation = static_cast<CPed*>(static_cast<mta::game_de::CPedClearWeaponDE*>(&player));
        CPlayerPed* const playerSdk = &player;
        CPed* const       throughPlayer = playerSdk;
        Check(static_cast<CPed*>(&player) == throughImplementation && throughImplementation == throughPlayer,
              "the actual complete test player has one unambiguous common CPed identity through both SDK branches");
        Check(static_cast<CPhysical*>(throughImplementation) == static_cast<CPhysical*>(throughPlayer) &&
                  static_cast<CEntity*>(throughImplementation) == static_cast<CEntity*>(throughPlayer),
              "the common physical/entity virtual bases are shared through both player and implementation paths");
        Check(dynamic_cast<CPlayerPed*>(throughImplementation) == playerSdk &&
                  dynamic_cast<mta::game_de::CPedVitalsDE*>(throughPlayer) == static_cast<mta::game_de::CPedVitalsDE*>(&player) &&
                  dynamic_cast<mta::game_de::CPedClearWeaponDE*>(throughPlayer) == static_cast<mta::game_de::CPedClearWeaponDE*>(&player),
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
