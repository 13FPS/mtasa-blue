// Run the memory branches of MTA's sound entity and decoder loader with BASS and BASS FX.
#include <windows.h>
#include <bass.h>
#include <bass_fx.h>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{
    void Check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }
    struct SString : std::string
    {
        using std::string::string;
        const char* operator*() const { return c_str(); }
    };
    struct SSoundThreadVariables
    {
    };
    struct
    {
        bool Connect(const wchar_t*, DWORD, void*) { throw std::runtime_error("Unexpected URL request"); }
    } ms_streamConnections;
    struct WString : std::wstring
    {
        using std::wstring::wstring;
        operator const wchar_t*() const { return c_str(); }
    };
    WString FromUTF8(const SString& name)
    {
        Check(!name.empty(), "Empty filename reached the audio backend");
        return {name.begin(), name.end()};  // The authored test uses ASCII filenames.
    }
    void OutputDebugLine(const char*)
    {
    }
    DWORD clockValue = 20000;
    DWORD GetTickCount32()
    {
        return clockValue;
    }
    void CALLBACK BeatCallback(DWORD, double, void*)
    {
    }
    struct Core
    {
        Core* GetConsole() { return this; }
        int   lastBpmError = 0;
        void  Printf(const char* format, ...)
        {
            if (std::strstr(format, "in BPM detection"))
            {
                va_list args;
                va_start(args, format);
                lastBpmError = va_arg(args, int);
                va_end(args);
            }
        }
    } core;
    Core*                        g_pCore = &core;
    std::mutex                   ms_FailedFilesMutex;
    std::map<std::string, DWORD> ms_FailedAudioFiles;
    constexpr DWORD              FAILED_LOAD_RETRY_DELAY = 10000;
    constexpr size_t             MAX_FAILED_FILES_CACHE_SIZE = 1000;

    struct CBassAudio
    {
        ~CBassAudio();
        bool                   BeginLoadingMedia(const void*, unsigned int);
        float                  GetSoundBPM(const void*, unsigned int);
        DWORD                  ConvertFileToMono(const SString&) { throw std::runtime_error("Unexpected file conversion"); }
        void                   SetFinishedCallbacks() {}
        SString                m_strPath;
        bool                   m_b3D{}, m_bStream{}, m_bLoop{}, m_bThrottle{}, m_bPendingPlay{};
        SSoundThreadVariables* m_pVars{};
        void*                  m_uiCallbackId{};
        DWORD                  m_pSound{};
        HMUSIC                 m_trackerSource{};
        float                  m_fBPM{};
        std::optional<DWORD>   m_lastBpmFailure;
        float                  m_fTempo{}, m_fPitch{}, m_fSampleRate{}, m_fDefaultFrequency{};
    };
    unsigned releases = 0;
    struct CountedDelete
    {
        void operator()(unsigned char* bytes) const
        {
            ++releases;
            delete[] bytes;
        }
    };
    struct CClientSound
    {
        bool  Play(void*, unsigned, bool);
        bool  Play(const SString&, bool);
        bool  Play3D(void*, unsigned, bool);
        bool  AdoptBuffer(void*, unsigned);
        void  ReleaseBuffer();
        float GetSoundBPM();
        void  BeginSimulationOfPlayPosition() {}
        bool  Create()
        {
            auto next = std::make_unique<CBassAudio>();
            next->m_b3D = m_b3D;
            next->m_bLoop = m_bLoop;
            next->m_strPath = m_strPath;
            if (!next->BeginLoadingMedia(m_Buffer.get(), m_uiBufferLength))
                return false;
            m_pAudio = std::move(next);
            return true;
        }
        std::unique_ptr<CBassAudio>                     m_pAudio;
        std::unique_ptr<unsigned char[], CountedDelete> m_Buffer;
        unsigned                                        m_uiBufferLength{};
        bool                                            m_b3D{}, m_bStream{}, m_bLoop{}, m_bThrottle{}, m_bPan{};
        SString                                         m_strPath;
    };
    enum class Fail
    {
        None,
        Reverse,
        Tempo
    };
    Fail    failure = Fail::None;
    HSTREAM lastSource{}, lastReverse{};
    HSTREAM OpenFile(DWORD type, const void* data, QWORD offset, QWORD length, DWORD flags)
    {
        if (type == BASS_FILE_NAME)
            return BASS_StreamCreateFile(type, data, offset, length, flags);
        if (!(flags & BASS_MUSIC_PRESCAN))
        {
            Check(type == BASS_FILE_MEM, "BPM query made an unnecessary input copy");
            return BASS_StreamCreateFile(type, data, offset, length, flags);
        }
        Check(type == BASS_FILE_MEMCOPY, "Playback decoder borrowed the entity's input");
        HSTREAM source = BASS_StreamCreateFile(type, data, offset, length, flags);
        if (source)
        {
            // Change a sample in live, writable input. The decoder must still read
            // the original PCM. Restore it so the spatial entity can reopen it.
            constexpr DWORD pos = 64000;
            Check(length >= 44 + pos + 2, "Unexpected non-WAV stream decoder");
            auto*               sample = static_cast<unsigned char*>(const_cast<void*>(data)) + 44 + pos;
            const unsigned char expected = *sample;
            *sample ^= 255;
            unsigned char pcm[2]{};
            Check(BASS_ChannelSetPosition(source, pos, BASS_POS_BYTE) && BASS_ChannelGetData(source, pcm, sizeof(pcm)) == sizeof(pcm) && pcm[0] == expected,
                  "Decoder PCM changed with the original memory");
            *sample = expected;
            Check(BASS_ChannelSetPosition(source, 0, BASS_POS_BYTE), "Could not rewind WAV decoder");
        }
        return source;
    }
    HSTREAM Reverse(DWORD source, float block, DWORD flags)
    {
        lastSource = source;
        lastReverse = failure == Fail::Reverse ? 0 : BASS_FX_ReverseCreate(source, block, flags);
        return lastReverse;
    }
    HSTREAM Tempo(DWORD source, DWORD flags)
    {
        return failure == Fail::Tempo ? 0 : BASS_FX_TempoCreate(source, flags);
    }
    unsigned analyses = 0, bpmFrees = 0;
    bool     failAnalysis = false;
    DWORD    lastAnalysis = 0;
    float    Analyze(DWORD channel, double start, double end, DWORD range, DWORD flags, BPMPROGRESSPROC* callback, void* user)
    {
        ++analyses;
        lastAnalysis = channel;
        return BASS_FX_BPM_DecodeGet(failAnalysis ? 0 : channel, start, end, range, flags, callback, user);
    }
    BOOL FreeAnalysis(DWORD channel)
    {
        Check(channel == lastAnalysis, "BPM cleanup used a different decoder");
        ++bpmFrees;
        return BASS_FX_BPM_Free(channel);
    }
