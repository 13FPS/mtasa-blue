// Load, retire and reopen the URL connection owner in an ordinary local DLL.
#include <windows.h>
#include <bass.h>
#include <cstdio>
#include <stdexcept>
#include <string>

#ifdef STREAM_FIXTURE_DLL
namespace
{
    DWORD  gatePhase = 0;
    HANDLE reached = nullptr, release = nullptr;
    void   Gate(DWORD phase)
    {
        if (gatePhase == phase)
        {
            SetEvent(reached);
            if (WaitForSingleObject(release, 10000) != WAIT_OBJECT_0)
                std::terminate();
        }
    }
    HSTREAM ControlledOpen(const wchar_t* url, DWORD offset, DWORD flags, DOWNLOADPROC* proc, void* key)
    {
        Gate(1);
        HSTREAM stream = BASS_StreamCreateURL(url, offset, flags, proc, key);
        Gate(2);
        return stream;
    }
}
    // Hold the worker immediately before or after the real BASS network call.
    #define BASS_StreamCreateURL ControlledOpen
    #include "../../Client/mods/deathmatch/logic/CSoundStreamConnections.h"
    #undef BASS_StreamCreateURL

class CClientSoundManager
{
};
namespace
{
    CClientSoundManager manager;
    HANDLE              resultReady = nullptr;
    unsigned            results = 0;
    void*               lastId = nullptr;
    HSTREAM             lastStream = 0;
    void                Publish(CClientSoundManager& client, void* id, HSTREAM stream)
    {
        if (&client != &manager)
            std::terminate();
        ++results;
        lastId = id;
        lastStream = stream;
        SetEvent(resultReady);
    }
    CSoundStreamConnections connections(Publish);
}

extern "C" __declspec(dllexport) bool Begin(HANDLE ready)
{
    if (!connections.CanBeginSession() || !BASS_Init(0, 44100, 0, nullptr, nullptr))
        return false;
    resultReady = ready;
    connections.Open(manager, "MTA loopback fixture");
    return true;
}
extern "C" __declspec(dllexport) bool Connect(const wchar_t* url, void* id, DWORD phase, HANDLE entered, HANDLE resume)
{
    gatePhase = phase;
    reached = entered;
    release = resume;
    return connections.Connect(url, BASS_STREAM_AUTOFREE, id);
}
extern "C" __declspec(dllexport) bool Consume(unsigned expectedCount, void* expectedId)
{
    BASS_CHANNELINFO info{};
    return results == expectedCount && lastId == expectedId && BASS_ChannelGetInfo(lastStream, &info) && BASS_StreamFree(lastStream);
}
extern "C" __declspec(dllexport) bool Retire(DWORD budget)
{
    connections.Close();
    BASS_Stop();
    BASS_Free();
    connections.Wait(budget);
    return connections.CanBeginSession();
}
extern "C" __declspec(dllexport) unsigned Publications()
{
    return results;
}

