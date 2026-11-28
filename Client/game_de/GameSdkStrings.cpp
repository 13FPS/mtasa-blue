/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Link the shared SDK strings used by a constructed DE CGame
 ****************************************************************************/
#include <windows.h>
#include <SharedUtil.Defines.h>
#include <SharedUtil.Legacy.h>
// The shared implementation uses the classic SDK's int-sized string indices.
#pragma warning(push, 3)
#pragma warning(disable : 4267)
#include <SString.hpp>
#pragma warning(pop)
