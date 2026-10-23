/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve named garage intent and explicit native owner refusal
 ****************************************************************************/
#include "CGarageNamedDE.h"
#include <algorithm>
// The SDK vector header uses shared scalar definitions supplied by the classic
// precompiled header. The DE service also compiles as its own translation unit.
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <cstring>
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

    namespace
    {
        void CopyGarageFloat(float& output, std::uint32_t word) noexcept
        {
            static_assert(sizeof(float) == sizeof(word));
            // These are raw SDK property observations, not calculations. Keep
            // signed zero, NaN payloads and all native binary32 storage bits.
            std::memcpy(&output, &word, sizeof(word));
        }
    }

    NativeGarageGeometry CGarageNamedDE::ReadGeometry(const char* operation) const
    {
        NativeGarageGeometry value;
        // Finish the same-owner observation before assigning any caller output.
        // A refusal must not leave a partially changed position/extent tuple.
        Require(native.ReadNamedGarageGeometry(name, value), operation);
        return value;
    }

    void CGarageNamedDE::GetPosition(CVector& position) const
    {
        const auto value = ReadGeometry("GetPosition");
        CopyGarageFloat(position.fX, value.words[0]);
        CopyGarageFloat(position.fY, value.words[1]);
        CopyGarageFloat(position.fZ, value.words[2]);
    }

    void CGarageNamedDE::GetDirection(float& x1, float& y1, float& x2, float& y2) const
    {
        const auto value = ReadGeometry("GetDirection");
        CopyGarageFloat(x1, value.words[3]);
        CopyGarageFloat(y1, value.words[4]);
        CopyGarageFloat(x2, value.words[5]);
        CopyGarageFloat(y2, value.words[6]);
    }

    void CGarageNamedDE::GetSize(float& height, float& width, float& depth) const
    {
        const auto value = ReadGeometry("GetSize");
        // Actual MTA returns the raw Height field. Native containment proves
        // that field is an upper Z plane; subtracting origin.z changes the SDK.
        CopyGarageFloat(height, value.words[7]);
        CopyGarageFloat(width, value.words[8]);
        CopyGarageFloat(depth, value.words[9]);
    }

    void CGarageNamedDE::GetBoundingBox(float& left, float& right, float& front, float& back) const
    {
        const auto value = ReadGeometry("GetBoundingBox");
        CopyGarageFloat(left, value.words[10]);
        CopyGarageFloat(right, value.words[11]);
        CopyGarageFloat(front, value.words[12]);
        CopyGarageFloat(back, value.words[13]);
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
