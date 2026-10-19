/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared entity type used by game and entity interfaces
 *****************************************************************************/
#pragma once

// CGame needs this type without pulling in an entity backend. Keeping its
// original definition here avoids nonstandard opaque enum declarations while
// preserving the existing SDK's type name, values and native representation.
enum eEntityType
{
    ENTITY_TYPE_NOTHING,
    ENTITY_TYPE_BUILDING,
    ENTITY_TYPE_VEHICLE,
    ENTITY_TYPE_PED,
    ENTITY_TYPE_OBJECT,
    ENTITY_TYPE_DUMMY,
    ENTITY_TYPE_NOTINPOOLS
};
