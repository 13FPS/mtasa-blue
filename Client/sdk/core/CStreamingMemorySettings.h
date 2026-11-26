/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read the saved streaming memory budget used by Core and Deathmatch
 *****************************************************************************/
#pragma once

#include "CCVarsInterface.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <optional>

namespace StreamingMemorySettings
{
    constexpr std::size_t BytesPerMiB = 1024 * 1024;
    constexpr std::size_t MaxBytes = INT_MAX;  // CLimits accepts signed int bytes.

    struct Budget
    {
        unsigned int mib;
        bool         needsRepair;
    };

    inline Budget Read(CCVarsInterface& vars, unsigned int minMiB, unsigned int maxMiB)
    {
        std::optional<unsigned int> saved;
        std::string                 text;
        if (vars.Exists("streaming_memory") && vars.Get("streaming_memory", text))
        {
            // Match the integer CVAR's base detection, including old hex values,
            // and require a complete, nonnegative number.
            constexpr auto whitespace = " \t\r\n\f\v";
            const auto     first = text.find_first_not_of(whitespace);
            if (first != std::string::npos && text[first] != '-')
            {
                const char* begin = text.c_str() + first;
                char*       end;
                const auto  value = std::strtoull(begin, &end, 0);
                if (end != begin && value <= UINT_MAX && text.find_first_not_of(whitespace, end - text.c_str()) == std::string::npos)
                    saved = static_cast<unsigned int>(value);
            }
        }

        constexpr unsigned int maxProfileMiB = MaxBytes / BytesPerMiB;
        // Before graphics initialization there is no hardware default to save.
        if (!maxMiB)
            return {saved && *saved <= maxProfileMiB ? *saved : 0u, false};
        maxMiB = std::min(maxMiB, maxProfileMiB);
        minMiB = std::clamp(minMiB, 1u, maxMiB);
        const auto mib = std::clamp(saved.value_or(maxMiB), minMiB, maxMiB);
        return {mib, saved != mib};
    }
}
