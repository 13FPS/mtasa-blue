// Exercise the normal DE game objects with two independent sets of SDK services.
#include "../../Client/game_de/CGameCameraAudioDE.h"
#include "../../Client/game_de/CAudioEngineDE.h"
#include "../../Client/game_de/CCameraTransitionDE.h"
#include "../../Client/game_de/CPoolsDE.h"
#include <CMatrix.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace mta::game_de;

extern "C" __declspec(dllimport) const CCameraTransitionFOV* FindFovService(const CCamera* camera);

namespace
{
    void Check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    class Pools final : public CPoolsDE
    {
    public:
        explicit Pools(NativeGameTimingAccess& owner) : CPoolsDE(owner) {}
#include "Pools.generated.inl"
    };

    struct Transition final : CameraTransitionSource
    {
        ClockResult ReadCameraTransition(CameraTransitionSample& out) const noexcept override
        {
            ++reads;
            out = sample;
            return status;
        }
        CameraTransitionSample sample{};
        ClockResult            status = ClockResult::Ok;
        mutable unsigned       reads = 0;
    };

    class Camera final : public CCameraTransitionDE
    {
    public:
        Camera(Pools& pools, NativeGameTimingAccess& owner, const Transition& source) : CCameraTransitionDE(pools, owner, source) {}
#include "Camera.generated.inl"
    };

    class Audio final : public CAudioEngineDE
    {
    public:
        explicit Audio(CAESoundManager& manager) : CAudioEngineDE(manager) {}
#include "Audio.generated.inl"
    };

    struct SoundManager final : CAESoundManager
    {
        void CancelSoundsInBankSlot(uint group, uint index) override
        {
            Check(group == 2 && index == 3, "wrong bank or sample cancelled");
            ++cancellations;
        }
        unsigned cancellations = 0;
    };

    class Access final : public NativeGameTimingAccess, public NativeClockAccess
    {
    public:
        ClockResult Read(ClockValues&) noexcept override { std::abort(); }
        ClockResult Set(std::uint8_t, std::uint8_t) noexcept override { std::abort(); }
        ClockResult Freeze(bool) noexcept override { std::abort(); }
        ClockResult Frozen(bool&) const noexcept override { std::abort(); }
        ClockResult SetTimeScale(float) noexcept override { std::abort(); }
        ClockResult SetMinuteDuration(std::uint32_t) noexcept override { std::abort(); }
        ClockResult Pause(bool) noexcept override { std::abort(); }

        ClockResult CaptureConstructorServices(const CClock*, NativeGameConstructorServices& out) noexcept override
        {
            out = current;
            return ClockResult::Ok;
        }
        ClockResult ValidateConstructorServices(const NativeGameConstructorServices& supplied) noexcept override
        {
            return supplied.Same(current) ? ClockResult::Ok : ClockResult::OwnerConflict;
        }

        NativeGameConstructorServices current;
    };

    struct Game final : CGameCameraAudioDE
    {
        template <typename... Args>
        Game(Args&&... args) : CGameCameraAudioDE(std::forward<Args>(args)...)
        {
        }
#include "Game.generated.inl"
    };

    bool Veto(const SWorldSoundEvent&)
    {
        return false;
    }

