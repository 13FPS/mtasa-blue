/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Run the client listener pulse against a stereo BASS no-sound channel
 *****************************************************************************/
#include <windows.h>
#include <bass.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.IntTypes.h>
#include <SharedUtil.Misc.h>
#include <SharedUtil.Legacy.h>
#include <SString.hpp>
#include <CSphere.h>
#include <CMatrix.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <optional>
#include <atomic>
#include <thread>
#include <string_view>
#include <variant>

using SharedUtil::CCriticalSection;
using SharedUtil::Clamp;
using SharedUtil::Lerp;
using SharedUtil::MapContains;
using SharedUtil::UnlerpClamped;
using std::list;
#include "WindowsCriticalSection.generated.inl"

namespace
{
    void Check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    // Only the active, unattached local-file receiver is provided. An unexpected
    // stream, attachment or script event makes the host fail at that boundary.
    [[noreturn]] void Unexpected()
    {
        throw std::runtime_error("Unexpected non-listener operation");
    }

    // Destruction follows removal of the worker's callback ID. No worker
    // holds the block during these three result-ownership cases.
#include "SoundWorkerState.generated.inl"

    void CheckUnclaimedStream(const char* url)
    {
        BASS_CHANNELINFO info{};
        HSTREAM          pending = BASS_StreamCreateURL(url, 0, BASS_STREAM_AUTOFREE, nullptr, nullptr);
        Check(pending && BASS_ChannelGetInfo(pending, &info), "Pending channel creation failed");
        auto* result = new SSoundThreadVariables;
        result->pSound = pending;
        result->bStreamCreateResult = true;
        delete result;
        Check(!BASS_ChannelGetInfo(pending, &info) && BASS_ErrorGetCode() == BASS_ERROR_HANDLE, "Dropping an unclaimed stream leaked the BASS channel");

        HSTREAM claimed = BASS_StreamCreateURL(url, 0, BASS_STREAM_AUTOFREE, nullptr, nullptr);
        Check(claimed != 0, "Claimed channel creation failed");
        result = new SSoundThreadVariables;
        result->pSound = claimed;
        result->bStreamCreateResult = false;
        delete result;
        Check(BASS_ChannelGetInfo(claimed, &info) && BASS_StreamFree(claimed), "Dropping a consumed result freed the playing owner's channel");

        result = new SSoundThreadVariables;
        result->bStreamCreateResult = true;
        delete result;
        std::puts("Unclaimed BASS handle released; consumed and failed stream results passed");
    }

    struct CLuaArguments
    {
        void    PushString(const SString& value) { reason = value; }
        SString reason;
        void    PushNumber(double value) { number = value; }
        double  number = 0;
        void    PushBoolean(bool) { Unexpected(); }
    };
    enum SoundEvent
    {
        SOUND_EVENT_FINISHED_DOWNLOAD,
        SOUND_EVENT_CHANGED_META,
        SOUND_EVENT_STREAM_RESULT,
        SOUND_EVENT_BEAT
    };
    struct SSoundEventInfo
    {
        SoundEvent type{};
        bool       bBool = false;
        double     dNumber = 0;
        SString    strString, strError;
    };

    struct CBassAudio
    {
        explicit CBassAudio(DWORD channel, bool is3D = true, bool stream = false) : m_pSound(channel), m_b3D(is3D), m_bStream(stream), m_pVars(stream) {}
        DWORD      m_pSound;
        bool       m_bPan = true;
        const bool m_b3D = true, m_bStream = false;
        const bool m_pVars = false;
        bool       m_bPendingPlay = false, m_bPaused = false;
        float      m_fVolume = 0.8f, m_fMinDistance = 10, m_fMaxDistance = 30;
        CVector    m_vecPosition;
        void       DoPulse(const CMatrix*);
        void       SetPaused(bool paused);
        void       Process3D(const CMatrix&);
        void       ServiceVars() { ++streamPulses; }
        unsigned   streamPulses = 0;
        bool       queuedBeat = false;
        bool       GetQueuedEvent(SSoundEventInfo& event)
        {
            if (!queuedBeat)
                return false;
            queuedBeat = false;
            event.type = SOUND_EVENT_BEAT;
            event.dNumber = 7;
            return true;
        }
    };

