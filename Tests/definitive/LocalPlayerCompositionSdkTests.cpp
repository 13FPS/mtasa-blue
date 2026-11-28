/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Focused actual ped/player composition without a production factory
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <CMatrix.h>
#include <game/CGame.h>
#include <game/CPedIK.h>
#include <game/CPedSound.h>
#include <game/CProjectileInfo.h>
#include <game/CWanted.h>
#include "../../Client/game_de/CPlayerPedCompositionDE.h"
#include "../../Client/game_de/CPedBorrowedFactoryDE.h"
#include <game/CWeapon.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include "LocalPlayerCompositionFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    using Player = local_player_composition_fixture::Player;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    bool Same(const NativePedReferenceToken& a, const NativePedReferenceToken& b) noexcept
    {
        return a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    struct Owner final : NativeGameTimingAccess
    {
        NativePedReferenceToken       token{19, 77, 37}, observed;
        NativePedPoolReader::Value    value;
        std::array<unsigned char, 64> nativeBytes{};
        ClockResult                   status = ClockResult::Ok;
        float                         health = 83.25f, armor = 61.5f;
        std::uintptr_t                render;
        std::int32_t                  move = MOVE_SNEAK;
        unsigned                      calls = 0, acquires = 0, releases = 0, vitalReads = 0, vitalWrites = 0, metadataReads = 0, renderReads = 0, moveReads = 0;
        unsigned*                     destroyed = nullptr;
        CPoolsDE*                     pools = nullptr;
        bool                          releaseAfterDestruction = false;
        Owner()
        {
            value.handle = 1;
            value.slot = 0;
            value.pool = reinterpret_cast<std::uintptr_t>(this);
            value.objects = reinterpret_cast<std::uintptr_t>(nativeBytes.data());
            value.flags = value.objects + 1;
            value.nativePed = value.objects;
            value.vtable = value.objects + 8;
            value.firstMethod = value.objects + 16;
            value.capacity = static_cast<std::int32_t>(NativePedPoolProfile::NativeCapacity);
            value.cursor = -1;
            value.generation = 1;
            value.ownsAllocations = 1;
            render = value.objects + 24;
        }
        ClockResult SetTimeScale(float) noexcept override { return ClockResult::NotInitialized; }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { return ClockResult::NotInitialized; }
        ClockResult Pause(bool) noexcept override { return ClockResult::NotInitialized; }
        ClockResult CheckPedPoolAccess() noexcept override
        {
            ++calls;
            return status;
        }
        ClockResult ProbePedPoolHandle(std::uint32_t handle, bool& present, NativePedPoolReader::Value& output) noexcept override
        {
            ++calls;
            if (status != ClockResult::Ok)
                return status;
            present = handle == value.handle;
            if (present)
                output = value;
            return ClockResult::Ok;
        }
        ClockResult RecheckPedPoolHandle(const NativePedPoolReader::Value& selected) noexcept override
        {
            ++calls;
            return status != ClockResult::Ok                                                  ? status
                   : selected.nativePed == value.nativePed && selected.handle == value.handle ? ClockResult::Ok
                                                                                              : ClockResult::BindingChanged;
        }
        ClockResult ResolvePedReference(const NativePedReferenceToken& selected, NativePedPoolReader::Value& output) noexcept override
        {
            ++calls;
            observed = selected;
            if (status != ClockResult::Ok)
                return status;
            if (!Same(selected, token))
                return ClockResult::OwnerConflict;
            output = value;
            return ClockResult::Ok;
        }
        ClockResult AcquirePedReference(std::uint32_t handle, NativePedReferenceToken& output) noexcept override
        {
            ++calls;
            ++acquires;
            if (status != ClockResult::Ok)
                return status;
            if (handle != value.handle)
                return ClockResult::InvalidArgument;
            output = token;
            return ClockResult::Ok;
        }
        ClockResult ReleasePedReference(const NativePedReferenceToken& selected) noexcept override
        {
            ++calls;
            ++releases;
            observed = selected;
            releaseAfterDestruction = destroyed && *destroyed == 1 && pools && !pools->GetPedCount();
            return Same(selected, token) ? ClockResult::Ok : ClockResult::OwnerConflict;
        }
        ClockResult ReadPedVital(const NativePedReferenceToken& selected, NativePedVitalField field, float& output) noexcept override
        {
            ++calls;
            ++vitalReads;
            observed = selected;
            output = field == NativePedVitalField::Health ? health : armor;
            return status;
        }
        ClockResult SetPedVital(const NativePedReferenceToken& selected, NativePedVitalField field, float input) noexcept override
        {
            ++calls;
            ++vitalWrites;
            observed = selected;
            if (status != ClockResult::Ok)
                return status;
            if (field == NativePedVitalField::Health)
                health = input;
            else
                armor = input;
            return ClockResult::Ok;
        }
        ClockResult ReadPedMetadata(const NativePedReferenceToken& selected, NativePedMetadata& output) noexcept override
        {
            ++calls;
            ++metadataReads;
            observed = selected;
            output.modelIndex = 0x4321;
            output.entityType = static_cast<std::uint8_t>(ENTITY_TYPE_PED);
            return status;
        }
        ClockResult ReadPedRender(const NativePedReferenceToken& selected, std::uintptr_t& output) noexcept override
        {
            ++calls;
            ++renderReads;
            observed = selected;
            output = render;
            return status;
        }
        ClockResult ReadPedMoveAnim(const NativePedReferenceToken& selected, std::int32_t& output) noexcept override
        {
            ++calls;
            ++moveReads;
            observed = selected;
            output = move;
            return status;
        }
    };
    struct Projectiles final : CProjectileInfo
    {
        CEntity*    creator = nullptr;
        CEntity*    targetEntity = nullptr;
        CVector*    targetPoint = nullptr;
        CVector     origin{};
        float       force = 0;
        eWeaponType weapon = WEAPONTYPE_UNARMED;
        bool        result = false;
        unsigned    calls = 0;
        bool        AddProjectile(CEntity* source, eWeaponType type, CVector start, float strength, CVector* point, CEntity* entity) override
        {
            ++calls;
            creator = source;
            targetEntity = entity;
            targetPoint = point;
            origin = start;
            force = strength;
            weapon = type;
            return result;
        }
        CProjectile*     GetProjectile(void*) override { std::abort(); }
        CProjectileInfo* GetProjectileInfo(void*) override { std::abort(); }
        CProjectileInfo* GetProjectileInfo(DWORD) override { std::abort(); }
        void             RemoveProjectile(CProjectileInfo*, CProjectile*, bool) override { std::abort(); }
        void             RemoveEntityReferences(CEntity*) override { std::abort(); }
        CEntity*         GetTarget() override { std::abort(); }
        void             SetTarget(CEntity*) override { std::abort(); }
        bool             IsActive() override { std::abort(); }
        void             SetCounter(DWORD) override { std::abort(); }
        DWORD            GetCounter() override { std::abort(); }
    };

    struct Game final : local_player_composition_fixture::Game
    {
        CProjectileInfo* projectiles = nullptr;
        CProjectileInfo* GetProjectileInfo() override { return projectiles; }
    };

    struct SlotWeapon final : local_player_composition_fixture::WeaponBase
    {
        std::string* events = nullptr;
        char         identity = 0;
        void         Record(char action)
        {
            events->push_back(identity);
            events->push_back(action);
        }
        void SetAmmoInClip(std::uint32_t ammo) override
        {
            if (ammo != 0)
                std::abort();
            Record('C');
        }
        void SetAmmoTotal(std::uint32_t ammo) override
        {
            if (ammo != 0)
                std::abort();
            Record('T');
        }
        void Remove() override { Record('R'); }
    };

    void CheckWeaponServices(Player& implementation, CPed& ped, Game& game)
    {
        std::string                            events, expected;
        std::array<SlotWeapon, WEAPONSLOT_MAX> bank;
        for (std::size_t i = 0; i < bank.size(); ++i)
        {
            bank[i].events = &events;
            bank[i].identity = static_cast<char>('a' + i);
            implementation.slots[i] = &bank[i];
            for (const char operation : {'C', 'T', 'R'})
            {
                expected.push_back(bank[i].identity);
                expected.push_back(operation);
            }
        }

        implementation.slots.back() = nullptr;
        bool refused = false;
        try
        {
            ped.ClearWeapons();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused && events.empty(), "an incomplete weapon bank is refused before modifying an earlier slot");
        implementation.slots.back() = &bank.back();
        ped.ClearWeapons();
        Check(events == expected, "all weapon slots get zero clip, zero total, then removal in slot order");
        implementation.slots.fill(nullptr);

        CVector     origin(2, -4, 8), destination(-3, 10, 5);
        Projectiles projectiles;
        Check(!ped.AddProjectile(WEAPONTYPE_GRENADE, origin, 0.5f, &destination, &ped), "an absent projectile service declines the request");
        game.projectiles = &projectiles;
        Check(!ped.AddProjectile(WEAPONTYPE_GRENADE, origin, 0.5f, &destination, &ped), "the ped returns the projectile provider's rejection");
        Check(projectiles.creator == &ped && projectiles.targetEntity == &ped && projectiles.targetPoint == &destination && projectiles.origin == origin &&
                  projectiles.force == 0.5f && projectiles.weapon == WEAPONTYPE_GRENADE,
              "the projectile service receives the actual creator, target, type, position and force");
        projectiles.result = true;
        Check(ped.AddProjectile(WEAPONTYPE_MOLOTOV, origin, -0.25f, nullptr, nullptr) && !projectiles.targetEntity && !projectiles.targetPoint &&
                  projectiles.force == -0.25f && projectiles.weapon == WEAPONTYPE_MOLOTOV && projectiles.calls == 2,
              "acceptance, uncapped input and optional null targets pass through one SDK call");
        game.projectiles = nullptr;
    }

    struct Sound final : CPedSound
    {
        short       typeID = -1;
        short       voiceID = -1;
        std::string calls;
        short GetVoiceTypeID() override
        {
            calls += 't';
            return typeID;
        }
        short GetVoiceID() override
        {
            calls += 'v';
            return voiceID;
        }
        void SetVoiceTypeID(short value) override
        {
            calls += 'T';
            typeID = value;
        }
        void SetVoiceID(short value) override
        {
            calls += 'V';
            voiceID = value;
        }
        CPedSoundSAInterface* GetInterface() override { std::abort(); }
        bool                  IsSpeechDisabled() override { std::abort(); }
        void                  EnablePedSpeech() override { std::abort(); }
        void                  DisablePedSpeech(bool) override { std::abort(); }
    };

    void CheckNumericVoice(Player& implementation, CPed& ped)
    {
        std::int16_t type = 17, id = 23;
        const CPed&  readable = ped;
        ped.SetVoice(-2, 41);
        readable.GetVoice(&type, &id);
        Check(type == 17 && id == 23, "an absent ped sound leaves both caller outputs intact");

        Sound sound;
        implementation.sound = &sound;
        ped.SetVoice(-2, 41);
        Check(sound.calls == "TV" && sound.typeID == -2 && sound.voiceID == 41, "voice assignment preserves signed IDs and sets type first");

        sound.calls.clear();
        readable.GetVoice(&type, &id);
        Check(sound.calls == "tv" && type == -2 && id == 41, "voice getters return the selected sound provider's type and ID in order");

        sound.calls.clear();
        readable.GetVoice(&type, nullptr);
        Check(sound.calls == "t", "requesting only a type skips the voice ID getter");
        sound.calls.clear();
        readable.GetVoice(nullptr, &id);
        Check(sound.calls == "v", "requesting only a voice ID skips the type getter");
        sound.calls.clear();
        readable.GetVoice(static_cast<std::int16_t*>(nullptr), nullptr);
        Check(sound.calls.empty(), "omitting both outputs skips both sound getters");

        readable.GetVoice(&id, &id);
        Check(id == sound.voiceID && sound.calls == "tv", "aliased output receives the voice ID last");
        implementation.sound = nullptr;
    }

    struct Wanted final : CWanted
    {
        unsigned* destroyed;
        explicit Wanted(unsigned& count) noexcept : destroyed(&count) {}
        ~Wanted() { ++*destroyed; }
        void SetWantedLevel(DWORD) override { std::abort(); }
        char GetWantedLevel() override { std::abort(); }
        void SetWantedLevelNoFlash(DWORD) override { std::abort(); }
    };
    template <class Action>
    bool Refused(Action action, ClockResult result)
    {
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            return std::string(error.what()).find(": " + std::to_string(static_cast<unsigned>(result))) != std::string::npos;
        }
        return false;
    }
}

