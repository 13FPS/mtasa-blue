/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Headless status shared by the real launcher and DE preflight loader
 *****************************************************************************/

#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string_view>

namespace DefinitivePreflight
{
    inline constexpr char SteamLaunchUri[] = "steam://run/1547000";
    inline bool           HasArgument(std::wstring_view argument) noexcept
    {
        int  count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments)
            return false;
        bool requested = false;
        for (int index = 1; index < count; ++index)
            requested = requested || std::wstring_view(arguments[index]) == argument;
        LocalFree(arguments);
        return requested;
    }

    inline bool LaunchRequested() noexcept
    {
        return HasArgument(L"--de-launch");
    }
    inline bool InstalledLaunchRequested() noexcept
    {
        return HasArgument(L"--de-launch-installed");
    }
    inline bool Requested() noexcept
    {
        return HasArgument(L"--de-preflight") || LaunchRequested() || InstalledLaunchRequested();
    }

    // The external launcher owns dispatch. Keeping its preparation result value
    // only prevents a validator or a successful shell call from claiming that
    // GTA, Core, rendering or multiplayer has actually started.
    struct HostLaunchResultV1
    {
        std::uint32_t size = sizeof(HostLaunchResultV1);
        std::uint32_t version = 1;
        std::uint32_t verifiedFiles = 0;
        std::uint32_t win32Error = 0;
        char          status[96]{};
    };
    static_assert(sizeof(HostLaunchResultV1) == 112);
    using PrepareHostLaunchV1 = int (*)(HostLaunchResultV1*);

    inline int Status(const char* status, int result, DWORD error = 0, unsigned int verifiedFiles = 0, bool hostLaunchReady = false,
                      bool hostLaunchRequested = false) noexcept
    {
        // A valid offline preflight must never be mistaken for an implemented
        // launch path, loaded Core, or a playable MTA client.
        // Filesystem exceptions may contain user paths. Never interpolate their
        // quotes/backslashes into JSON or disclose them as a status identifier.
        const auto statusLength = std::strlen(status);
        bool       identifier = statusLength > 0 && statusLength <= 80;
        for (std::size_t index = 0; index < statusLength; ++index)
            identifier =
                identifier && ((status[index] >= 'a' && status[index] <= 'z') || status[index] == '_' || (status[index] >= '0' && status[index] <= '9'));
        if (!identifier)
            status = "preflight_exception";
        char      output[512]{};
        const int length = _snprintf_s(output, sizeof(output), _TRUNCATE,
                                       "{\"status\":\"%s\",\"preflightValid\":%s,\"launchReady\":false,\"hostLaunchReady\":%s,"
                                       "\"hostLaunchRequested\":%s,\"fullMtaReady\":false,"
                                       "\"gameLaunched\":false,\"coreLoaded\":false,\"installationWrites\":false,"
                                       "\"verifiedFiles\":%u,\"win32Error\":%lu}\n",
                                       status, result == 0 || hostLaunchReady ? "true" : "false", hostLaunchReady ? "true" : "false",
                                       hostLaunchRequested ? "true" : "false", verifiedFiles, error);
        if (length > 0 && static_cast<std::size_t>(length) < sizeof(output))
        {
            DWORD written = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), output, static_cast<DWORD>(length), &written, nullptr);
        }
        return result;
    }
}
