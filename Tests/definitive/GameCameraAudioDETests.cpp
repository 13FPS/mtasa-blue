// Exercise the normal DE game objects with two independent sets of SDK services.
#include "../../Client/game_de/CGameCameraAudioDE.h"
#include "../../Client/game_de/CAudioEngineDE.h"
#include <game/CCamera.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace mta::game_de;

namespace
{
    void Check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    class Camera final : public CCamera
    {
    public:
        void  SetShakeForce(float force) override { shake = force; }
        float GetShakeForce() override { return shake; }
        float shake = 0;
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
    bool StaleGameRefused(Lookup lookup)
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
        Camera                             camera, otherCamera;
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
        game.GetCamera()->SetShakeForce(0.75f);

        {
            Game   second(otherCamera, otherAudio, timing, clock, owner, streaming, state);
            CGame& another = second;
            another.GetShaderReplacementStats(stats);
            Check(stats.uiTotalTextures == 0, "texture registry leaked across games");
            Check(another.GetCamera() == &otherCamera && another.GetAudioEngine() == &otherAudio, "shared provider across games");
            Check(another.GetCamera()->GetShakeForce() == 0 && otherAudio.CanPlayWorldSound(event), "shared service state");
            another.GetAudioEngine()->SetWorldSoundEnabled(2, 3, false, true);
            Check(!otherAudio.CanPlayWorldSound(event) && game.GetAudioEngine()->IsWorldSoundEnabled(2, 3), "shared mute state");
            Check(another.GetAESoundManager() == &otherManager && otherManager.cancellations == 1 && manager.cancellations == 0, "wrong cancellation owner");
            another.GetAESoundManager()->CancelSoundsInBankSlot(2, 3);
            Check(otherManager.cancellations == 2, "direct SDK cancellation reached a different sound manager");
        }
        Check(game.GetCamera()->GetShakeForce() == 0.75f, "camera identity changed");
        game.GetAudioEngine()->SetWorldSoundHandler(nullptr);
        Check(audio.CanPlayWorldSound(event), "cleared handler still active");

        // Prepared-services validation compares opaque identities only. A call
        // into a native service or a fixture playback sink aborts this host.
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
        Check(StaleGameRefused([&] { bound.GetCamera(); }) && StaleGameRefused([&] { bound.GetAudioEngine(); }) &&
                  StaleGameRefused([&] { bound.GetAESoundManager(); }),
              "expired generation accepted");
        std::puts("Game SDK camera/audio identity, dispatch, isolation, unregister and stale generation passed");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
