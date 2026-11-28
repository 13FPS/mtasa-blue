/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Own asynchronous BASS URL connections for one client session
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <bass.h>
#include <chrono>
#include <list>
#include <mutex>
#include <string>

class CClientSoundManager;

// The main thread starts jobs, retires the session and reaps thread handles.
// Workers only access their own inputs and the synchronized result receiver.
class CSoundStreamConnections
{
public:
    using Publish = void (*)(CClientSoundManager&, void*, HSTREAM);
    explicit CSoundStreamConnections(Publish publish) : m_publish(publish) {}

    bool CanBeginSession()
    {
        Reap();
        return !m_manager && m_jobs.empty();
    }

    void Open(CClientSoundManager& manager, const char* userAgent)
    {
        std::lock_guard lock(m_mutex);
        // BASS borrows its HTTP agent. A late connection can still be using
        // the previous value after the manager's destructor returns.
        m_userAgent = userAgent;
        BASS_SetConfigPtr(BASS_CONFIG_NET_AGENT, m_userAgent.c_str());
        m_manager = &manager;
    }

    bool Connect(const wchar_t* url, DWORD flags, void* soundId)
    {
        if (!m_manager)
            return false;
        Reap();
        Job& job = m_jobs.emplace_back(*this, url, flags, soundId);
        // The client loader can release its reference immediately after its
        // bounded shutdown wait. Acquire the worker's reference before launch.
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&Run), &job.module))
        {
            job.thread = CreateThread(nullptr, 0, &Run, &job, 0, nullptr);
            if (job.thread)
                return true;
            FreeLibrary(job.module);
        }
        m_jobs.pop_back();
        return false;
    }

    void Close()
    {
        {
            // Wait for any result already being delivered and close the
            // receiver before the main thread flushes queues and frees BASS.
            std::lock_guard lock(m_mutex);
            m_manager = nullptr;
        }
        BASS_SetConfigPtr(BASS_CONFIG_NET_AGENT, static_cast<const void*>(nullptr));
        // The shipped Win32 import library predates this optional export.
        // On older runtimes the worker still holds the DLL until it returns.
        HMODULE bass = GetModuleHandleW(L"bass.dll");
        auto    cancel = bass ? reinterpret_cast<decltype(&BASS_StreamCancel)>(GetProcAddress(bass, "BASS_StreamCancel")) : nullptr;
        if (cancel)
            for (auto& job : m_jobs)
                cancel(&job);
    }

    void Wait(DWORD timeout)
    {
        using namespace std::chrono;
        const auto deadline = steady_clock::now() + milliseconds(timeout);
        for (auto& job : m_jobs)
        {
            const auto remaining = duration_cast<milliseconds>(deadline - steady_clock::now()).count();
            WaitForSingleObject(job.thread, remaining > 0 ? static_cast<DWORD>(remaining) : 0);
        }
        Reap();
    }

private:
    struct Job
    {
        Job(CSoundStreamConnections& owner, const wchar_t* url, DWORD flags, void* id) : owner(owner), url(url), flags(flags), soundId(id) {}
        ~Job()
        {
            if (thread)
                CloseHandle(thread);
        }
        CSoundStreamConnections& owner;
        std::wstring             url;
        DWORD                    flags;
        void*                    soundId;
        HMODULE                  module = nullptr;
        HANDLE                   thread = nullptr;
    };

    static DWORD WINAPI Run(void* context)
    {
        Job& job = *static_cast<Job*>(context);
        job.owner.RunConnection(job);
        // This is the final operation in the client DLL for this thread.
        FreeLibraryAndExitThread(job.module, 0);
    }

    void RunConnection(Job& job)
    {
        {
            std::lock_guard lock(m_mutex);
            if (!m_manager)
                return;
        }
        // Each live job has its own cancellation key. If shutdown beats entry
        // into BASS, this thread still holds the DLL and gates session reuse.
        HSTREAM         stream = BASS_StreamCreateURL(job.url.c_str(), 0, job.flags | BASS_UNICODE, nullptr, &job);
        std::lock_guard lock(m_mutex);
        if (m_manager)
            m_publish(*m_manager, job.soundId, stream);
        // After Close(), BASS_Free owns cleanup for the retired device. No new
        // device/session can start until this thread handle is signaled.
    }

    void Reap()
    {
        m_jobs.remove_if([](const Job& job) { return WaitForSingleObject(job.thread, 0) == WAIT_OBJECT_0; });
    }

    std::list<Job>       m_jobs;
    std::string          m_userAgent;
    std::mutex           m_mutex;
    CClientSoundManager* m_manager = nullptr;
    Publish              m_publish;
};
