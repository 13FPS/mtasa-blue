/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: A loadable owned DLL that cannot satisfy launcher dispatch
 *****************************************************************************/

// A real DLL with the wrong export distinguishes failed symbol dispatch from
// Windows rejecting an invalid image. It is never a substitute Core/backend.
extern "C" __declspec(dllexport) unsigned int UnrelatedPreflightFixture() noexcept
{
    return 1;
}
