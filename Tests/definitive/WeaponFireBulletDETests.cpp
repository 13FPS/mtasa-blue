/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual FireBullet SDK itinerary through borrowed genuine services
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include "../../Client/game_de/CWeaponFireBulletDE.h"
#include "../../Client/game_de/NativeGameTimingAccess.h"
#include <CVector.h>
#include <CVector2D.h>
// The legacy SDK forwards eBone and contains anonymous SurfaceInfo unions.
// Keep those existing declarations local rather than weakening new code.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4471 4201)
#endif
#include <game/CPlayerPed.h>
#include <game/CWorld.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
#include <game/CWeaponStatManager.h>
#include <game/CPedIK.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace weapon_fire_bullet_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified FireBullet SDK fixture: %s\n", method);
        std::abort();
    }
}
#include "WeaponFireBulletFixture.generated.h"

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
    enum class Step
    {
        Type,
        World,
        Ignore,
        PreView,
        Pre,
        Manager,
        Stats,
        Offset,
        Transform,
        Native,
        Audio,
        PostView,
        Post,
        Clear
    };
    struct Event
    {
        Step          step;
        std::uint32_t first = 0;
        bool          operator==(const Event& other) const noexcept { return step == other.step && first == other.first; }
    };
    struct Scenario
    {
        std::array<Event, 64>   events{};
        std::size_t             count = 0;
        int                     failAt = -1;
        unsigned                typeCalls = 0, nativeCalls = 0, preCalls = 0, postCalls = 0, audioCalls = 0, transforms = 0;
        std::uint32_t           type = WEAPONTYPE_PISTOL;
        bool                    preEnabled = true, postEnabled = true, disablePostInPre = false, enablePostInAudio = false;
        bool                    changeTypeInPre = false, changeTypeInManager = false, nativeReturn = false;
        bool                    nativeMutates = false, badArguments = false, aliasMuzzle = false;
        bool                    refuseSecondType = false;
        ClockResult             typeResult = ClockResult::Ok;
        CEntity*                ignored = nullptr;
        CPlayerPed*             player = nullptr;
        CVector                 offset{1.25f, -2.5f, 3.75f}, muzzleReceived;
        CVector*                expectedOrigin = nullptr;
        CVector*                expectedTarget = nullptr;
        CEntity*                expectedFirer = nullptr;
        NativePedReferenceToken expectedToken{17, 29, 31}, receivedToken;
        std::uint32_t           receivedSlot = 0;
        explicit Scenario(const NativePedReferenceToken& token = NativePedReferenceToken{17, 29, 31}) noexcept : expectedToken(token) {}
        void RecordNoThrow(Step step, std::uint32_t value = 0) noexcept
        {
            if (count == events.size())
                std::abort();
            events[count++] = {step, value};
        }
        void Record(Step step, std::uint32_t value = 0)
        {
            RecordNoThrow(step, value);
            if (failAt >= 0 && count == static_cast<std::size_t>(failAt + 1))
                throw std::runtime_error("sdk-prefix-" + std::to_string(failAt));
        }
        void Expect(CEntity* firing, CVector& origin, CVector& target) noexcept
        {
            expectedFirer = firing;
            expectedOrigin = &origin;
            expectedTarget = &target;
        }
    };
    // The CGame API registers plain function pointers. This narrow CPU router
    // supplies the actual callback signature; it never becomes backend state.
    thread_local Scenario* callbackScenario = nullptr;
    bool                   Pre(CPlayerPed* player, bool stopForBulletSync)
    {
        auto& current = *callbackScenario;
        current.Record(Step::Pre);
        current.badArguments = current.badArguments || player != current.player || stopForBulletSync;
        ++current.preCalls;
        if (current.changeTypeInPre)
            current.type = WEAPONTYPE_ROCKETLAUNCHER;
        if (current.disablePostInPre)
            current.postEnabled = false;
        return false;
    }
    void Post()
    {
        callbackScenario->Record(Step::Post);
        ++callbackScenario->postCalls;
    }
    struct Handlers final : WeaponFireHandlerAccess
    {
        Scenario& current;
        explicit Handlers(Scenario& selected) noexcept : current(selected) {}
        PreWeaponFireHandler* GetPreWeaponFireHandler() const noexcept override
        {
            current.RecordNoThrow(Step::PreView);
            return current.preEnabled ? &Pre : nullptr;
        }
        PostWeaponFireHandler* GetPostWeaponFireHandler() const noexcept override
        {
            current.RecordNoThrow(Step::PostView);
            return current.postEnabled ? &Post : nullptr;
        }
    };
    struct Native final : NativeGameTimingAccess
    {
        Scenario& current;
        explicit Native(Scenario& selected) noexcept : current(selected) {}
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult ReadPedWeaponType(const NativePedReferenceToken& token, std::uint32_t slot, std::uint32_t& output) noexcept override
        {
            current.RecordNoThrow(Step::Type, current.type);
            ++current.typeCalls;
            current.receivedToken = token;
            current.receivedSlot = slot;
            output = current.type;
            return current.refuseSecondType && current.typeCalls == 2 ? ClockResult::BindingChanged : current.typeResult;
        }
    };
    struct World final : weapon_fire_bullet_fixture::WorldBase
    {
        Scenario& current;
        explicit World(Scenario& selected) noexcept : current(selected) {}
        void IgnoreEntity(CEntity* entity) override
        {
            current.Record(entity ? Step::Ignore : Step::Clear);
            current.ignored = entity;
        }
    };
    struct Stat final : weapon_fire_bullet_fixture::StatBase
    {
        Scenario& current;
        explicit Stat(Scenario& selected) noexcept : current(selected) {}
        CVector* GetFireOffset() override
        {
            current.Record(Step::Offset);
            return &current.offset;
        }
    };
    struct Manager final : weapon_fire_bullet_fixture::ManagerBase
    {
        Scenario& current;
        Stat      stat;
        explicit Manager(Scenario& selected) noexcept : current(selected), stat(selected) {}
        CWeaponStat* GetWeaponStatsFromSkillLevel(eWeaponType type, float skill) override
        {
            current.Record(Step::Stats, static_cast<std::uint32_t>(type));
            current.badArguments = current.badArguments || skill != 999.0f;
            return &stat;
        }
    };
    struct Game final : weapon_fire_bullet_fixture::GameBase
    {
        Scenario& current;
        World     world;
        Manager   manager;
        explicit Game(Scenario& selected) noexcept : current(selected), world(selected), manager(selected) {}
        CWorld* GetWorld() override
        {
            current.Record(Step::World);
            return &world;
        }
        CWeaponStatManager* GetWeaponStatManager() override
        {
            current.Record(Step::Manager);
            if (current.changeTypeInManager)
                current.type = WEAPONTYPE_CAMERA;
            return &manager;
        }
    };
    struct Player final : weapon_fire_bullet_fixture::PlayerBase
    {
        Scenario& current;
        explicit Player(Scenario& selected) noexcept : current(selected) { current.player = this; }
        CVector* GetTransformedBonePosition(eBone bone, CVector* position) override
        {
            current.Record(Step::Transform, static_cast<std::uint32_t>(bone));
            current.badArguments = current.badArguments || bone != BONE_RIGHTWRIST;
            current.aliasMuzzle =
                current.aliasMuzzle || position == &current.offset || position == current.expectedOrigin || position == current.expectedTarget;
            ++current.transforms;
            position->fX += 10.0f;
            position->fY -= 20.0f;
            position->fZ += 30.0f;
            return position;
        }
        void AddWeaponAudioEvent(EPedWeaponAudioEventType type) override
        {
            current.Record(Step::Audio, static_cast<std::uint32_t>(type));
            current.badArguments = current.badArguments || type != EPedWeaponAudioEventType::FIRE;
            ++current.audioCalls;
            if (current.enablePostInAudio)
                current.postEnabled = true;
        }
    };
    struct Weapon final : weapon_fire_bullet_fixture::WeaponBase
    {
        Scenario& current;
        Weapon(Scenario& selected, CGame& game, WeaponFireHandlerAccess& handlers, NativeGameTimingAccess& native, CPed* owner) noexcept
            : WeaponBase(game, handlers, native, selected.expectedToken, owner, WEAPONSLOT_TYPE_HANDGUN), current(selected)
        {
        }
        bool FireInstantHit(CEntity* firing, CVector* origin, CVector* muzzle, CEntity* target, CVector* destination, CVector* driveby, bool crossHairGun,
                            bool createGunFx) override
        {
            current.Record(Step::Native);
            ++current.nativeCalls;
            current.badArguments = current.badArguments || firing != current.expectedFirer || origin != current.expectedOrigin ||
                                   destination != current.expectedTarget || target || driveby || crossHairGun || !createGunFx;
            current.aliasMuzzle = current.aliasMuzzle || muzzle == &current.offset || muzzle == origin || muzzle == destination;
            current.muzzleReceived = *muzzle;
            if (current.nativeMutates)
            {
                origin->fX = 101.0f;
                destination->fY = -202.0f;
                muzzle->fZ = 303.0f;
            }
            return current.nativeReturn;
        }
    };
    bool SameToken(const NativePedReferenceToken& first, const NativePedReferenceToken& second) noexcept
    {
        return first.version == second.version && first.ownerGeneration == second.ownerGeneration && first.slot == second.slot;
    }
    void ExpectSteps(const Scenario& current, std::initializer_list<Step> expected)
    {
        Check(current.count == expected.size(), "actual SDK retains the exact service and effect count");
        std::size_t index = 0;
        for (const auto step : expected)
            Check(current.events[index++].step == step, "actual SDK preserves real service, handler, wrist, native and audio ordering");
    }
    struct Bundle
    {
        Scenario scenario;
        Handlers handlers{scenario};
        Native   native{scenario};
        Game     game{scenario};
        Player   player{scenario};
        Weapon   weapon{scenario, game, handlers, native, &player};
        explicit Bundle(const NativePedReferenceToken& token = NativePedReferenceToken{17, 29, 31}) noexcept : scenario(token) {}
        bool Fire(CEntity* firing, CVector& origin, CVector& target)
        {
            callbackScenario = &scenario;
            scenario.Expect(firing, origin, target);
            return static_cast<CWeapon&>(weapon).FireBullet(firing, origin, target);
        }
    };
}

