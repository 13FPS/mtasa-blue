/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Borrow an admitted owner of native pad-update suppression
 *****************************************************************************/
#pragma once

namespace mta::game_de
{
    // This controls the native producer, not a per-pad disable word or a
    // replacement controller frame. The concrete owner retains qualification,
    // thread admission and restoration obligations for the global update route.
    class NativePadUpdateAccess
    {
    public:
        virtual ~NativePadUpdateAccess() = default;
        virtual bool IsEnabled(bool& output) const noexcept = 0;
        virtual bool SetDisabled(bool disabled) noexcept = 0;
    };
}
