/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Independent checks for actual MTA FireBullet callback orchestration
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <game/CWeaponFireBulletPolicy.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
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
        Ignore,
        PlayerQuery,
        PreGate,
        PreHandler,
        Manager,
        Stats,
        Offset,
        Transform,
        Fire,
        Audio,
        PostGate,
        PostHandler,
        Clear
    };
    struct Event
    {
        Step          step;
        std::uint32_t first = 0, second = 0;
        bool          operator==(const Event& other) const noexcept { return step == other.step && first == other.first && second == other.second; }
    };
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
    bool SameVector(const CVector& first, const CVector& second) noexcept
    {
        return std::memcmp(&first, &second, sizeof(CVector)) == 0;
    }
    struct Entity
    {
        virtual ~Entity() = default;
    };
    struct Player final : Entity
    {
    };
    struct Scenario;
    struct Stat
    {
        Scenario& scenario;
        CVector   offset{1.25f, -2.5f, 3.75f};
        CVector*  GetFireOffset();
    };
    struct Manager
    {
        Scenario& scenario;
        Stat*     GetWeaponStatsFromSkillLevel(eWeaponType type, float skill);
    };
    struct Weapon
    {
        Scenario&   scenario;
        eWeaponType GetType() const;
        bool        FireInstantHit(Entity* firing, CVector* origin, CVector* muzzle, Entity* target, CVector* destination, CVector* driveby, bool crossHairGun,
                                   bool createGunFx);
    };
    struct Scenario
    {
        std::vector<Event> events;
        int                failAt = -1;
        eWeaponType        currentType = WEAPONTYPE_PISTOL;
        eWeaponType        preType = WEAPONTYPE_PISTOL;
        eWeaponType        managerType = WEAPONTYPE_MINIGUN;
        bool               changeTypeInPre = false;
        bool               changeTypeInManager = false;
        bool               preEnabled = true, postEnabled = true, nativeResult = false;
        bool               disablePostInPre = false, enablePostInAudio = false;
        bool               mutateNativeVectors = false;
        unsigned           types = 0, nativeCalls = 0, audioCalls = 0, preCalls = 0, postCalls = 0, transforms = 0;
        Entity*            ignored = nullptr;
        Entity*            expectedFirer = nullptr;
        CVector*           expectedOrigin = nullptr;
        CVector*           expectedTarget = nullptr;
        CVector            muzzleReceived;
        bool               wrongArguments = false, aliasedMuzzle = false;
        Stat               stat{*this};
        Manager            manager{*this};
        Weapon             weapon{*this};
        void               Record(Step step, std::uint32_t first = 0, std::uint32_t second = 0)
        {
            events.push_back({step, first, second});
            if (failAt >= 0 && events.size() == static_cast<std::size_t>(failAt + 1))
                throw std::runtime_error("fire-prefix-" + std::to_string(failAt));
        }
        bool Fire(Entity* firing, const CVector& origin, const CVector& target)
        {
            expectedFirer = firing;
            expectedOrigin = const_cast<CVector*>(&origin);
            expectedTarget = const_cast<CVector*>(&target);
            return MtaGame::CWeaponFireBulletPolicy::Fire(
                weapon, firing, origin, target,
                [this](Entity* selected)
                {
                    Record(selected ? Step::Ignore : Step::Clear);
                    ignored = selected;
                },
                [this](Entity* selected) -> Player*
                {
                    Record(Step::PlayerQuery);
                    wrongArguments = wrongArguments || selected != expectedFirer;
                    return dynamic_cast<Player*>(selected);
                },
                [this](Player* selected)
                {
                    Record(Step::PreGate, selected ? 1u : 0u);
                    if (preEnabled && selected)
                    {
                        Record(Step::PreHandler);
                        ++preCalls;
                        if (changeTypeInPre)
                            currentType = preType;
                        if (disablePostInPre)
                            postEnabled = false;
                    }
                },
                [this]() -> Manager*
                {
                    Record(Step::Manager);
                    if (changeTypeInManager)
                        currentType = managerType;
                    return &manager;
                },
                [this](Player* selected, CVector* muzzle)
                {
                    Record(Step::Transform);
                    wrongArguments = wrongArguments || selected != dynamic_cast<Player*>(expectedFirer);
                    aliasedMuzzle = aliasedMuzzle || muzzle == &stat.offset || muzzle == expectedOrigin || muzzle == expectedTarget;
                    ++transforms;
                    muzzle->fX += 10.0f;
                    muzzle->fY -= 20.0f;
                    muzzle->fZ += 30.0f;
                },
                [this](Player* selected)
                {
                    Record(Step::Audio);
                    wrongArguments = wrongArguments || selected != dynamic_cast<Player*>(expectedFirer);
                    ++audioCalls;
                    if (enablePostInAudio)
                        postEnabled = true;
                },
                [this](Player* selected)
                {
                    Record(Step::PostGate, selected ? 1u : 0u);
                    if (postEnabled && selected)
                    {
                        Record(Step::PostHandler);
                        ++postCalls;
                    }
                });
        }
    };
    CVector* Stat::GetFireOffset()
    {
        scenario.Record(Step::Offset);
        return &offset;
    }
    Stat* Manager::GetWeaponStatsFromSkillLevel(eWeaponType type, float skill)
    {
        scenario.Record(Step::Stats, static_cast<std::uint32_t>(type), Bits(skill));
        return &scenario.stat;
    }
    eWeaponType Weapon::GetType() const
    {
        scenario.Record(Step::Type, static_cast<std::uint32_t>(scenario.currentType));
        ++scenario.types;
        return scenario.currentType;
    }
    bool Weapon::FireInstantHit(Entity* firing, CVector* origin, CVector* muzzle, Entity* target, CVector* destination, CVector* driveby, bool crossHairGun,
                                bool createGunFx)
    {
        scenario.Record(Step::Fire);
        ++scenario.nativeCalls;
        scenario.wrongArguments = scenario.wrongArguments || firing != scenario.expectedFirer || origin != scenario.expectedOrigin ||
                                  destination != scenario.expectedTarget || target != nullptr || driveby != nullptr || crossHairGun || !createGunFx;
        scenario.aliasedMuzzle = scenario.aliasedMuzzle || muzzle == &scenario.stat.offset || muzzle == origin || muzzle == destination;
        scenario.muzzleReceived = *muzzle;
        if (scenario.mutateNativeVectors)
        {
            origin->fX = 101.0f;
            destination->fY = -202.0f;
            muzzle->fZ = 303.0f;
        }
        return scenario.nativeResult;
    }
    void ExpectSteps(const Scenario& scenario, std::initializer_list<Step> expected)
    {
        Check(scenario.events.size() == expected.size(), "actual FireBullet keeps the exact callback count");
        std::size_t index = 0;
        for (const auto step : expected)
            Check(scenario.events[index++].step == step, "actual FireBullet retains MTA callback ordering");
    }
}

