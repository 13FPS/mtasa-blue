// Exercise the sound manager's two public archive playback methods with an
// owned game SDK and an observable sound output. The runner imports the exact
// production method definitions; GTA and an output device are unnecessary.
#pragma once
#include <algorithm>
#include <array>
#include <memory>
#ifdef MTA_TEST_BASS_DECODE
    #include <bass.h>
#endif

namespace sfx_playback_fixture
{
    struct Game
    {
        CGame&        sdk;
        unsigned char volume = 0;
        unsigned      volumeReads = 0;

        CAudioContainer* GetAudioContainer() { return sdk.GetAudioContainer(); }
        Game*            GetSettings() { return this; }
        unsigned char    GetSFXVolume()
        {
            ++volumeReads;
            return volume;
        }
    };
    inline Game* g_pGame = nullptr;

    struct CClientSound
    {
        float volume = -1;
        void  SetVolume(float value) { volume = value; }
    };

    struct CClientSoundManager
    {
        std::array<bool, AUDIO_LOOKUP_RADIO> m_aValidatedSFX{};
        CClientSound                         sound;
        std::unique_ptr<unsigned char[]>     buffer;
        unsigned                             length = 0;
        bool                                 failOutput = false, looped = false;
        CVector                              position;

        CClientSound* PlaySound2D(void* data, unsigned size, bool loop)
        {
            buffer.reset(static_cast<unsigned char*>(data));
            length = size;
            looped = loop;
            sound.volume = -1;
            return failOutput ? nullptr : &sound;
        }
        CClientSound* PlaySound3D(void* data, unsigned size, const CVector& point, bool loop)
        {
            position = point;
            return PlaySound2D(data, size, loop);
        }

        CClientSound* PlayGTASFX(eAudioLookupIndex, int, int, bool);
        CClientSound* PlayGTASFX3D(eAudioLookupIndex, int, int, const CVector&, bool);
        bool          GetSFXStatus(eAudioLookupIndex);
    };

#include "GTASfxPlayback.generated.inl"

#ifdef MTA_TEST_BASS_DECODE
    inline void Decode(const CClientSoundManager& output, DWORD expectedType)
    {
        const HSTREAM channel = BASS_StreamCreateFile(BASS_FILE_MEM, output.buffer.get(), 0, output.length, BASS_STREAM_DECODE);
        Check(channel != 0, "BASS opened the in-memory archive sample");
        BASS_CHANNELINFO info{};
        Check(BASS_ChannelGetInfo(channel, &info) && info.ctype == expectedType && info.freq && info.chans, "decoded stream type, channels and frequency");
        std::array<short, 8192> samples{};
        unsigned                decoded = 0;
        bool                    signal = false;
        while (!signal && decoded < 1024 * 1024)
        {
            const DWORD count = BASS_ChannelGetData(channel, samples.data(), sizeof(samples));
            if (!count || count == static_cast<DWORD>(-1))
                break;
            Check(count <= sizeof(samples) && count % (info.chans * sizeof(short)) == 0, "whole PCM frames");
            decoded += count;
            signal = std::any_of(samples.begin(), samples.begin() + count / sizeof(short), [](short value) { return value != 0; });
        }
        Check(BASS_StreamFree(channel) && signal, "decoded non-silent PCM");
        std::printf("%s: %u Hz, %u channel(s), %u decoded frames with signal\n", expectedType == BASS_CTYPE_STREAM_OGG ? "Ogg" : "WAVE", info.freq, info.chans,
                    decoded / static_cast<unsigned>(info.chans * sizeof(short)));
    }
#endif

    inline void Run(CGame& game, bool installed)
    {
        Game settings{game};
        g_pGame = &settings;
        CClientSoundManager manager;
        manager.m_aValidatedSFX[AUDIO_LOOKUP_FEET] = game.GetAudioContainer()->ValidateContainer(AUDIO_LOOKUP_FEET);
        const CVector position(3, -2, 1);
        struct Gain
        {
            unsigned char raw;
            float         expected;
        };
        for (auto [raw, expected] : std::array<Gain, 4>{{{0, 0}, {32, 0.5f}, {64, 1}, {255, 1}}})
        {
            settings.volume = raw;
            auto* sound = manager.PlayGTASFX(AUDIO_LOOKUP_FEET, 0, 0, true);
            Check(sound && sound->volume == expected && manager.looped, "2D archive sound, slider volume and loop flag");
            sound = manager.PlayGTASFX3D(AUDIO_LOOKUP_FEET, 0, 0, position, false);
            Check(sound && sound->volume == expected && !manager.looped && manager.position == position, "3D archive sound, slider volume, loop and position");
        }
#ifdef MTA_TEST_BASS_DECODE
        if (installed)
            Decode(manager, BASS_CTYPE_STREAM_WAV_PCM);
#else
        (void)installed;
#endif
        Check(manager.PlayGTASFX3D(AUDIO_LOOKUP_RADIO, RADIO_STREAM_POLICE, 1, position, false) != nullptr, "radio bypasses SFX bank validation");
#ifdef MTA_TEST_BASS_DECODE
        if (installed)
            Decode(manager, BASS_CTYPE_STREAM_OGG);
#endif
        const unsigned reads = settings.volumeReads;
        manager.m_aValidatedSFX[AUDIO_LOOKUP_FEET] = false;
        Check(!manager.PlayGTASFX(AUDIO_LOOKUP_FEET, 0, 0, false), "invalid archive denied");
        manager.failOutput = true;
        Check(!manager.PlayGTASFX3D(AUDIO_LOOKUP_RADIO, RADIO_STREAM_POLICE, 1, position, true) && reads == settings.volumeReads,
              "failed output skips volume setup");
        g_pGame = nullptr;
        std::puts("2D/3D archive SFX: slider 0/32/64/255, loops, position, invalid archive and failed output passed");
    }
}
