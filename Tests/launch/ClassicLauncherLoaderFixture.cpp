/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Confirm which owned test DLL the classic launcher dispatches
 *****************************************************************************/

#include <Windows.h>
#include <array>
#include <cstring>

#if defined(_M_IX86)
    #pragma comment(linker, "/EXPORT:DoWinMain=_DoWinMain")
#else
    #error This fixture must match the classic x86 launcher.
#endif

namespace
{
    HMODULE fixtureModule = nullptr;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        fixtureModule = module;
    return TRUE;
}

extern "C" int DoWinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
    // Identical fixtures may exist in both candidate directories. Checking the
    // resolved module path proves adjacency wins rather than merely proving
    // that some loadable DLL returned the expected success code.
    std::array<wchar_t, 32768> expected{};
    std::array<wchar_t, 32768> actual{};
    const DWORD expectedLength = GetEnvironmentVariableW(L"MTA_CLASSIC_LAUNCHER_FIXTURE_DLL", expected.data(), static_cast<DWORD>(expected.size()));
    const DWORD actualLength = GetModuleFileNameW(fixtureModule, actual.data(), static_cast<DWORD>(actual.size()));
    if (!expectedLength || expectedLength >= expected.size() || !actualLength || actualLength >= actual.size())
        return 74;
    if (CompareStringOrdinal(expected.data(), -1, actual.data(), -1, TRUE) != CSTR_EQUAL)
        return 74;
    if (!commandLine || std::strcmp(commandLine, "--classic-launcher-fixture") != 0)
        return 75;
    return 73;
}
