/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the DE audio policy through its real SDK methods
 ****************************************************************************/
#include "../../Client/game_de/CAudioEngineDE.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

using mta::game_de::CAudioEngineDE;
static_assert(std::is_abstract_v<CAudioEngineDE>);

namespace
{
    struct SoundManagerFixture final : CAESoundManager
    {
        std::vector<std::pair<uint, uint>> cancellations;
        void                               CancelSoundsInBankSlot(uint group, uint index) override { cancellations.emplace_back(group, index); }
    };

    class AudioFixture final : public CAudioEngineDE
    {
    public:
        explicit AudioFixture(CAESoundManager& manager) : CAudioEngineDE(manager) {}
        std::vector<std::pair<bool, bool>> ambientUpdates;

        void UpdateAmbientSoundSettings() override
        {
            ambientUpdates.emplace_back(IsAmbientSoundEnabled(AMBIENT_SOUND_GENERAL), IsAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE));
        }

        // A policy test must fail if it reaches any playback operation.
        void          PlayFrontEndSound(DWORD) override { std::abort(); }
        void          PlayBeatTrack(short) override { std::abort(); }
        void          SetEffectsMasterVolume(BYTE) override { std::abort(); }
        void          SetMusicMasterVolume(BYTE) override { std::abort(); }
        void          ClearMissionAudio(int) override { std::abort(); }
        void          PreloadMissionAudio(unsigned short, int) override { std::abort(); }
        unsigned char GetMissionAudioLoadingStatus(int) override { std::abort(); }
        bool          IsMissionAudioSampleFinished(int) override { std::abort(); }
        void          AttachMissionAudioToPhysical(CPhysical*, int) override { std::abort(); }
        void          SetMissionAudioPosition(CVector*, int) override { std::abort(); }
        bool          PlayLoadedMissionAudio(int) override { std::abort(); }
        void          PauseAllSound(bool) override { std::abort(); }
        void          StopRadio() override { std::abort(); }
        void          StartRadio(unsigned int) override { std::abort(); }
        void          ReportBulletHit(CEntity*, unsigned char, CVector*, float) override { std::abort(); }
        void          ReportWeaponEvent(int, eWeaponType, CPhysical*) override { std::abort(); }
    };

    void Check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    unsigned         deliveries = 0;
    SWorldSoundEvent received{};
    bool             Capture(const SWorldSoundEvent& event)
    {
        ++deliveries;
        received = event;
        return false;
    }

    void TestWorldSounds()
    {
        SoundManagerFixture    manager;
        AudioFixture           fixture(manager);
        CAudioEngine&          sdk = fixture;
        constexpr uint         all = (std::numeric_limits<uint>::max)();
        int                    identity = 0;
        const SWorldSoundEvent event{2, 256, reinterpret_cast<CEntitySAInterface*>(&identity), CVector(-3.5f, 2.0f, 0.125f)};
        Check(&fixture.GetSoundManager() == &manager && fixture.CanPlayWorldSound(event), "manager identity and initial handler state");
        sdk.SetWorldSoundHandler(Capture);
        Check(!fixture.CanPlayWorldSound(event) && deliveries == 1, "handler veto");
        Check(received.uiGroup == event.uiGroup && received.uiIndex == event.uiIndex && received.pGameEntity == event.pGameEntity &&
                  received.vecPosition.fX == -3.5f && received.vecPosition.fY == 2.0f && received.vecPosition.fZ == 0.125f,
              "event payload");
        sdk.SetWorldSoundEnabled(2, 256, false, false);
        Check(!fixture.CanPlayWorldSound(event) && deliveries == 1 && manager.cancellations.empty(), "muting precedes dispatch");
        Check(!sdk.IsWorldSoundEnabled(2, all) && sdk.IsWorldSoundEnabled(3, 0), "per-bank wildcard query and high sample index isolation");
        sdk.SetWorldSoundEnabled(2, 256, false, true);
        sdk.SetWorldSoundEnabled(2, all, false, true);
        sdk.SetWorldSoundEnabled(2, 256, true, true);
        Check(manager.cancellations == std::vector<std::pair<uint, uint>>{{2, 256}, {2, all}}, "immediate single and wildcard cancellation");
        Check(sdk.IsWorldSoundEnabled(2, 256) && !sdk.IsWorldSoundEnabled(2, 0) && !sdk.IsWorldSoundEnabled(2, all), "single exception to wildcard");
        sdk.SetWorldSoundEnabled(2, all, true, true);
        Check(sdk.IsWorldSoundEnabled(2, all), "enable whole bank clears all exceptions");
        sdk.SetWorldSoundEnabled(2, 256, false, false);
        sdk.ResetWorldSounds();
        Check(!fixture.CanPlayWorldSound(event) && deliveries == 2, "reset unmutes and preserves handler");
        sdk.SetWorldSoundHandler(
            [](const SWorldSoundEvent& sound)
            {
                ++deliveries;
                return sound.pGameEntity == nullptr;
            });
        Check(fixture.CanPlayWorldSound({2, 399, nullptr, CVector{}}) && deliveries == 3, "replacement permits entityless event");
        Check(!fixture.CanPlayWorldSound({2, all, nullptr, CVector{}}) && deliveries == 3, "wildcard cannot be a sample");
        sdk.SetWorldSoundHandler(nullptr);
        Check(fixture.CanPlayWorldSound(event) && deliveries == 3, "handler teardown");
        sdk.SetWorldSoundEnabled(2, all, false, false);
        SoundManagerFixture anotherManager;
        AudioFixture        another(anotherManager);
        Check(&another.GetSoundManager() == &anotherManager && another.CanPlayWorldSound(event), "independent manager and settings");
    }

    void TestAmbientSounds()
    {
        SoundManagerFixture manager;
        AudioFixture        fixture(manager);
        CAudioEngine&       sdk = fixture;
        Check(sdk.IsAmbientSoundEnabled(AMBIENT_SOUND_GENERAL) && sdk.IsAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE), "initial ambient groups");
        sdk.SetAmbientSoundEnabled(AMBIENT_SOUND_GENERAL, false);
        sdk.PauseAmbientSounds(true);
        sdk.SetAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE, false);
        sdk.ResetAmbientSounds();
        Check(!sdk.IsAmbientSoundEnabled(AMBIENT_SOUND_GENERAL) && !sdk.IsAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE), "reset preserves global pause");
        sdk.SetAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE, false);
        sdk.PauseAmbientSounds(false);
        Check(sdk.IsAmbientSoundEnabled(AMBIENT_SOUND_GENERAL) && !sdk.IsAmbientSoundEnabled(AMBIENT_SOUND_GUNFIRE),
              "resume uses settings edited while paused");
        Check(fixture.ambientUpdates ==
                  std::vector<std::pair<bool, bool>>{
                      {false, true}, {false, false}, {false, false}, {false, false}, {false, false}, {false, false}, {true, false}},
              "effective settings delivered in order");
    }
}

int main()
{
    try
    {
        TestWorldSounds();
        TestAmbientSounds();
        std::puts("DE world sound dispatch, mute/cancel and ambient SDK cases passed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