    struct CClientEntity
    {
    };
    struct CStaticFunctionDefinitions
    {
        static bool GetElementVelocity(CClientEntity&, CVector&) { Unexpected(); }
    };
    struct CClientSoundManager;
    struct CClientSound
    {
        CClientSound(CBassAudio* audio, CClientSoundManager* manager)
            : m_pAudio(audio), m_pSoundManager(manager), m_b3D(audio->m_b3D), m_bStream(audio->m_bStream)
        {
        }
        CBassAudio*          m_pAudio;
        CClientSoundManager* m_pSoundManager;
        const bool           m_b3D = true, m_bStream = false;
        bool                 m_bStreamFailureWarned = false;
        int                  m_LuaDebugInfo = 0;
        CVector              m_vecPosition;
        SString              m_strPath;
        void                 Process3D(const CMatrix*);
        void                 SetPaused(bool paused);
        bool                 IsPaused();
        bool                 m_bPaused = false;
        struct
        {
            bool paused = false;
            void SetPaused(bool value) { paused = value; }
        } m_SimulatedPlayPosition;
        bool           IsSound3D() const { return m_b3D; }
        unsigned       GetDimension() const { return 0; }
        CSphere        GetWorldBoundingSphere() const { return CSphere(m_pAudio->m_vecPosition, m_pAudio->m_fMaxDistance); }
        void           DistanceStreamIn();
        void           DistanceStreamOut();
        bool           IsFinished() const { return false; }
        CClientEntity* GetAttachedTo() const { return nullptr; }
        void           GetPosition(CVector&) { Unexpected(); }
        void           DoAttaching() { Unexpected(); }
        void           SetVelocity(const CVector&) { Unexpected(); }
        void           UpdateSpatialData() { Unexpected(); }
        bool           Create() { Unexpected(); }
        void           Destroy() { Unexpected(); }
        void           CallEvent(const char* name, CLuaArguments& arguments, bool children)
        {
            if (std::string_view(name) == "onClientSoundStopped" || std::string_view(name) == "onClientSoundStarted")
            {
                const bool paused = std::string_view(name) == "onClientSoundStopped";
                Check(!children && arguments.reason == (paused ? "paused" : "resumed"), "Incorrect pause event");
                Check(IsPaused() == paused && m_SimulatedPlayPosition.paused == paused && (!m_pAudio || m_pAudio->m_bPaused == paused),
                      "Pause event exposed stale playback state");
                ++pauseEvents;
                if (!paused && pauseOnResume)
                    SetPaused(true);
                return;
            }
            if (std::string_view(name) != "onClientSoundBeat" || arguments.number != 7 || !children)
                Unexpected();
            ++beats;
        }
        unsigned beats = 0;
        unsigned pauseEvents = 0;
        bool     pauseOnResume = false;
    };

    struct CClientPlayerVoice
    {
        void SetPaused(bool) { Unexpected(); }
    };
    struct CClientPlayer
    {
        CClientPlayerVoice* GetVoice() { Unexpected(); }
    };
    struct CLuaAudioDefs
    {
        static bool SetSoundPaused(std::variant<CClientSound*, CClientPlayer*> sound, bool paused);
    };

    // Expose the listener position observed by the production spatial query.
    using CClientEntityResult = std::list<CClientSound*>;
    template <typename T>
    T* DynamicCast(CClientSound* sound)
    {
        return static_cast<T*>(sound);
    }
    struct SpatialDatabase
    {
        unsigned      queries = 0;
        CVector       lastListener;
        CClientSound* candidate = nullptr;
        void          SphereQuery(CClientEntityResult& result, const CSphere& sphere)
        {
            Check(sphere.fRadius == 0, "Sound streaming must query the listener point");
            ++queries;
            lastListener = sphere.vecPosition;
            if (candidate)
                result.push_back(candidate);
        }
    };
    SpatialDatabase  spatial;
    SpatialDatabase* GetClientSpatialDatabase()
    {
        return &spatial;
    }

