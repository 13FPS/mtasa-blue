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
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using SharedUtil::Clamp;
using SharedUtil::Lerp;
using SharedUtil::MapContains;
using SharedUtil::UnlerpClamped;
using std::list;

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

    struct CLuaArguments
    {
        void PushString(const SString&) { Unexpected(); }
        void PushNumber(double) { Unexpected(); }
        void PushBoolean(bool) { Unexpected(); }
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
        explicit CBassAudio(DWORD channel, bool is3D = true) : m_pSound(channel), m_b3D(is3D) {}
        DWORD      m_pSound;
        bool       m_bPan = true;
        const bool m_b3D = true, m_bStream = false;
        const bool m_pVars = false;
        bool       m_bPendingPlay = false, m_bPaused = true;
        float      m_fVolume = 0.8f, m_fMinDistance = 10, m_fMaxDistance = 30;
        CVector    m_vecPosition;
        void       DoPulse(const CVector&, const CVector&);
        void       Process3D(const CVector&, const CVector&);
        void       ServiceVars() { Unexpected(); }
        bool       GetQueuedEvent(SSoundEventInfo&) { return false; }
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
        CClientSound(CBassAudio* audio, CClientSoundManager* manager) : m_pAudio(audio), m_pSoundManager(manager), m_b3D(audio->m_b3D) {}
        CBassAudio*          m_pAudio;
        CClientSoundManager* m_pSoundManager;
        const bool           m_b3D = true, m_bStream = false;
        bool                 m_bStreamFailureWarned = false;
        int                  m_LuaDebugInfo = 0;
        CVector              m_vecPosition;
        SString              m_strPath;
        void                 Process3D(const CVector&, const CVector&);
        bool                 IsSound3D() const { return m_b3D; }
        unsigned             GetDimension() const { return 0; }
        CSphere              GetWorldBoundingSphere() const { return CSphere(m_pAudio->m_vecPosition, m_pAudio->m_fMaxDistance); }
        void                 DistanceStreamIn();
        void                 DistanceStreamOut();
        bool                 IsFinished() const { return false; }
        CClientEntity*       GetAttachedTo() const { return nullptr; }
        void                 GetPosition(CVector&) { Unexpected(); }
        void                 DoAttaching() { Unexpected(); }
        void                 SetVelocity(const CVector&) { Unexpected(); }
        void                 UpdateSpatialData() { Unexpected(); }
        bool                 Create() { Unexpected(); }
        void                 Destroy() { Unexpected(); }
        void                 CallEvent(const char*, CLuaArguments&, bool) { Unexpected(); }
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

    struct CClientCamera
    {
        CVector  position{100, 200, 15};
        CVector  target{100, 201, 15};
        unsigned positions = 0, targets = 0;
        void     GetPosition(CVector& out)
        {
            ++positions;
            out = position;
        }
        void GetFixedTarget(CVector& out)
        {
            ++targets;
            out = target;
        }
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
        Core* GetCVars() { Unexpected(); }
        bool  GetValue(const char*, bool) { Unexpected(); }
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
        void                    ProcessStopQueues() { ++stopUpdates; }
        bool                    IsDistanceStreamedIn(CClientSound*) const { Unexpected(); }
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

    void Run()
    {
        Check(HIWORD(BASS_GetVersion()) == BASSVERSION, "BASS DLL and import header differ");
        Check(BASS_Init(0, 44100, 0, nullptr, nullptr) && BASS_GetDevice() == 0, "BASS no-sound device unavailable");
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
        CBassAudio   secondAudio(secondChannel);
        CClientSound secondSound(&secondAudio, &manager);
        manager.m_Sounds.push_back(&secondSound);
        spatial.candidate = &sound;
        unsigned activePulses = 0;
        auto     listen = [&](const CVector& offset, float pan, float volume)
        {
            audio.m_vecPosition = client.camera.position + offset;
            secondAudio.m_vecPosition = client.camera.position;
            Check(BASS_ChannelSetAttribute(secondChannel, BASS_ATTRIB_VOL, 0.2f), "Failed to prepare second spatial sample");
            manager.DoPulse();
            ++activePulses;
            Check(client.cameraRequests == activePulses && client.camera.positions == activePulses && client.camera.targets == activePulses,
                  "Active pulse must read this camera once");
            Check(spatial.queries == activePulses && spatial.lastListener == client.camera.position && manager.volumeUpdates == activePulses + 2 &&
                      manager.stopUpdates == activePulses + 2,
                  "Active pulse did not send the camera listener to streaming or process its queues");
            ExpectAttribute(flatChannel, BASS_ATTRIB_PAN, 0.25f);
            ExpectAttribute(flatChannel, BASS_ATTRIB_VOL, 0.6f);
            ExpectAttribute(secondChannel, BASS_ATTRIB_PAN, 0.0f);
            ExpectAttribute(secondChannel, BASS_ATTRIB_VOL, 0.8f);
            ExpectAttribute(channel, BASS_ATTRIB_PAN, pan);
            ExpectAttribute(channel, BASS_ATTRIB_VOL, volume);
        };
        listen(CVector{5, 0, 0}, 0.35f, 0.8f);
        listen(CVector{-5, 0, 0}, -0.35f, 0.8f);
        listen(CVector{20, 0, 0}, 1.0f, 0.8f * std::exp(-2.5f));
        Check(manager.m_DistanceStreamedInMap.count(&sound) == 1, "Nearby sound must stream in");
        spatial.candidate = nullptr;  // Previously active sounds must still be considered outside the spatial query.
        listen(CVector{-75, 0, 0}, -1.0f, 0.0f);
        Check(manager.m_DistanceStreamedInMap.empty(), "Far sound must stream out");
        spatial.candidate = &sound;
        client.camera.position = CVector{-80, 40, 90};
        client.camera.target = client.camera.position + CVector{0, -1, 0};
        listen(CVector{20, 0, 0}, -1.0f, 0.8f * std::exp(-2.5f));
        audio.m_bPan = false;
        listen(CVector{20, 0, 0}, 0.0f, 0.8f * std::exp(-2.5f));
        listen(CVector{0, 0, 30}, 0.0f, 0.0f);

        manager.m_Sounds.clear();
        spatial.candidate = nullptr;
        manager.DoPulse();
        Check(manager.volumeUpdates == activePulses + 3 && manager.stopUpdates == activePulses + 3 && spatial.queries == activePulses &&
                  client.cameraRequests == activePulses,
              "Last sound removal must stop listener queries");
        Check(BASS_StreamFree(secondChannel) && BASS_StreamFree(flatChannel) && BASS_StreamFree(channel) && BASS_Free(), "BASS no-sound cleanup failed");
        std::printf("BASS device 0; %u mixed camera pulses, 1 playing 2D-only pulse, 2 empty pulses, all pan and volume reads passed\n", activePulses);
    }
}

int main()
{
    try
    {
        Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s (BASS error %d)\n", error.what(), BASS_ErrorGetCode());
        return 1;
    }
}