    template <class Lookup>
    bool Refused(Lookup lookup)
    {
        try
        {
            lookup();
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        return false;
    }
}

int main()
{
    try
    {
        Access                             owner;
        CClockDE                           clock(owner);
        const NativeGameTimingReader       timing;
        const NativeStreamingFileIdsReader streaming;
        const NativeGameStateReader        state;
        Pools                              pools(owner);
        Transition                         transition;
        Camera                             camera(pools, owner, transition);
        SoundManager                       manager, otherManager;
        Audio                              audio(manager), otherAudio(otherManager);
        Game                               first(camera, audio, timing, clock, owner, streaming, state);
        CGame&                             game = first;

        Check(game.GetCamera() == &camera && game.GetAudioEngine() == &audio && game.GetAESoundManager() == &manager, "wrong initial provider");
        Check(game.GetClock() == &clock && game.GetGameVersion() == VERSION_DE, "wrong underlying game");
        Check(game.GetAudioContainer() != nullptr, "file-audio service missing from game");
        // The inherited texture registry uses shared SDK strings when a game
        // is instantiated. Exercise its normal initialization and stats paths.
        SShaderReplacementStats stats{};
        game.GetShaderReplacementStats(stats);
        Check(stats.uiTotalTextures == 0, "new game already had registered textures");
        first.GetRenderPreparation().Initialize();
        game.GetShaderReplacementStats(stats);
        Check(stats.uiTotalTextures == 1, "session texture registry did not initialize");
        const SWorldSoundEvent event{2, 3, nullptr, {}};
        game.GetAudioEngine()->SetWorldSoundHandler(&Veto);
        Check(!audio.CanPlayWorldSound(event), "game did not install sound callback");
        // Test the same rolled transition through CGame that a client would
        // observe. Only the sample input comes from the test.
        transition.sample = {1, 47.5f, CVector(8, 2, 3), CVector(8, 6, 3), CVector(1, 0, 0)};
        CMatrix pose;
        Check(game.GetCamera()->GetTransitionMatrix(pose) && transition.reads == 1 && pose.vPos == CVector(8, 2, 3) && pose.vFront == CVector(0, 1, 0) &&
                  pose.vUp == CVector(1, 0, 0) && pose.vRight == CVector(0, 0, -1),
              "wrong DE transition pose through CGame");

        {
            Access     otherOwner;
            CClockDE   otherClock(otherOwner);
            Pools      otherPools(otherOwner);
            Transition otherTransition;
            Camera     otherCamera(otherPools, otherOwner, otherTransition);
            Game       second(otherCamera, otherAudio, timing, otherClock, otherOwner, streaming, state);
            CGame&     another = second;
            another.GetShaderReplacementStats(stats);
            Check(stats.uiTotalTextures == 0, "texture registry leaked across games");
            Check(another.GetCamera() == &otherCamera && another.GetAudioEngine() == &otherAudio, "shared provider across games");
            Check(!another.GetCamera()->IsInTransition() && otherTransition.reads == 1 && transition.reads == 1 && otherAudio.CanPlayWorldSound(event),
                  "shared service state");
            another.GetAudioEngine()->SetWorldSoundEnabled(2, 3, false, true);
            Check(!otherAudio.CanPlayWorldSound(event) && game.GetAudioEngine()->IsWorldSoundEnabled(2, 3), "shared mute state");
            Check(another.GetAESoundManager() == &otherManager && otherManager.cancellations == 1 && manager.cancellations == 0, "wrong cancellation owner");
            another.GetAESoundManager()->CancelSoundsInBankSlot(2, 3);
            Check(otherManager.cancellations == 2, "direct SDK cancellation reached a different sound manager");
        }
        const auto* sampled = FindFovService(game.GetCamera());
        float       fov = 0;
        Check(sampled && sampled->TryGetActiveTransitionFOV(fov) && fov == 47.5f && transition.reads == 2,
              "client DLL did not find the first game's transition FOV");
        transition.status = ClockResult::Stopped;
        const auto before = pose.To4x4Array();
        Check(Refused([&] { game.GetCamera()->GetTransitionMatrix(pose); }) && pose.To4x4Array() == before && transition.reads == 3,
              "refused transition published a pose");
        game.GetAudioEngine()->SetWorldSoundHandler(nullptr);
        Check(audio.CanPlayWorldSound(event), "cleared handler still active");

        // Prepared-services validation compares opaque identities only.
        auto& services = owner.current;
        services.generation = 1;
        services.clock = &clock;
        void* identity = &owner;
        services.stats = static_cast<CStats*>(identity);
        services.playerInfo = static_cast<CPlayerInfo*>(identity);
        services.wanted = static_cast<CWanted*>(identity);
        services.ropes = static_cast<CRopes*>(identity);
        services.weather = static_cast<CWeather*>(identity);
        services.weaponManager = static_cast<CWeaponStatManager*>(identity);
        services.weapons.fill(static_cast<CWeaponInfo*>(identity));

        Game   prepared(camera, audio, timing, clock, owner, streaming, state, services);
        CGame& bound = prepared;
        Check(bound.GetCamera() == &camera && bound.GetAudioEngine() == &audio && bound.GetAESoundManager() == &manager, "prepared lookup lost provider");
        ++services.generation;
        Check(Refused([&] { bound.GetCamera(); }) && Refused([&] { bound.GetAudioEngine(); }) && Refused([&] { bound.GetAESoundManager(); }),
              "expired generation accepted");
        std::puts("CGame DE camera transition pose, source refusal, audio dispatch, game isolation, unregister and stale generation passed");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