static_assert(std::is_abstract_v<mta::game_de::CWeaponFireBulletDE>);
static_assert(std::is_base_of_v<mta::game_de::CWeaponNativeDE, mta::game_de::CWeaponFireBulletDE>);
static_assert(std::is_same_v<decltype(&mta::game_de::CWeaponFireBulletDE::FireBullet),
                             bool (mta::game_de::CWeaponFireBulletDE::*)(CEntity*, const CVector&, const CVector&)>);
static_assert(!std::is_abstract_v<Weapon> && !std::is_abstract_v<Player>);

int main()
{
    try
    {
        CVector origin(7, 8, 9), target(17, 18, 19);
        Bundle  missing;
        Check(!missing.scenario.count && missing.weapon.GetPed() == &missing.player && missing.weapon.GetSlot() == WEAPONSLOT_TYPE_HANDGUN,
              "constructor borrows actual service/owner metadata without native type, handler or game calls");
        Check(!missing.Fire(nullptr, origin, target) && !missing.scenario.count, "actual SDK null shooter refuses without reading native type or any service");
        Bundle rejected;
        rejected.scenario.type = WEAPONTYPE_UNARMED;
        Check(!rejected.Fire(&rejected.player, origin, target) && rejected.scenario.count == 1 && rejected.scenario.typeCalls == 1,
              "actual SDK rejects non-whitelist type after one inherited native getter without pretending a bullet was fired");

        Bundle complete;
        complete.scenario.changeTypeInPre = true;
        complete.scenario.changeTypeInManager = true;
        const auto copiedToken = complete.scenario.expectedToken;
        complete.scenario.expectedToken = {};
        Check(complete.Fire(&complete.player, origin, target), "actual inherited CWeapon SDK FireBullet completes despite native false");
        ExpectSteps(complete.scenario, {Step::Type, Step::World, Step::Ignore, Step::PreView, Step::Pre, Step::Manager, Step::Type, Step::Stats, Step::Offset,
                                        Step::Transform, Step::Native, Step::Audio, Step::PostView, Step::Post, Step::World, Step::Clear});
        Check(complete.scenario.events[6].first == WEAPONTYPE_CAMERA && complete.scenario.events[7].first == WEAPONTYPE_CAMERA &&
                  SameToken(complete.scenario.receivedToken, copiedToken) && complete.scenario.receivedSlot == WEAPONSLOT_TYPE_HANDGUN,
              "native getter uses copied construction token/slot; actual manager receiver precedes fresh type after the real pre handler");
        Check(complete.scenario.nativeCalls == 1 && complete.scenario.transforms == 1 && complete.scenario.audioCalls == 1 && complete.scenario.preCalls == 1 &&
                  complete.scenario.postCalls == 1 && !complete.scenario.badArguments && !complete.scenario.aliasMuzzle &&
                  complete.scenario.muzzleReceived.fX == 11.25f && complete.scenario.muzzleReceived.fY == -22.5f &&
                  complete.scenario.muzzleReceived.fZ == 33.75f && complete.scenario.offset.fX == 1.25f && complete.scenario.offset.fY == -2.5f &&
                  complete.scenario.offset.fZ == 3.75f && !complete.scenario.ignored,
              "actual RTTI selects player wrist/FIRE audio, preserves a copied muzzle and original native arguments, then clears world ignore");
        for (const bool result : {false, true})
        {
            Bundle                             nonplayer;
            weapon_fire_bullet_fixture::Entity entity;
            nonplayer.scenario.nativeReturn = result;
            Check(nonplayer.Fire(&entity, origin, target), "actual nonplayer SDK itinerary ignores either native bool result");
            ExpectSteps(nonplayer.scenario, {Step::Type, Step::World, Step::Ignore, Step::PreView, Step::Manager, Step::Type, Step::Stats, Step::Offset,
                                             Step::Native, Step::PostView, Step::World, Step::Clear});
            Check(!nonplayer.scenario.preCalls && !nonplayer.scenario.postCalls && !nonplayer.scenario.transforms && !nonplayer.scenario.audioCalls &&
                      nonplayer.scenario.nativeCalls == 1 && !nonplayer.scenario.badArguments,
                  "real dynamic_cast skips only player effects while both borrowed handler getters still run");
        }
        Bundle optional;
        optional.scenario.preEnabled = false;
        optional.scenario.postEnabled = false;
        optional.scenario.enablePostInAudio = true;
        Check(optional.Fire(&optional.player, origin, target) && !optional.scenario.preCalls && optional.scenario.postCalls == 1,
              "actual optional pre is skipped and post registration is fetched after audio enables it");
        Bundle removed;
        removed.scenario.disablePostInPre = true;
        Check(removed.Fire(&removed.player, origin, target) && removed.scenario.preCalls == 1 && !removed.scenario.postCalls,
              "the SDK observes post-handler removal during the preceding actual pre callback");

        Bundle aliased;
        aliased.scenario.nativeMutates = true;
        CVector shared(1, 2, 3);
        Check(aliased.Fire(&aliased.player, shared, shared) && shared.fX == 101.0f && shared.fY == -202.0f && !aliased.scenario.aliasMuzzle &&
                  !aliased.scenario.badArguments && aliased.scenario.offset.fZ == 3.75f,
              "actual FireInstantHit dispatch retains caller origin/target aliasing while owning the copied mutable muzzle");

        for (const bool second : {false, true})
        {
            Bundle refused;
            refused.scenario.refuseSecondType = second;
            if (!second)
                refused.scenario.typeResult = ClockResult::NotInitialized;
            bool output = false, raised = false;
            try
            {
                output = refused.Fire(&refused.player, origin, target);
            }
            catch (const std::runtime_error& error)
            {
                raised = std::string(error.what()).find("GetType") != std::string::npos;
            }
            Check(raised && !output && !refused.scenario.nativeCalls && refused.scenario.typeCalls == (second ? 2u : 1u) &&
                      refused.scenario.ignored == (second ? static_cast<CEntity*>(&refused.player) : nullptr),
                  "actual inherited native getter refusal preserves caller result and any accepted ignore/pre prefix without false success or cleanup");
        }
        const std::array<Step, 10> throwing = {Step::World,  Step::Ignore,    Step::Pre,    Step::Manager, Step::Stats,
                                               Step::Offset, Step::Transform, Step::Native, Step::Audio,   Step::Post};
        Bundle                     baseline;
        Check(baseline.Fire(&baseline.player, origin, target), "actual SDK exception-prefix baseline completed");
        for (std::size_t failure = 0; failure < baseline.scenario.count; ++failure)
        {
            bool canThrow = false;
            for (const auto step : throwing)
                canThrow = canThrow || step == baseline.scenario.events[failure].step;
            canThrow = canThrow || baseline.scenario.events[failure].step == Step::Clear;
            if (!canThrow)
                continue;
            Bundle partial;
            partial.scenario.failAt = static_cast<int>(failure);
            bool output = false, exact = false;
            try
            {
                output = partial.Fire(&partial.player, origin, target);
            }
            catch (const std::runtime_error& error)
            {
                exact = std::string(error.what()) == "sdk-prefix-" + std::to_string(failure);
            }
            Check(exact && !output && partial.scenario.count == failure + 1,
                  "each genuine SDK throwing dependency propagates its exact prefix without retry or synthetic rollback");
            for (std::size_t index = 0; index <= failure; ++index)
                Check(partial.scenario.events[index] == baseline.scenario.events[index],
                      "actual SDK accepted prefix contains no hidden extra service or cleanup calls");
            Check(partial.scenario.ignored == (failure > 2 ? static_cast<CEntity*>(&partial.player) : nullptr),
                  "world ignore retains precisely its accepted prefix even when native/audio/post/clear fails");
        }
        Bundle     another(NativePedReferenceToken{41, 43, 47});
        const auto before = complete.scenario.count;
        Check(another.Fire(&another.player, origin, target) && complete.scenario.count == before && another.scenario.preCalls == 1 &&
                  another.scenario.postCalls == 1 && SameToken(another.scenario.receivedToken, NativePedReferenceToken{41, 43, 47}),
              "separate weapon/game/handler/service owners do not redirect through a prior wrapper's globals");
        callbackScenario = nullptr;
        std::puts("Actual weapon FireBullet SDK fixture completed");
        std::printf("Passed %u actual weapon FireBullet SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        callbackScenario = nullptr;
        std::fprintf(stderr, "Weapon FireBullet SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
