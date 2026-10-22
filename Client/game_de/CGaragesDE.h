/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse actual MTA garage initialization through the existing SDK
 *****************************************************************************/
#pragma once
#include <windows.h>
#include <game/CGarages.h>
#include <type_traits>

namespace mta::game_de
{
    // The future native service must provide genuine stable garage wrappers
    // and their SDK-ID mapping. Sharing Initialize does not supply that factory.
    class CGaragesDE : public CGarages
    {
    public:
        void Initialize() override;

    protected:
        CGaragesDE() = default;
    };
    static_assert(std::is_abstract_v<CGaragesDE> && std::is_base_of_v<CGarages, CGaragesDE>);
}