    enum class EFixedCameraMode
    {
        TARGET
    };
    struct CClientCamera
    {
        CMatrix             m_matFixedMatrix{CVector{100, 200, 15}};
        CVector             m_vecFixedTarget;
        float               m_fRoll = 0;
        EFixedCameraMode    m_FixedCameraMode = EFixedCameraMode::TARGET;
        bool                m_bFixed = false;
        std::optional<bool> m_pulseMatrixAvailable{true};
        mutable unsigned    activePoseReads = 0;
        bool                GetMatrix(CMatrix&) const;
        void                SetFixedTarget(const CVector&, float);
        const CMatrix&      AcquirePulseMatrix() const
        {
            ++activePoseReads;
            return m_matFixedMatrix;
        }
        bool IsInFixedMode() const { return m_bFixed; }
        void ToggleCameraFixedMode(bool fixed) { m_bFixed = fixed; }
        void UpdateCenterOfWorldFromFixedMatrix() {}
        void InvalidateCachedTransforms() {}
    };
    // No player lookup exists on either receiver. Reintroducing one into the
    // extracted production methods is a compile-time regression.
    struct ClientManager
    {
        CClientCamera  camera;
        unsigned       cameraRequests = 0;
        CClientCamera* GetCamera()
        {
            ++cameraRequests;
            return &camera;
        }
    };
    struct ClientGame
    {
        ClientGame* GetElementDeleter() { Unexpected(); }
        void        Delete(CClientSound*) { Unexpected(); }
        ClientGame* GetScriptDebugging() { Unexpected(); }
        void        LogWarning(int, const SString&) { Unexpected(); }
    };
    ClientGame  clientGame;
    ClientGame* g_pClientGame = &clientGame;
    struct Core
    {
        Core* GetCVars() { return this; }
        bool  GetValue(const char* name, bool)
        {
            Check(std::string_view(name) == "allow_external_sounds", "Unexpected CVar read");
            return true;
        }
    };
    Core  core;
    Core* g_pCore = &core;
    void  OutputDebugLine(const SString&)
    {
        Unexpected();
    }

    struct CClientSoundManager
    {
        ClientManager*          m_pClientManager;
        list<CClientSound*>     m_Sounds;
        unsigned                volumeUpdates = 0, stopUpdates = 0;
        std::set<CClientSound*> m_DistanceStreamedInMap;
        unsigned                m_usDimension = 0;
        void                    DoPulse();
        void                    UpdateVolume() { ++volumeUpdates; }
        void                    UpdateDistanceStreaming(const CVector& listener);
        void                    ProcessStopQueues()
        {
            ++stopUpdates;
            ProcessStopQueues(false);
        }
        void ProcessStopQueues(bool);
        void QueueChannelStop(DWORD);
        struct CElapsedTime
        {
            int Get() const { Unexpected(); }
        };
        CCriticalSection                    m_CS;
        std::vector<DWORD>                  m_ChannelStopQueue;
        std::map<CBassAudio*, CElapsedTime> m_AudioStopQueue;
        bool                                IsDistanceStreamedIn(CClientSound*) const { Unexpected(); }
    };

    void CClientSound::DistanceStreamIn()
    {
        m_pSoundManager->m_DistanceStreamedInMap.insert(this);
    }
    void CClientSound::DistanceStreamOut()
    {
        m_pSoundManager->m_DistanceStreamedInMap.erase(this);
    }

#include "ListenerMethods.generated.inl"

    void ExpectAttribute(DWORD channel, DWORD attribute, float expected)
    {
        float actual = 0;
        Check(BASS_ChannelGetAttribute(channel, attribute, &actual), "BASS failed to read the channel attribute");
        if (std::abs(actual - expected) > 0.0001f)
        {
            std::fprintf(stderr, "attribute %lu expected %.6f, got %.6f\n", attribute, expected, actual);
            throw std::runtime_error("Unexpected BASS channel attribute");
        }
    }