int main()
{
    try
    {
        Entity   entity;
        Player   player;
        CVector  origin(7.0f, 8.0f, 9.0f), target(17.0f, 18.0f, 19.0f);
        Scenario missing;
        Check(!missing.Fire(nullptr, origin, target) && missing.events.empty(),
              "null shooter refuses before GetType, ignore state, RTTI, stats or native callbacks");

        const std::array<eWeaponType, 14> allowed = {
            WEAPONTYPE_PISTOL,         WEAPONTYPE_PISTOL_SILENCED, WEAPONTYPE_DESERT_EAGLE, WEAPONTYPE_SHOTGUN, WEAPONTYPE_SAWNOFF_SHOTGUN,
            WEAPONTYPE_SPAS12_SHOTGUN, WEAPONTYPE_MICRO_UZI,       WEAPONTYPE_MP5,          WEAPONTYPE_AK47,    WEAPONTYPE_M4,
            WEAPONTYPE_TEC9,           WEAPONTYPE_COUNTRYRIFLE,    WEAPONTYPE_SNIPERRIFLE,  WEAPONTYPE_MINIGUN};
        for (const auto type : allowed)
        {
            Scenario current;
            current.currentType = type;
            const auto offsetBefore = current.stat.offset;
            Check(current.Fire(&entity, origin, target), "every genuine classic whitelist type returns true after orchestration");
            ExpectSteps(current, {Step::Type, Step::Ignore, Step::PlayerQuery, Step::PreGate, Step::Manager, Step::Type, Step::Stats, Step::Offset, Step::Fire,
                                  Step::PostGate, Step::Clear});
            Check(current.types == 2 && current.nativeCalls == 1 && !current.preCalls && !current.transforms && !current.audioCalls && !current.postCalls &&
                      current.ignored == nullptr && !current.wrongArguments && !current.aliasedMuzzle,
                  "nonplayer keeps both gate evaluations but skips only player effects; native false does not become FireBullet false");
            Check(SameVector(current.muzzleReceived, offsetBefore) && SameVector(current.stat.offset, offsetBefore) &&
                      current.events[6].first == static_cast<std::uint32_t>(type) && current.events[6].second == Bits(999.0f),
                  "fresh skill999.0 stats offset is copied into a distinct stack muzzle without normalization or fallback");
        }
        for (const std::uint32_t raw : {0u, 1u, 16u, 21u, 35u, 36u, 37u, 39u, 40u, 46u, 47u, 100u, 254u, 255u})
        {
            Scenario rejected;
            rejected.currentType = static_cast<eWeaponType>(raw);
            Check(!rejected.Fire(&player, origin, target) && rejected.types == 1 && !rejected.nativeCalls && rejected.events.size() == 1 &&
                      rejected.events[0].step == Step::Type,
                  "non-whitelist type bits return false before ignore state or any player/native preparation");
        }

        Scenario complete;
        complete.changeTypeInPre = true;
        complete.preType = WEAPONTYPE_ROCKETLAUNCHER;
        const auto originalOffset = complete.stat.offset;
        Check(complete.Fire(&player, origin, target), "supported player FireBullet completes even when FireInstantHit returns false");
        ExpectSteps(complete, {Step::Type, Step::Ignore, Step::PlayerQuery, Step::PreGate, Step::PreHandler, Step::Manager, Step::Type, Step::Stats,
                               Step::Offset, Step::Transform, Step::Fire, Step::Audio, Step::PostGate, Step::PostHandler, Step::Clear});
        Check(complete.events[0].first == WEAPONTYPE_PISTOL && complete.events[6].first == WEAPONTYPE_ROCKETLAUNCHER &&
                  complete.events[7].first == WEAPONTYPE_ROCKETLAUNCHER && complete.events[7].second == Bits(999.0f),
              "the second fresh GetType follows the pre handler and supplies stats without a second whitelist gate or cached type");
        Check(complete.preCalls == 1 && complete.transforms == 1 && complete.nativeCalls == 1 && complete.audioCalls == 1 && complete.postCalls == 1 &&
                  !complete.ignored && !complete.wrongArguments && !complete.aliasedMuzzle && SameVector(complete.stat.offset, originalOffset) &&
                  complete.muzzleReceived.fX == 11.25f && complete.muzzleReceived.fY == -22.5f && complete.muzzleReceived.fZ == 33.75f,
              "player transform changes only the stack muzzle; native call precedes FIRE audio, post effect and ignore clear");
        for (const bool nativeResult : {false, true})
        {
            Scenario value;
            value.nativeResult = nativeResult;
            value.preEnabled = false;
            value.postEnabled = false;
            Check(value.Fire(&player, origin, target) && !value.preCalls && !value.postCalls && value.transforms == 1 && value.nativeCalls == 1 &&
                      value.audioCalls == 1 && !value.ignored,
                  "optional handlers alone are skipped; direct FireInstantHit boolean never controls the accepted policy result");
        }

        Scenario disabled;
        disabled.disablePostInPre = true;
        Check(disabled.Fire(&player, origin, target) && disabled.preCalls == 1 && disabled.audioCalls == 1 && !disabled.postCalls,
              "post-handler availability is read when reached rather than captured before the pre callback");
        Scenario enabled;
        enabled.postEnabled = false;
        enabled.enablePostInAudio = true;
        Check(enabled.Fire(&player, origin, target) && enabled.postCalls == 1,
              "a post handler enabled by the preceding audio callback is observed on the same invocation");
        Scenario receiver;
        receiver.changeTypeInPre = true;
        receiver.preType = WEAPONTYPE_ROCKETLAUNCHER;
        receiver.changeTypeInManager = true;
        receiver.managerType = WEAPONTYPE_CAMERA;
        Check(receiver.Fire(&player, origin, target) && receiver.events[5].step == Step::Manager && receiver.events[6].first == WEAPONTYPE_CAMERA &&
                  receiver.events[7].first == WEAPONTYPE_CAMERA,
              "actual stats-manager receiver lookup precedes the second GetType argument and its fresh mutation is observed");

        Scenario mutations;
        mutations.mutateNativeVectors = true;
        CVector    mutableOrigin(-0.0f, 2.0f, 3.0f), mutableTarget(4.0f, 5.0f, 6.0f);
        const auto storedOffset = mutations.stat.offset;
        Check(mutations.Fire(&player, mutableOrigin, mutableTarget) && mutableOrigin.fX == 101.0f && mutableTarget.fY == -202.0f && !mutations.wrongArguments &&
                  !mutations.aliasedMuzzle && SameVector(mutations.stat.offset, storedOffset),
              "FireInstantHit receives original const-cast origin/target addresses, while its mutable muzzle remains an owned stack copy");
        Scenario alias;
        alias.mutateNativeVectors = true;
        CVector one(1.0f, 2.0f, 3.0f);
        Check(alias.Fire(&entity, one, one) && one.fX == 101.0f && one.fY == -202.0f && !alias.wrongArguments,
              "caller origin/target aliasing remains intact rather than being silently split into sanitized buffers");

        Scenario baseline;
        baseline.changeTypeInPre = true;
        baseline.preType = WEAPONTYPE_ROCKETLAUNCHER;
        Check(baseline.Fire(&player, origin, target), "failure-prefix baseline completed");
        for (std::size_t failure = 0; failure < baseline.events.size(); ++failure)
        {
            Scenario partial;
            partial.changeTypeInPre = true;
            partial.preType = WEAPONTYPE_ROCKETLAUNCHER;
            partial.failAt = static_cast<int>(failure);
            bool output = false, exact = false;
            try
            {
                output = partial.Fire(&player, origin, target);
            }
            catch (const std::runtime_error& error)
            {
                exact = std::string(error.what()) == "fire-prefix-" + std::to_string(failure);
            }
            Check(exact && !output && partial.events.size() == failure + 1,
                  "every callback exception keeps caller output and propagates the original accepted prefix without retry");
            for (std::size_t index = 0; index <= failure; ++index)
                Check(partial.events[index] == baseline.events[index], "exception neither rewinds nor adds synthetic ignore cleanup");
            const bool ignoreWasAccepted = failure > 1;
            Check(partial.ignored == (ignoreWasAccepted ? &player : nullptr),
                  "ignored entity remains at its actual accepted prefix; orchestration invents no exception compensation");
        }
        std::puts("Production MTA weapon FireBullet policy fixture completed");
        std::printf("Passed %u actual MTA weapon FireBullet policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Weapon FireBullet policy failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
