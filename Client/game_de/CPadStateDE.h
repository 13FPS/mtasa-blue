/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the real CPad SDK for the qualified native controller states
 *****************************************************************************/
#pragma once

#include <windows.h>
#include <game/CPad.h>
#include <type_traits>

namespace mta::game_de
{
    class NativePadStateAccess;

    // Only the two verified controller blocks are implemented here. The SDK's
    // whole-pad restore, updater, horn and rumble methods stay abstract until
    // their actual engine behavior and cleanup ownership are qualified.
    class CPadStateDE : public CPad
    {
    public:
        CControllerState* GetCurrentControllerState(CControllerState* output) override;
        CControllerState* GetLastControllerState(CControllerState* output) override;
        void              SetCurrentControllerState(CControllerState* input) override;
        void              SetLastControllerState(CControllerState* input) override;
        void              Clear() override;

        CPadStateDE(const CPadStateDE&) = delete;
        CPadStateDE& operator=(const CPadStateDE&) = delete;

    protected:
        // This borrows access inside its existing synchronous qualified engine
        // scope; it never creates actor/thread/generation authority of its own.
        explicit CPadStateDE(NativePadStateAccess& access) noexcept : states(access) {}

    private:
        NativePadStateAccess& states;
    };

    static_assert(std::is_abstract_v<CPadStateDE> && std::is_base_of_v<CPad, CPadStateDE>);
}