int main()
{
    try
    {
        static_assert(std::is_abstract_v<CPlayerPedCompositionDE> && !std::is_abstract_v<Player>);
        static_assert(std::is_constructible_v<Player, NativeGameTimingAccess&, const NativePedReferenceToken&, CGame&, CPoolsDE&, CWanted*, std::uint32_t>);
        static_assert(!std::is_copy_constructible_v<Player> && !std::is_move_constructible_v<Player>);
        Owner                                   owner, foreign;
        Game                                    game;
        local_player_composition_fixture::Pools pools(owner);
        unsigned                                destroyed = 0, wantedDestroyed = 0;
        Wanted                                  wanted(wantedDestroyed);
        bool                                    foreignRefused = false;
        try
        {
            Player rejected(foreign, foreign.token, game, pools, &wanted, 7);
        }
        catch (const std::invalid_argument&)
        {
            foreignRefused = true;
        }
        Check(foreignRefused && !owner.calls && !foreign.calls && !pools.GetPedCount(),
              "foreign pool-owner mismatch refuses in CPU construction before native calls or registration");
        {
            Player borrowed(owner, owner.token, game, pools, nullptr, 7);
            Check(!owner.calls && static_cast<CPlayerPed&>(borrowed).GetWanted() == nullptr,
                  "same-owner construction copies existing bindings only and preserves nullable wanted association");
        }
        Check(!owner.calls && !owner.acquires && !owner.releases, "ordinary borrowed composition destruction does not acquire or release native lifetime");
        std::puts("Actual local player composition owner admission fixture completed");
        owner.destroyed = &destroyed;
        owner.pools = &pools;
        CPedBorrowedFactoryDE<Player> factory(pools);
        Check(factory.Create(1, nullptr, game, pools, &wanted, 7u, &destroyed) == ClockResult::Ok && owner.acquires == 1 && pools.GetPedCount() == 1,
              "existing typed owner acquires once and registers the actual composed CPU wrapper");
        CPed* ped = nullptr;
        Check(factory.Read(ped) == ClockResult::Ok && ped, "existing registry publishes the complete strict fixture only after current owner checks");
        auto* player = dynamic_cast<CPlayerPed*>(ped);
        auto* implementation = dynamic_cast<Player*>(ped);
        Check(player && implementation && static_cast<CPed*>(player) == ped, "actual dynamic player conversion shares the same virtual CPed object");
        Check(static_cast<CPed*>(static_cast<CPedRebuildDE*>(implementation)) == ped && static_cast<CPed*>(static_cast<CPedPooledDE*>(implementation)) == ped &&
                  static_cast<CPed*>(static_cast<CPlayerPedMoveAnimDE*>(implementation)) == ped,
              "all three production branches converge on one SDK ped identity");
        Check(static_cast<CPedVitalsDE*>(static_cast<CPedRebuildDE*>(implementation)) == static_cast<CPedVitalsDE*>(implementation),
              "linear ped chain contains one vitals and physical-state implementation");
        const auto expected = owner.token;
        Check(Same(*implementation->FixtureTokenAddress(), expected) && implementation->FixtureTokenAddress() != &owner.token,
              "existing immutable ped reference stores the issued identity without aliasing the caller");
        Check(ped->GetOccupiedSeat() == 0xff, "a newly registered player starts unseated");
        player->SetOccupiedSeat(0);
        Check(ped->GetOccupiedSeat() == 0, "both player and ped SDK views share the driver seat");
        CheckNumericVoice(*implementation, *player);
        CheckWeaponServices(*implementation, *player, game);
        unsigned identity = 0;
        player->SetStoredPointer(&identity);
        Check(static_cast<CPed*>(static_cast<CPedPooledDE*>(implementation))->GetStoredPointer() == &identity,
              "shared inherited CPU stored pointer is identical through both SDK branches");
        std::puts("Actual local player composition single base fixture completed");
        Check(player->GetHealth() == 83.25f && owner.vitalReads == 1 && Same(owner.observed, expected),
              "actual health dispatch uses the selected same owner and copied token");
        player->SetHealth(53.75f);
        Check(ped->GetHealth() == 53.75f && owner.vitalWrites == 1 && Same(owner.observed, expected),
              "actual setter and getter share the one native owner across SDK views");
        Check(ped->GetModelIndex() == 0x4321 && ped->GetEntityType() == ENTITY_TYPE_PED && owner.metadataReads == 2,
              "actual inherited metadata methods remain genuine production dispatch");
        Check(reinterpret_cast<std::uintptr_t>(player->GetRpClump()) == owner.render && owner.renderReads == 1 && Same(owner.observed, expected),
              "GetRpClump inherits the qualified raw opaque-render getter rather than a strict historical rejector");
        owner.render = 0;
        Check(ped->GetRpClump() == nullptr && owner.renderReads == 2, "qualified raw null render remains null without a composition fallback");
        Check(reinterpret_cast<std::uintptr_t>(player->GetInterface()) == owner.value.nativePed && Same(owner.observed, expected),
              "GetInterface inherits actual controlled same-owner pool inverse provenance");
        Check(player->GetWanted() == &wanted && !wantedDestroyed, "player wanted view remains the explicitly borrowed CPU association");
        implementation->FixtureCustomMoveAnim(-12345);
        Check(player->GetMoveAnim() == MOVE_SNEAK && owner.moveReads == 1 && Same(owner.observed, expected) && ped->GetCustomMoveAnim() == -12345,
              "actual movement-group observation remains separate from the shared custom animation state");
        owner.status = ClockResult::Stopped;
        Check(Refused([&] { (void)player->GetHealth(); }, ClockResult::Stopped) && Refused([&] { (void)ped->GetRpClump(); }, ClockResult::Stopped) &&
                  Refused([&] { (void)player->GetMoveAnim(); }, ClockResult::Stopped),
              "representative ped/render/player owner refusals are not hidden by composition");
        Check(Refused([&] { (void)player->GetInterface(); }, ClockResult::Stopped), "pooled interface still refuses after owner Stop");
        owner.status = ClockResult::Ok;
        std::puts("Actual local player composition inherited dispatch fixture completed");
        Check(factory.Retire() == ClockResult::Ok && destroyed == 1 && owner.releases == 1 && owner.releaseAfterDestruction && !pools.GetPedCount(),
              "existing typed retirement withdraws CPU registration then destroys the wrapper before one native release");
        Check(factory.Retire() == ClockResult::Ok && destroyed == 1 && owner.releases == 1 && !wantedDestroyed,
              "completed typed retirement is idempotent and does not destroy the borrowed wanted view");
        Check(std::is_abstract_v<CPlayerPedCompositionDE>, "production composition remains abstract despite the strict test-only complete receiver");
        Check(!std::is_abstract_v<Player>, "exact 93 SDK and two backend strict declarations close only the fixture");
        std::puts("All 93 local player SDK methods and two backend hooks remain abstract");
        std::puts("Actual local player composition SDK fixture completed");
        std::printf("Passed %u local player composition SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