    void CheckPausePlayback(bool spatialSound)
    {
        HSTREAM channel = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, nullptr);
        Check(channel != 0, "Could not create the pause test channel");
        std::array<short, 16384> samples{};
        Check(BASS_StreamPutData(channel, samples.data(), sizeof(samples)) != static_cast<DWORD>(-1), "Could not queue the pause test audio");
        CBassAudio   audio(channel, spatialSound);
        CClientSound sound(&audio, nullptr);
        CMatrix      listener;
        const auto   setPaused = [&](bool paused) { Check(CLuaAudioDefs::SetSoundPaused(&sound, paused), "Lua pause request failed"); };
        audio.m_bPendingPlay = true;
        setPaused(false);
        Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_STOPPED && !sound.pauseEvents, "Redundant resume bypassed first-pulse playback");
        setPaused(true);
        audio.DoPulse(&listener);
        Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_STOPPED, "Paused sound started on the first available pose");
        setPaused(false);
        Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_STOPPED, "Resume bypassed the pending first play");
        audio.DoPulse(nullptr);
        if (spatialSound)
        {
            Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_STOPPED, "Spatial first play bypassed the missing listener");
            audio.DoPulse(&listener);
        }
        Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_PLAYING, "Pending first play did not start");
        setPaused(true);
        Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_PAUSED, "Playing channel was not paused immediately");
        setPaused(false);
        if (spatialSound)
        {
            audio.DoPulse(nullptr);
            Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_PAUSED, "Spatial resume bypassed the missing listener");
            audio.DoPulse(&listener);
        }
        Check(BASS_ChannelIsActive(channel) == BASS_ACTIVE_PLAYING, "Channel failed to resume");
        setPaused(true);
        sound.pauseOnResume = true;
        setPaused(false);
        audio.DoPulse(&listener);
        Check(sound.IsPaused() && BASS_ChannelIsActive(channel) == BASS_ACTIVE_PAUSED && sound.pauseEvents == 7, "Resume overwrote the event handler's pause");
        Check(BASS_StreamFree(channel), "Could not release pause test channel");
    }

    void CheckQueuedChannels()
    {
        CClientSoundManager      manager{};
        std::array<HSTREAM, 128> channels{};
        for (auto& channel : channels)
        {
            channel = BASS_StreamCreate(44100, 2, BASS_STREAM_AUTOFREE, STREAMPROC_PUSH, nullptr);
            Check(channel != 0, "Could not create stop queue channel");
        }
        std::atomic<bool> done{false};
        std::thread       worker(
            [&]
            {
                for (auto channel : channels)
                    manager.QueueChannelStop(channel);
                done = true;
            });
        do
        {
            manager.ProcessStopQueues(false);
            std::this_thread::yield();
        } while (!done);
        worker.join();
        manager.ProcessStopQueues(true);
        BASS_CHANNELINFO info{};
        for (auto channel : channels)
            Check(!BASS_ChannelGetInfo(channel, &info) && BASS_ErrorGetCode() == BASS_ERROR_HANDLE, "Stop queue lost an unpublished channel");
        std::puts("Worker and client pulse retired 128 queued BASS channels");
    }

    void Run(const char* url)
    {
        Check(HIWORD(BASS_GetVersion()) == BASSVERSION, "BASS DLL and import header differ");
        Check(BASS_Init(0, 44100, 0, nullptr, nullptr) && BASS_GetDevice() == 0, "BASS no-sound device unavailable");
        CheckUnclaimedStream(url);
        CheckQueuedChannels();
        CheckPausePlayback(false);
        CheckPausePlayback(true);
        std::puts("Lua pause/resume passed for pending, playing and re-paused 2D and 3D channels");
        HSTREAM channel = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, nullptr);
        Check(channel != 0, "BASS stereo channel creation failed");
        ClientManager       client;
        CClientSoundManager manager{&client};
        CBassAudio          audio{channel};
        CClientSound        sound{&audio, &manager};
        manager.DoPulse();
        Check(manager.volumeUpdates == 1 && manager.stopUpdates == 1 && !spatial.queries && !client.cameraRequests,
              "Empty pulse must update volume and stops without requesting a listener");

        // A ready 2D sound must play on a nonempty frame with no listener at all.
        HSTREAM flatChannel = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, nullptr);
        Check(flatChannel != 0, "BASS 2D stereo channel creation failed");
        std::array<short, 16384> samples{};
        Check(BASS_StreamPutData(flatChannel, samples.data(), sizeof(samples)) != static_cast<DWORD>(-1), "Unable to queue 2D stereo data");
        Check(BASS_ChannelSetAttribute(flatChannel, BASS_ATTRIB_PAN, 0.25f) && BASS_ChannelSetAttribute(flatChannel, BASS_ATTRIB_VOL, 0.6f),
              "Unable to initialize 2D channel attributes");
        CBassAudio   flatAudio(flatChannel, false);
        CClientSound flatSound(&flatAudio, &manager);
        flatAudio.m_bPendingPlay = true;
        flatAudio.m_bPaused = false;
        manager.m_Sounds.push_back(&flatSound);
        manager.DoPulse();
        Check(!flatAudio.m_bPendingPlay && BASS_ChannelIsActive(flatChannel) == BASS_ACTIVE_PLAYING, "2D-only pulse must start the BASS channel");
        Check(manager.volumeUpdates == 2 && manager.stopUpdates == 2 && !spatial.queries && !client.cameraRequests, "2D-only pulse requested a camera");
        ExpectAttribute(flatChannel, BASS_ATTRIB_PAN, 0.25f);
        ExpectAttribute(flatChannel, BASS_ATTRIB_VOL, 0.6f);

        manager.m_Sounds.push_back(&sound);
        HSTREAM secondChannel = BASS_StreamCreate(44100, 2, 0, STREAMPROC_PUSH, nullptr);
        Check(secondChannel != 0, "Second spatial channel creation failed");
        CBassAudio   secondAudio(secondChannel, true, true);
        CClientSound secondSound(&secondAudio, &manager);
        manager.m_Sounds.push_back(&secondSound);
        spatial.candidate = &sound;
        // The camera returns its defined compatibility matrix but refuses the pose.
        // The 3D stream must service its queued event without starting at the origin.
        client.camera.m_pulseMatrixAvailable = false;
        secondAudio.m_bPendingPlay = true;
        secondAudio.m_bPaused = false;
        secondAudio.queuedBeat = true;
        Check(BASS_StreamPutData(secondChannel, samples.data(), sizeof(samples)) != static_cast<DWORD>(-1), "Unable to queue 3D data");
        Check(BASS_ChannelSetAttribute(secondChannel, BASS_ATTRIB_VOL, 0.4f), "Prepare unstarted stream");
        manager.DoPulse();
        Check(client.cameraRequests == 1 && client.camera.activePoseReads == 1 && !spatial.queries && manager.stopUpdates == 3 && manager.volumeUpdates == 3 &&
                  secondAudio.streamPulses == 1 && secondSound.beats == 1,
              "Missing camera must allow stream events, volume and stop queues");
        Check(secondAudio.m_bPendingPlay && BASS_ChannelIsActive(secondChannel) == BASS_ACTIVE_STOPPED, "Missing listener must defer the 3D play request");
        ExpectAttribute(secondChannel, BASS_ATTRIB_VOL, 0.4f);
        client.camera.m_pulseMatrixAvailable = true;

        unsigned activePulses = 0;
        auto     listen = [&](const CVector& offset, float pan, float volume)
        {
            audio.m_vecPosition = client.camera.m_matFixedMatrix.vPos + offset;
            secondAudio.m_vecPosition = client.camera.m_matFixedMatrix.vPos;
            Check(BASS_ChannelSetAttribute(secondChannel, BASS_ATTRIB_VOL, 0.2f), "Failed to prepare second spatial sample");
            manager.DoPulse();
            ++activePulses;
            Check(client.cameraRequests == activePulses + 1, "Active pulse must read this camera once");
            Check(spatial.queries == activePulses && spatial.lastListener == client.camera.m_matFixedMatrix.vPos && manager.volumeUpdates == activePulses + 3 &&
                      manager.stopUpdates == activePulses + 3,
                  "Active pulse did not send the camera listener to streaming or process its queues");
            ExpectAttribute(flatChannel, BASS_ATTRIB_PAN, 0.25f);
            ExpectAttribute(flatChannel, BASS_ATTRIB_VOL, 0.6f);
            ExpectAttribute(secondChannel, BASS_ATTRIB_PAN, 0.0f);
            ExpectAttribute(secondChannel, BASS_ATTRIB_VOL, 0.8f);
            ExpectAttribute(channel, BASS_ATTRIB_PAN, pan);
            ExpectAttribute(channel, BASS_ATTRIB_VOL, volume);
        };
        // Handedness is determined by front and up, even for a mirrored input matrix.
        client.camera.m_matFixedMatrix.vRight = CVector(-1, 0, 0);
        listen(CVector{5, 0, 0}, 0.35f, 0.8f);
        Check(!secondAudio.m_bPendingPlay && BASS_ChannelIsActive(secondChannel) == BASS_ACTIVE_PLAYING, "Spatial stream must resume on the first valid pose");
        listen(CVector{-5, 0, 0}, -0.35f, 0.8f);
        listen(CVector{20, 0, 0}, 1.0f, 0.8f * std::exp(-2.5f));
        Check(manager.m_DistanceStreamedInMap.count(&sound) == 1, "Nearby sound must stream in");
        spatial.candidate = nullptr;  // Previously active sounds must still be considered outside the spatial query.
        listen(CVector{-75, 0, 0}, -1.0f, 0.0f);
        Check(manager.m_DistanceStreamedInMap.empty(), "Far sound must stream out");
        spatial.candidate = &sound;
        client.camera.m_matFixedMatrix.vPos = CVector{-80, 40, 90};
        const CVector position = client.camera.m_matFixedMatrix.vPos;
        client.camera.SetFixedTarget(position + CVector{0, -1, 0}, 0.0f);
        listen(CVector{20, 0, 0}, -1.0f, 0.8f * std::exp(-2.5f));
        client.camera.SetFixedTarget(position + CVector{0, 1, 0}, 0.0f);
        // A scripted fixed matrix may disagree with its own front/up pair.
        client.camera.m_matFixedMatrix.vRight = -client.camera.m_matFixedMatrix.vRight;
        listen(CVector{15, 0, 0}, 0.675f, 0.8f * std::exp(-1.25f));
        // A level camera preserves the old azimuth even for a source above the listener.
        listen(CVector{12, 0, 16}, 1.0f, 0.8f * std::exp(-2.5f));
        listen(CVector{12, 16, 0}, 0.6f, 0.8f * std::exp(-2.5f));
        listen(CVector{-12, 16, 0}, -0.6f, 0.8f * std::exp(-2.5f));
        client.camera.SetFixedTarget(position + CVector{0, 1, 0}, 90.0f);
        listen(CVector{0, 0, 5}, 0.35f, 0.8f);
        listen(CVector{0, 0, -20}, -1.0f, 0.8f * std::exp(-2.5f));
        listen(CVector{20, 0, 0}, 0.0f, 0.8f * std::exp(-2.5f));
        client.camera.SetFixedTarget(position + CVector{0, 1, 0}, 180.0f);
        listen(CVector{20, 0, 0}, -1.0f, 0.8f * std::exp(-2.5f));
        client.camera.SetFixedTarget(position + CVector{0, 0, 1}, 0.0f);
        listen(CVector{20, 0, 0}, 1.0f, 0.8f * std::exp(-2.5f));
        listen(CVector{0, 0, 20}, 0.0f, 0.8f * std::exp(-2.5f));
        client.camera.SetFixedTarget(position + CVector{0, 0, -1}, 0.0f);
        listen(CVector{-20, 0, 0}, -1.0f, 0.8f * std::exp(-2.5f));
        listen(CVector{}, 0.0f, 0.8f);
        audio.m_bPan = false;
        listen(CVector{20, 0, 0}, 0.0f, 0.8f * std::exp(-2.5f));
        listen(CVector{0, 0, 30}, 0.0f, 0.0f);

        manager.m_Sounds.clear();
        spatial.candidate = nullptr;
        manager.DoPulse();
        Check(manager.volumeUpdates == activePulses + 4 && manager.stopUpdates == activePulses + 4 && spatial.queries == activePulses &&
                  client.cameraRequests == activePulses + 1,
              "Last sound removal must stop listener queries");
        Check(BASS_StreamFree(secondChannel) && BASS_StreamFree(flatChannel) && BASS_StreamFree(channel) && BASS_Free(), "BASS no-sound cleanup failed");
        std::printf(
            "BASS device 0; %u mixed camera pulses, 1 missing pose with a queued beat, 1 playing 2D-only pulse, 2 empty pulses, all pan and volume reads "
            "passed\n",
            activePulses);
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Expected a local audio URL");
        Run(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s (BASS error %d)\n", error.what(), BASS_ErrorGetCode());
        return 1;
    }
}
