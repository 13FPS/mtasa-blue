/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared body-piece type used by game and ped interfaces
 *****************************************************************************/
#pragma once

// The game interface's hit callback must compile independently of a ped
// backend. These are the existing SDK values, shared by both interfaces.
enum ePedPieceTypes
{
    PED_PIECE_UNKNOWN = 0,
    PED_PIECE_TORSO = 3,
    PED_PIECE_ASS,
    PED_PIECE_LEFT_ARM,
    PED_PIECE_RIGHT_ARM,
    PED_PIECE_LEFT_LEG,
    PED_PIECE_RIGHT_LEG,
    PED_PIECE_HEAD
};