#define BASS_FX_BPM_DecodeGet Analyze
#define BASS_FX_BPM_Free      FreeAnalysis
#define BASS_StreamCreateFile OpenFile
#define BASS_FX_ReverseCreate Reverse
#define BASS_FX_TempoCreate   Tempo
#define SAFE_DELETE(ptr) \
    do \
    { \
        delete (ptr); \
        (ptr) = nullptr; \
    } while (false)
#include "SoundBufferMethods.generated.inl"
#undef SAFE_DELETE
#undef BASS_StreamCreateFile
#undef BASS_FX_ReverseCreate
#undef BASS_FX_TempoCreate
#undef BASS_FX_BPM_DecodeGet
#undef BASS_FX_BPM_Free

    SString Wave(DWORD count = 88200)
    {
        SString       result(44 + count, '\x2a');
        unsigned char header[44] = {'R', 'I', 'F',  'F',  0, 0, 0,    0,    'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16,  0,   0, 0, 1, 0,
                                    1,   0,   0x22, 0x56, 0, 0, 0x44, 0xac, 0,   0,   2,   0,   16,  0,   'd', 'a', 't', 'a', 0, 0, 0, 0};
        DWORD         riff = count + 36;
        std::memcpy(header + 4, &riff, 4);
        std::memcpy(header + 40, &count, 4);
        std::memcpy(result.data(), header, sizeof(header));
        return result;
    }
    SString Tracker()
    {
        SString result(1084 + 1024 + 32, '\0');
        result[43] = 16;  // 32-byte instrument
        result[45] = 64;
        result[49] = 1;
        result[950] = 1;  // single pattern
        std::memcpy(result.data() + 1080, "M.K.", 4);
        result[1084] = 1;
        result[1085] = static_cast<char>(0xac);
        result[1086] = 0x10;
        return result;
    }
    bool Live(DWORD handle)
    {
        BASS_CHANNELINFO info{};
        return handle && BASS_ChannelGetInfo(handle, &info);
    }
    void TestBpm()
    {
        constexpr DWORD rate = 22050;
        constexpr DWORD samples = rate * 24;
        SString         beat = Wave(samples * 2);
        std::memset(beat.data() + 44, 0, samples * 2);
        for (DWORD start = 0; start < samples; start += rate / 2)
        {
            for (unsigned i = 0; i < rate / 20; ++i)
            {
                const short value = static_cast<short>(26000 * std::exp(-static_cast<double>(i) / 180) * std::sin(i * 0.09));
                std::memcpy(beat.data() + 44 + 2 * (start + i), &value, sizeof(value));
            }
        }
        std::ofstream file("metronome.wav", std::ios::binary);
        file.write(beat.data(), beat.size());
        file.close();
        Check(file.good(), "Could not write authored metronome");
        const auto   size = static_cast<unsigned>(beat.size());
        unsigned     before = releases;
        CClientSound fromFile, fromMemory, spatial;
        Check(fromFile.Play("metronome.wav", false) && fromMemory.Play(CopyRawAudio(beat).release(), size, false) &&
                  spatial.Play3D(CopyRawAudio(beat).release(), size, false) && spatial.Create() && releases == before,
              "Metronome failed to open");

        const auto player = fromMemory.m_pAudio->m_pSound;
        Check(BASS_ChannelPlay(player, false) && BASS_ChannelPause(player) && BASS_ChannelSetPosition(player, 4 * rate, BASS_POS_BYTE),
              "Could not position paused metronome");
        const auto  position = BASS_ChannelGetPosition(player, BASS_POS_BYTE);
        const float fileBpm = fromFile.GetSoundBPM();
        Check(fileBpm >= 119 && fileBpm <= 121 && !Live(lastAnalysis), "File BPM missed the authored beat or leaked its decoder");
        Check(fromMemory.GetSoundBPM() == fileBpm && !fromMemory.m_Buffer && releases == before + 1 && !Live(lastAnalysis),
              "2D memory BPM differed from file or retained its input after caching");
        Check(spatial.GetSoundBPM() == fileBpm && spatial.m_Buffer && !Live(lastAnalysis), "Spatial BPM dropped the source needed for re-entry");
        Check(analyses == 3 && bpmFrees == 3 && fromMemory.GetSoundBPM() == fileBpm && fromFile.GetSoundBPM() == fileBpm && analyses == 3 &&
                  BASS_ChannelGetPosition(player, BASS_POS_BYTE) == position && BASS_ChannelIsActive(player) == BASS_ACTIVE_PAUSED,
              "Repeated BPM analysis sought on the player or reopened the decoder");
        std::printf("Authored 120 BPM, file %.0f, memory %.0f, spatial %.0f. Cached reads preserved paused byte position %llu.\n", fileBpm,
                    fromMemory.GetSoundBPM(), spatial.GetSoundBPM(), position);

        CClientSound retry, unrelated;
        Check(retry.Play(CopyRawAudio(beat).release(), size, false) && unrelated.Play(CopyRawAudio(beat).release(), size, false),
              "Retry sounds failed to open");
        failAnalysis = true;
        Check(!retry.GetSoundBPM() && retry.m_Buffer && !Live(lastAnalysis) && core.lastBpmError == BASS_ERROR_HANDLE && bpmFrees == 4,
              "BPM error was lost during cleanup or the decoder leaked");
        failAnalysis = false;
        clockValue += FAILED_LOAD_RETRY_DELAY - 1;
        Check(
            !retry.GetSoundBPM() && analyses == 4 && unrelated.GetSoundBPM() == fileBpm && !unrelated.m_Buffer && analyses == 5 && ms_FailedAudioFiles.empty(),
            "A failed analysis affected another memory sound or playback's file cache");
        ++clockValue;
        Check(retry.GetSoundBPM() == fileBpm && !retry.m_Buffer && analyses == 6 && bpmFrees == 6 && !Live(lastAnalysis), "BPM retry failed to recover");

        const auto   module = Tracker();
        CClientSound noBeat;
        Check(noBeat.Play(CopyRawAudio(module).release(), static_cast<unsigned>(module.size()), false), "Tracker for BPM failed to load");
        core.lastBpmError = 0;
        Check(!noBeat.GetSoundBPM() && noBeat.m_Buffer && !Live(lastAnalysis) && bpmFrees == 7 && !core.lastBpmError,
              "Tracker with no beat leaked an analysis decoder or reported an error");
        CBassAudio url;
        url.m_bStream = true;
        url.m_strPath = "https://unused.invalid/test";
        Check(!url.GetSoundBPM(nullptr, 0) && analyses == 7, "URL was reopened for a synchronous BPM query");
        url.m_fBPM = 104;
        Check(url.GetSoundBPM(nullptr, 0) == 104, "URL lost BPM already supplied by playback");
        std::puts("Independent BPM queries released WAV and tracker decoders, preserved errors, retried locally and skipped URL opens");
    }
    void Test()
    {
        const auto wave = Wave(), tracker = Tracker();
        Check(!CopyRawAudio({}), "Empty Lua bytes yielded an allocation");
        CClientSound sound;
        Check(!sound.Play(new unsigned char[0], 0, false) && releases == 1, "Empty archive allocation was retained");
        Check(!sound.Play3D(nullptr, 1, false) && releases == 1, "Null spatial input was accepted");
        Check(sound.Play(CopyRawAudio(wave).release(), static_cast<unsigned>(wave.size()), false) && sound.m_Buffer && releases == 1,
              "2D sound did not retain input for lazy BPM analysis");
        Check(Live(lastSource) && Live(lastReverse), "2D effect chain failed to load");
        sound.m_pAudio.reset();
        sound.ReleaseBuffer();
        Check(!Live(lastSource) && !Live(lastReverse), "2D effect chain did not retire");

        auto  size = static_cast<unsigned>(wave.size());
        auto* bytes = CopyRawAudio(wave).release();
        Check(sound.Play3D(bytes, size, true) && sound.Play3D(bytes, size, true) && releases == 2 && sound.Create(), "Spatial input re-adoption failed");
        auto  first = std::move(sound.m_pAudio);
        DWORD firstSource = lastSource;
        Check(sound.Create(), "Could not reopen spatial input with first decoder queued");
        auto  second = std::move(sound.m_pAudio);
        DWORD secondSource = lastSource;
        Check(sound.Play3D(CopyRawAudio(wave).release(), size, false) && releases == 3 && sound.Create(), "Could not replace spatial source");
        auto  third = std::move(sound.m_pAudio);
        DWORD thirdSource = lastSource;
        sound.ReleaseBuffer();
        Check(releases == 4 && Live(firstSource) && Live(secondSource) && Live(thirdSource), "Entity release damaged pending decoders");
        second.reset();
        Check(!Live(secondSource) && Live(firstSource) && Live(thirdSource), "Overlapping spatial chains did not retire independently");
        first.reset();
        third.reset();
        Check(!Live(firstSource) && !Live(thirdSource), "Spatial chain retained decoder after stop");

        for (auto& input : {wave, tracker})
        {
            for (auto point : {Fail::Reverse, Fail::Tempo})
            {
                failure = point;
                const unsigned before = releases;
                lastSource = lastReverse = 0;
                Check(!sound.Play(CopyRawAudio(input).release(), static_cast<unsigned>(input.size()), false) && releases == before + 1,
                      "Failed effect wrapped or retained its input");
                std::printf("%s %s failure: decoder %lu (%s), reverse %lu (%s)\n", input.size() == wave.size() ? "WAV" : "tracker",
                            point == Fail::Reverse ? "reverse" : "tempo", lastSource, Live(lastSource) ? "live" : "released", lastReverse,
                            Live(lastReverse) ? "live" : "released");
                Check(lastSource && !Live(lastSource) && (point == Fail::Reverse || lastReverse) && !Live(lastReverse),
                      "Failed effect left a source or reverse channel alive");
            }
        }
        failure = Fail::None;
        auto bad = CopyRawAudio("invalid");
        auto before = releases;
        Check(!sound.Play(bad.release(), 7, false) && releases == before + 1, "Undecodable bytes were retained");
        Check(sound.Play(CopyRawAudio(tracker).release(), static_cast<unsigned>(tracker.size()), true) && BASS_ChannelPlay(sound.m_pAudio->m_pSound, false),
              "Tracker fallback failed to play");
        sound.m_pAudio.reset();
        Check(!Live(lastSource) && !Live(lastReverse), "Tracker fallback retained music after normal stop");
        std::puts("2D release, independent PCM, overlapping 3D reopens, replacement, empty and invalid data passed");
        std::puts("Real WAV and tracker chains released every native handle on reverse/tempo failure and normal stop");
    }
}
int main()
{
    try
    {
        Check(BASS_Init(0, 44100, 0, nullptr, nullptr), "Could not open BASS no-sound device");
        Test();
        TestBpm();
        Check(BASS_Free(), "Could not close BASS");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s (BASS %d)\n", error.what(), BASS_ErrorGetCode());
        return 1;
    }
}
