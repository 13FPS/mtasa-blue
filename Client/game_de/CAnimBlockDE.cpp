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
    CAnimBlockSAInterface* CAnimBlockDE::GetInterface()
    {
        const auto expected = identity;
        char*      address = nullptr;
        const auto result = native.ReadAnimationBlockName(expected, address);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE animation block owner refused GetInterface: " + std::to_string(static_cast<unsigned>(result)));
        if (!address)
            throw std::runtime_error("DE animation block owner returned no native interface");
        // The common SDK only roundtrips this legacy tag to the same backend.
        // GetName already qualifies this actual rowbase. The cast neither
        // applies classic structure layout nor extends the current borrow.
        return reinterpret_cast<CAnimBlockSAInterface*>(address);
    }
    CAnimBlendHierarchySAInterface* CAnimBlockDE::GetAnimationHierarchyInterface(std::size_t animation)
    {
        const auto     expected = identity;
        std::uintptr_t address = 0;
        const auto     result = native.ReadAnimationBlockHierarchy(expected, animation, address);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE animation block owner refused GetAnimationHierarchyInterface: " + std::to_string(static_cast<unsigned>(result)));
        // The native owner preserves unloaded/null before range checks and
        // qualifies DE's own40-byte row. No classic sizeof or fields are read.
        return address ? reinterpret_cast<CAnimBlendHierarchySAInterface*>(address) : nullptr;
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
    void CAnimBlockDE::AddRef()
    {
        // Actual CAnimBlockSA increments its ushort directly. The native
        // owner owns the exact persistent WORD effect, including wrap; the
        // wrapper adds no loaded check, CPU ref cache, retry or inverse write.
        const auto expected = identity;
        const auto result = native.AddAnimationBlockRef(expected);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE animation block owner refused AddRef: " + std::to_string(static_cast<unsigned>(result)));
    }
    unsigned short CAnimBlockDE::GetRefs()
    {
        const auto    expected = identity;
        std::uint16_t value;
        const auto    result = native.ReadAnimationBlockRefs(expected, value);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE animation block owner refused GetRefs: " + std::to_string(static_cast<unsigned>(result)));
        static_assert(sizeof(unsigned short) == sizeof(value));
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
