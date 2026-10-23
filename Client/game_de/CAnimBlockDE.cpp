/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve genuine block scalar widths and refuse unavailable owners
 ****************************************************************************/
#include "CAnimBlockDE.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    CAnimBlockDE::CAnimBlockDE(NativeAnimationBlockAccess& access, const NativeAnimationBlockIdentity& value) : native(access), identity(value)
    {
        if (std::find(identity.name.begin(), identity.name.end(), 0) == identity.name.end())
            throw std::invalid_argument("DE animation block requires a terminated copied16-byte name identity");
    }
    NativeAnimationBlockValues CAnimBlockDE::Read(const char* operation) const
    {
        NativeAnimationBlockValues value;
        const auto                 result = native.ReadAnimationBlock(identity, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE animation block owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
        if (value.index != identity.index || value.index > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()))
            throw std::runtime_error("DE animation block owner returned another or unrepresentable index");
        return value;
    }
    char* CAnimBlockDE::GetName()
    {
        char*      value = nullptr;
        const auto result = native.ReadAnimationBlockName(identity, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE animation block owner refused GetName: " + std::to_string(static_cast<unsigned>(result)));
        if (!value)
            throw std::runtime_error("DE animation block owner returned no native name");
        // Preserve actual SDK mutable pointer intent. Copying into the wrapper
        // would falsely hide native edits and invent a longer-lived name.
        return value;
    }
    int CAnimBlockDE::GetIndex()
    {
        return static_cast<int>(Read("GetIndex").index);
    }
    bool CAnimBlockDE::IsLoaded()
    {
        return Read("IsLoaded").loaded != 0;
    }
    int CAnimBlockDE::GetIDOffset()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetIDOffset").idOffset;
    }
    std::size_t CAnimBlockDE::GetAnimationCount()
    {
        // Classic size_t is32bits. DE's genuine producer also stores DWORD;
        // widening those unsigned bits is different from reading host size_t
        // across the neighboring assoc-group field or signextending a count.
        return static_cast<std::size_t>(Read("GetAnimationCount").animationCount);
    }
}
