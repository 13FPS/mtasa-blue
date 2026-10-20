/*****************************************************************************
 * PROJECT: Multi Theft Auto - experimental Definitive Edition client tests
 * LICENSE: See LICENSE in the repository root
 *****************************************************************************/
#pragma once

#include <windows.h>

namespace NativeFrontendASIFixture
{
    inline constexpr wchar_t MapVariable[] = L"MTA_ASI_FIXTURE_MAP";
    inline constexpr wchar_t EnteredVariable[] = L"MTA_ASI_FIXTURE_ENTERED";
    inline constexpr wchar_t CompletedVariable[] = L"MTA_ASI_FIXTURE_COMPLETED";
    enum Field : unsigned
    {
        Calls,
        RequestSize,
        RequestVersion,
        RequestFlags,
        RequestReserved,
        CallerThread,
        Complete,
        InvalidArgument,
        ElapsedMs,
        FieldCount
    };
}