#else
namespace
{
    void Check(bool value, const char* reason)
    {
        if (!value)
            throw std::runtime_error(reason);
    }
    struct Event
    {
        HANDLE value = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ~Event() { CloseHandle(value); }
    };
    template <class Fn>
    Fn Export(HMODULE module, const char* name)
    {
        auto fn = reinterpret_cast<Fn>(GetProcAddress(module, name));
        Check(fn != nullptr, name);
        return fn;
    }
    constexpr wchar_t moduleName[] = L"stream-fixture.dll";
    struct Fixture
    {
        HMODULE module = LoadLibraryW(moduleName);
        bool (*begin)(HANDLE) = Export<bool (*)(HANDLE)>(module, "Begin");
        bool (*connect)(const wchar_t*, void*, DWORD, HANDLE, HANDLE) = Export<bool (*)(const wchar_t*, void*, DWORD, HANDLE, HANDLE)>(module, "Connect");
        bool (*consume)(unsigned, void*) = Export<bool (*)(unsigned, void*)>(module, "Consume");
        bool (*retire)(DWORD) = Export<bool (*)(DWORD)>(module, "Retire");
        unsigned (*publications)() = Export<unsigned (*)()>(module, "Publications");
        void Unload()
        {
            Check(FreeLibrary(module), "Loader release failed");
            module = nullptr;
        }
    };
    void Published(Fixture& fixture, const wchar_t* url, Event& ready, unsigned count)
    {
        ResetEvent(ready.value);
        void* id = &count;
        Check(fixture.connect(url, id, 0, nullptr, nullptr) && WaitForSingleObject(ready.value, 5000) == WAIT_OBJECT_0 && fixture.consume(count, id),
              "Fresh session failed to publish real loopback audio");
    }
    template <class Predicate>
    bool Until(Predicate condition)
    {
        auto deadline = GetTickCount64() + 5000;
        do
        {
            if (condition())
                return true;
            Sleep(2);
        } while (GetTickCount64() < deadline);
        return false;
    }
    void Run(const std::wstring& url)
    {
        Event   ready, reached, release;
        Fixture first;
        Check(first.begin(ready.value), "Initial BASS session failed");
        Published(first, url.c_str(), ready, 1);
        auto held = url.substr(0, url.rfind(L'/')) + L"/held";
        Check(first.connect(held.c_str(), nullptr, 0, nullptr, nullptr) && Until([] { return GetFileAttributesW(L"request-held") != INVALID_FILE_ATTRIBUTES; }),
              "Held loopback request did not arrive");
        Check(first.retire(3000) && first.publications() == 1, "Blocked BASS connection did not cancel and retire");
        std::puts("Held loopback request cancelled before next session");

        for (DWORD phase : {1u, 2u})
        {
            ResetEvent(reached.value);
            ResetEvent(release.value);
            Check(first.begin(ready.value), "Could not reopen quiet session");
            unsigned before = first.publications();
            Check(first.connect(url.c_str(), nullptr, phase, reached.value, release.value) && WaitForSingleObject(reached.value, 5000) == WAIT_OBJECT_0,
                  "Worker did not reach test gate");
            auto start = GetTickCount64();
            Check(!first.retire(25) && GetTickCount64() - start < 1500, "Retirement missed its bound or reported a live worker as exited");
            first.Unload();
            Check(GetModuleHandleW(moduleName) != nullptr, "Loader released module while its worker was still executing");
            Fixture retry;
            Check(!retry.begin(ready.value), "Early retry started over a retiring connection");
            SetEvent(release.value);
            Check(Until([&] { return retry.begin(ready.value); }) && retry.publications() == before, "Old job published into the new session");
            Published(retry, url.c_str(), ready, before + 1);
            Check(retry.retire(3000), "New session failed to retire");
            first = retry;
        }
        Check(first.begin(ready.value), "Fresh session for final worker failed");
        ResetEvent(reached.value);
        ResetEvent(release.value);
        Check(first.connect(url.c_str(), nullptr, 2, reached.value, release.value) && WaitForSingleObject(reached.value, 5000) == WAIT_OBJECT_0,
              "Last-reference worker did not connect");
        Check(!first.retire(0), "Last worker unexpectedly retired");
        first.Unload();
        Check(GetModuleHandleW(moduleName) != nullptr, "Pending worker lost last loader reference");
        SetEvent(release.value);
        Check(Until([] { return GetModuleHandleW(moduleName) == nullptr; }), "Last worker retained the DLL after exiting");
        std::puts("Workers held before and after BASS open survived loader release, refused early restart, discarded stale results and permitted retry");
        std::puts("Final worker released its last DLL reference without a loader retry");
        Fixture fresh;
        Check(fresh.publications() == 0 && fresh.begin(ready.value), "Remapped DLL did not start a fresh session");
        Published(fresh, url.c_str(), ready, 1);
        Check(fresh.retire(3000), "Remapped DLL retained a worker");
        fresh.Unload();
        Check(GetModuleHandleW(moduleName) == nullptr, "Remapped DLL did not unload after session shutdown");
        std::puts("Remapped DLL published a fresh loopback channel and unloaded");
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        Check(argc == 2, "Expected a loopback URL");
        Run(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s (Windows error %lu)\n", error.what(), GetLastError());
        return 1;
    }
}
#endif
