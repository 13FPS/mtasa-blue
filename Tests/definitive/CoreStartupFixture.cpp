/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/

// A controlled DLL supplies loader-lock delay and thread-affinity observations.
// It contains no MTA/Game DE implementation and never touches game memory.
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include "../../Client/core/DefinitiveInputReference.h"

namespace
{
    HANDLE                            mapping = nullptr;
    volatile LONG*                    counters = nullptr;
    bool                              mtaSet = false, gtaSet = false;
    DefinitiveInputReference::Request inputRequest;

    DWORD Setting(const wchar_t* name)
    {
        wchar_t text[256]{};
        GetEnvironmentVariableW(name, text, 256);
        return static_cast<DWORD>(std::wcstoul(text, nullptr, 10));
    }
    void Call(unsigned counter)
    {
        if (!counters)
            return;
        if (static_cast<DWORD>(counters[0]) != GetCurrentThreadId())
            InterlockedIncrement(counters + 2);
        InterlockedIncrement(counters + counter);
    }
    HANDLE Event(const wchar_t* variable, DWORD access)
    {
        wchar_t name[256]{};
        if (!GetEnvironmentVariableW(variable, name, 256))
            return nullptr;
        return OpenEventW(access, FALSE, name);
    }
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEInputReferenceApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_INPUT_MODE") == 3 ? 2u : 1u;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_ConfigureDEInputReference(std::uint32_t duration, DefinitiveInputReference::CancelCallback cancelled,
                                                                             DefinitiveInputReference::PublishCallback publish)
{
    Call(8);
    if (duration != DefinitiveInputReference::MaximumDurationMs || !cancelled || !publish)
        return 0;
    inputRequest = {duration, cancelled, publish};
    return 1;
}

extern "C" __declspec(dllexport) std::uint32_t MTA_DEStartupApiVersion()
{
    Call(7);
    return Setting(L"MTA_CORE_FIXTURE_BAD_VERSION") ? 2u : 1u;
}
#ifndef CORE_FIXTURE_MISSING_EXPORT
extern "C" __declspec(dllexport) std::uint32_t MTA_ReadDEStartupState(std::uint32_t field)
{
    Call(6);
    const std::array<std::uint32_t, 8> values{1, 5, 6, Setting(L"MTA_CORE_FIXTURE_BAD_STATE") ? 33u : 97u, 0, 0, 0, 1};
    return field < values.size() ? values[field] : 0xffffffffu;
}
#endif
extern "C" __declspec(dllexport) void SetMTADirectory(const wchar_t* directory, std::size_t size)
{
    Call(4);
    mtaSet = directory && size && directory[size] == L'\0';
}
extern "C" __declspec(dllexport) void SetGTADirectory(const wchar_t* directory, std::size_t size)
{
    Call(5);
    gtaSet = directory && size && directory[size] == L'\0';
}
extern "C" __declspec(dllexport) int InitializeCore()
{
    Call(3);
    if (Setting(L"MTA_CORE_FIXTURE_BAD_INITIALIZE"))
        return 0;
    if (mtaSet && gtaSet && inputRequest.publish)
    {
        using namespace DefinitiveInputReference;
        Snapshot sample{};
        sample[Version] = 1;
        sample[Phase] = 1;
        sample[Sequence] = 1;
        sample[Flags] = 7;
        sample[SupportedBindings] = sample[EligibleKeys] = 5;
        sample[Rearms] = sample[SampleCount] = 1;
        sample[HeldActionsLow] = 1u << 4;
        sample[Current + 1] = 0xffffff80u;
        if (Setting(L"MTA_CORE_FIXTURE_INPUT_MODE") != 2)
        {
            Call(9);
            inputRequest.publish(sample.data(), FieldCount);
            const auto release = Event(L"MTA_CORE_FIXTURE_INPUT_RELEASE", SYNCHRONIZE);
            if (release)
            {
                while (WaitForSingleObject(release, 5) == WAIT_TIMEOUT && !inputRequest.cancelled())
                    Call(10);
                CloseHandle(release);
            }
        }
        const bool cancelled = inputRequest.cancelled();
        sample[Phase] = sample[Sequence] = 2;
        sample[ElapsedMs] = cancelled ? 50u : MaximumDurationMs;
        sample[Flags] = 4;
        sample[HeldActionsLow] = 0;
        sample[Reason] = cancelled ? 2u : 1u;
        sample[InputCleanupComplete] = Setting(L"MTA_CORE_FIXTURE_INPUT_MODE") == 2 ? 0u : 1u;
        sample[Current + 1] = 0;
        Call(9);
        inputRequest.publish(sample.data(), FieldCount);
    }
    return !mtaSet ? 3 : !gtaSet ? 4 : 5;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        wchar_t name[256]{};
        if (!GetEnvironmentVariableW(L"MTA_CORE_FIXTURE_MAP", name, 256))
            return FALSE;
        mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (!mapping)
            return FALSE;
        counters = static_cast<volatile LONG*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 64 * sizeof(LONG)));
        if (!counters)
        {
            CloseHandle(mapping);
            mapping = nullptr;
            return FALSE;
        }
        InterlockedExchange(counters, static_cast<LONG>(GetCurrentThreadId()));
        const auto entered = Event(L"MTA_CORE_FIXTURE_ENTERED", EVENT_MODIFY_STATE);
        const auto release = Event(L"MTA_CORE_FIXTURE_RELEASE", SYNCHRONIZE);
        if (entered)
        {
            SetEvent(entered);
            CloseHandle(entered);
        }
        // Holding DllMain deliberately exposes any hidden loader API on Poll or
        // Cancel. The host releases this gate only after responsive calls pass.
        Sleep(Setting(L"MTA_CORE_FIXTURE_DELAY_MS"));
        if (release)
        {
            WaitForSingleObject(release, 10000);
            CloseHandle(release);
        }
        if (Setting(L"MTA_CORE_FIXTURE_FAIL_ATTACH"))
            return FALSE;
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (counters)
        {
            InterlockedExchange(counters + 1, static_cast<LONG>(GetCurrentThreadId()));
            UnmapViewOfFile(const_cast<LONG*>(counters));
            counters = nullptr;
        }
        if (mapping)
        {
            CloseHandle(mapping);
            mapping = nullptr;
        }
    }
    return TRUE;
}
