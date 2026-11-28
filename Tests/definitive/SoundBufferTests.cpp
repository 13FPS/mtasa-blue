// Run the memory branches of MTA's sound entity and decoder loader with BASS and BASS FX.
#include <windows.h>
#include <bass.h>
#include <bass_fx.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
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
    const wchar_t* FromUTF8(const SString&)
    {
        throw std::runtime_error("Unexpected file or URL request");
    }
    void OutputDebugLine(const char*)
    {
    }
    DWORD GetTickCount32()
    {
        return GetTickCount();
    }
    void CALLBACK BeatCallback(DWORD, double, void*)
    {
    }
    struct Core
    {
        Core* GetConsole() { return this; }
        void  Printf(const char*, ...) {}
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
        DWORD                  ConvertFileToMono(const SString&) { throw std::runtime_error("Unexpected file conversion"); }
        void                   SetFinishedCallbacks() {}
        SString                m_strPath;
        bool                   m_b3D{}, m_bStream{}, m_bLoop{}, m_bThrottle{}, m_bPendingPlay{};
        SSoundThreadVariables* m_pVars{};
        void*                  m_uiCallbackId{};
        DWORD                  m_pSound{};
        HMUSIC                 m_trackerSource{};
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
        bool Play(void*, unsigned, bool);
        bool Play3D(void*, unsigned, bool);
        bool AdoptBuffer(void*, unsigned);
        void ReleaseBuffer();
        void BeginSimulationOfPlayPosition() {}
        bool Create()
        {
            auto next = std::make_unique<CBassAudio>();
            next->m_b3D = m_b3D;
            next->m_bLoop = m_bLoop;
            if (!next->BeginLoadingMedia(m_Buffer.get(), m_uiBufferLength))
                return false;
            audio = std::move(next);
            return true;
        }
        std::unique_ptr<CBassAudio>                     audio;
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
        Check(type == BASS_FILE_MEMCOPY, "Memory decoder borrowed the entity's input");
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

    SString Wave()
    {
        constexpr DWORD count = 88200;
        SString         result(44 + count, '\x2a');
        unsigned char   header[44] = {'R', 'I', 'F',  'F',  0, 0, 0,    0,    'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16,  0,   0, 0, 1, 0,
                                      1,   0,   0x22, 0x56, 0, 0, 0x44, 0xac, 0,   0,   2,   0,   16,  0,   'd', 'a', 't', 'a', 0, 0, 0, 0};
        DWORD           riff = count + 36;
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
    void Test()
    {
        const auto wave = Wave(), tracker = Tracker();
        Check(!CopyRawAudio({}), "Empty Lua bytes yielded an allocation");
        CClientSound sound;
        Check(!sound.Play(new unsigned char[0], 0, false) && releases == 1, "Empty archive allocation was retained");
        Check(!sound.Play3D(nullptr, 1, false) && releases == 1, "Null spatial input was accepted");
        Check(sound.Play(CopyRawAudio(wave).release(), static_cast<unsigned>(wave.size()), false) && !sound.m_Buffer && releases == 2,
              "2D input did not transfer to the decoder");
        Check(Live(lastSource) && Live(lastReverse), "2D effect chain failed to load");
        sound.audio.reset();
        Check(!Live(lastSource) && !Live(lastReverse), "2D effect chain did not retire");

        auto  size = static_cast<unsigned>(wave.size());
        auto* bytes = CopyRawAudio(wave).release();
        Check(sound.Play3D(bytes, size, true) && sound.Play3D(bytes, size, true) && releases == 2 && sound.Create(), "Spatial input re-adoption failed");
        auto  first = std::move(sound.audio);
        DWORD firstSource = lastSource;
        Check(sound.Create(), "Could not reopen spatial input with first decoder queued");
        auto  second = std::move(sound.audio);
        DWORD secondSource = lastSource;
        Check(sound.Play3D(CopyRawAudio(wave).release(), size, false) && releases == 3 && sound.Create(), "Could not replace spatial source");
        auto  third = std::move(sound.audio);
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
        Check(sound.Play(CopyRawAudio(tracker).release(), static_cast<unsigned>(tracker.size()), true) && BASS_ChannelPlay(sound.audio->m_pSound, false),
              "Tracker fallback failed to play");
        sound.audio.reset();
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
        Check(BASS_Free(), "Could not close BASS");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s (BASS %d)\n", error.what(), BASS_ErrorGetCode());
        return 1;
    }
}
