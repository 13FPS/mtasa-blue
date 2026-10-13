/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Recognize the opt-in DE host before classic startup hooks run
 *****************************************************************************/

#pragma once

#include <Windows.h>
#include <array>
#include <string_view>

namespace DefinitiveProcess
{
    inline bool IsCandidate(std::wstring_view executablePath, bool native64) noexcept
    {
        if (!native64 || executablePath.empty() || executablePath.find(L'\0') != std::wstring_view::npos)
            return false;

        const auto                  separator = executablePath.find_last_of(L"/\\");
        const auto                  basename = executablePath.substr(separator == std::wstring_view::npos ? 0 : separator + 1);
        constexpr std::wstring_view expected = L"SanAndreas.exe";
        if (basename.size() != expected.size())
            return false;
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            auto character = basename[index];
            if (character >= L'A' && character <= L'Z')
                character += L'a' - L'A';
            auto reference = expected[index];
            if (reference >= L'A' && reference <= L'Z')
                reference += L'a' - L'A';
            if (character != reference)
                return false;
        }
        return true;
    }

    inline bool IsCurrentCandidate() noexcept
    {
        // DllMain only classifies the host so classic address hooks are skipped.
        // Exact image/hash validation belongs to the adapter outside loader lock;
        // a matching filename alone must never authorize engine access.
        std::array<wchar_t, 32768> path{};
        const DWORD                length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        return length != 0 && length < path.size() && IsCandidate({path.data(), length}, sizeof(void*) == 8);
    }
}
