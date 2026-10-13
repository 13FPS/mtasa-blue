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
#include <string_view>

namespace DefinitivePreflight
{
    inline bool Requested() noexcept
    {
        int  count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments)
            return false;
        bool requested = false;
        for (int index = 1; index < count; ++index)
            requested = requested || std::wstring_view(arguments[index]) == L"--de-preflight";
        LocalFree(arguments);
        return requested;
    }

    inline int Status(const char* status, int result, DWORD error = 0, unsigned int verifiedFiles = 0) noexcept
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
                                       "{\"status\":\"%s\",\"preflightValid\":%s,\"launchReady\":false,\"fullMtaReady\":false,"
                                       "\"gameLaunched\":false,\"coreLoaded\":false,\"installationWrites\":false,"
                                       "\"verifiedFiles\":%u,\"win32Error\":%lu}\n",
                                       status, result == 0 ? "true" : "false", verifiedFiles, error);
        if (length > 0 && static_cast<std::size_t>(length) < sizeof(output))
        {
            DWORD written = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), output, static_cast<DWORD>(length), &written, nullptr);
        }
        return result;
    }
}
