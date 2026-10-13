/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve the existing probe include while MTA owns snapshot storage
 *****************************************************************************/

#pragma once

// Existing probes share the module implementation instead of retaining their
// own controller storage when the adapter becomes an MTA-owned game module.
#include "../game_de/ControllerSnapshot.h"
