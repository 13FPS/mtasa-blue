/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve named garage intent and explicit native owner refusal
 ****************************************************************************/
#include "CGarageNamedDE.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    CGarageNamedDE::CGarageNamedDE(NativeNamedGarageAccess& access, const NativeGarageName& identity) : native(access), name(identity)
    {
        // Never let an unterminated caller buffer turn a bounded name lookup
        // into access to the adjacent type/door bytes. No local casefold or
        // ordinal conversion substitutes for the genuine native named lookup.
        if (std::find(name.begin(), name.end(), 0) == name.end())
            throw std::invalid_argument("DE named garage requires a terminated name of at most seven bytes");
    }

    void CGarageNamedDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE named garage owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }

    BYTE CGarageNamedDE::GetType() const
    {
        std::uint8_t value;
        Require(native.ReadNamedGarageType(name, value), "GetType");
        return value;
    }

    void CGarageNamedDE::SetType(BYTE type)
    {
        // A native refusal may follow an accepted byte write. Keep that effect
        // and let the owning runtime retire its scope; never retry or rewind it.
        Require(native.SetNamedGarageType(name, type), "SetType");
    }

    bool CGarageNamedDE::IsOpen() const
    {
        bool value;
        Require(native.ReadNamedGarageOpen(name, value), "IsOpen");
        return value;
    }

    void CGarageNamedDE::SetOpen(bool open)
    {
        // Native access applies the actual shared MTA door-state policy; this
        // SDK layer dispatches only the caller's bool once to the named owner.
        Require(native.SetNamedGarageOpen(name, open), "SetOpen");
    }
}
