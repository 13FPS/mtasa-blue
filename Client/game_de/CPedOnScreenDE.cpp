/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Return the accepted native on-screen result without substitution
 *****************************************************************************/
#include "CPedOnScreenDE.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    bool CPedOnScreenDE::IsOnScreen()
    {
        bool       onScreen = false;
        const auto result = nativeOnScreenAccess.ReadPedOnScreen(ReferenceToken(), onScreen);
        // A refused query is not successful off-screen detection. The exact
        // immutable registered ped stays selected, and the native owner alone
        // handles any entered lazy geometry effect without an SDK-side retry.
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped on-screen owner refused IsOnScreen: " + std::to_string(static_cast<unsigned>(result)));
        return onScreen;
    }
}
