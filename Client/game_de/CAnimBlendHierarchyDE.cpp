/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve hierarchy scalar bits and current nested-pointer admission
 ****************************************************************************/
#include "CAnimBlendHierarchyDE.h"
#include "NativeClassicAnimationNameHash.h"
#include <cstring>
#include <stdexcept>
#include <string>

namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE animation hierarchy owner refused ") + operation + ": " +
                                         std::to_string(static_cast<unsigned>(result)));
        }
    }
    void CAnimBlendHierarchyDE::Initialize()
    {
        // Only the owner knows whether this is an issued, empty private
        // record. A borrowed bank hierarchy cannot be reset through this API.
        const auto token = reference;
        Require(native.InitializePrivateAnimationHierarchy(token), "Initialize");
    }
    void CAnimBlendHierarchyDE::SetRunningCompressed(bool compressed)
    {
        // Preserve the actual MTA bool assignment; the current private owner
        // qualifies the record and preserves every unrelated native field.
        const auto token = reference;
        const bool value = compressed;
        Require(native.SetPrivateAnimationHierarchyRunningCompressed(token, value), "SetRunningCompressed");
    }
    void CAnimBlendHierarchyDE::SetAnimationBlockID(int blockId)
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        const auto token = reference;
        const auto value = static_cast<std::int32_t>(blockId);
        Require(native.SetPrivateAnimationHierarchyBlockID(token, value), "SetAnimationBlockID");
    }
    void CAnimBlendHierarchyDE::SetName(const char* name)
    {
        const auto token = reference;
        if (!name)
            throw std::invalid_argument("DE animation hierarchy SetName requires a nonnull name");
        // Addressed classic 4CF2D0 assigns only the uppercase source key. Hash
        // the entire NUL string before callback; hierarchy names have no
        // sequence row's 64-byte bound and perform no native name/cache work.
        const auto hash = NativeClassicAnimationNamePolicy::Hash(name);
        Require(native.SetPrivateAnimationHierarchyNameHash(token, hash), "SetName");
    }
    void CAnimBlendHierarchyDE::SetSequences(CAnimBlendSequenceSAInterface* sequences)
    {
        const auto token = reference;
        const auto locator = reinterpret_cast<std::uintptr_t>(sequences);
        // The legacy tag is same-backend opaque input. Only its owner may
        // admit prepared storage or acknowledge null as a safe private detach.
        Require(native.SetPrivateAnimationHierarchySequences(token, locator), "SetSequences");
    }
    void CAnimBlendHierarchyDE::SetNumSequences(unsigned short count)
    {
        static_assert(sizeof(count) == sizeof(std::uint16_t));
        const auto token = reference;
        const auto value = static_cast<std::uint16_t>(count);
        // Actual MTA assigns the complete WORD; capacity is a separate owner
        // concern and cannot silently clamp this caller's source value.
        Require(native.SetPrivateAnimationHierarchySequenceCount(token, value), "SetNumSequences");
    }
    NativeAnimationHierarchyValues CAnimBlendHierarchyDE::Read(const char* operation) const
    {
        const auto                     token = reference;
        NativeAnimationHierarchyValues value;
        Require(native.ReadAnimationHierarchy(token, value), operation);
        return value;
    }
    float CAnimBlendHierarchyDE::GetTotalTime() const
    {
        // This is a raw float field, not an opportunity to sanitize a native
        // NaN, negative zero or infinity, or hide a stale owner with zero.
        const auto bits = Read("GetTotalTime").totalTimeBits;
        float      value;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    CAnimBlendSequenceSAInterface* CAnimBlendHierarchyDE::GetSequence(DWORD index)
    {
        const auto token = reference;
        static_assert(sizeof(index) == sizeof(std::uint32_t));
        const auto     requested = static_cast<std::uint32_t>(index);
        std::uintptr_t value = 0;
        Require(native.ReadAnimationHierarchySequence(token, requested, value), "GetSequence");
        // Only the owner can admit a current full-cookie capacity. The SDK
        // does not replace it with the separate published WORD count.
        return reinterpret_cast<CAnimBlendSequenceSAInterface*>(value);
    }
    CAnimBlendSequenceSAInterface* CAnimBlendHierarchyDE::GetSequences()
    {
        const auto     token = reference;
        std::uintptr_t value = 0;
        Require(native.ReadAnimationHierarchySequences(token, value), "GetSequences");
        return reinterpret_cast<CAnimBlendSequenceSAInterface*>(value);
    }
    unsigned short CAnimBlendHierarchyDE::GetNumSequences()
    {
        static_assert(sizeof(unsigned short) == sizeof(std::uint16_t));
        return Read("GetNumSequences").sequenceCount;
    }
    bool CAnimBlendHierarchyDE::IsRunningCompressed()
    {
        return Read("IsRunningCompressed").runningCompressed != 0;
    }
    bool CAnimBlendHierarchyDE::IsCustom()
    {
        // Actual SA reads the native block ID here. Even a bank-only facade
        // must qualify that read rather than cache its construction kind.
        return Read("IsCustom").blockId == -1;
    }
    int CAnimBlendHierarchyDE::GetAnimBlockID()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetAnimBlockID").blockId;
    }
    CAnimBlendHierarchySAInterface* CAnimBlendHierarchyDE::GetInterface()
    {
        // The returned tag is a same-backend immediate borrow. No classic
        // structure layout or native lifetime is attached to the C++ cast.
        return reinterpret_cast<CAnimBlendHierarchySAInterface*>(Read("GetInterface").interfaceAddress);
    }
    unsigned int CAnimBlendHierarchyDE::GetNameHashKey()
    {
        static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
        const auto value = Read("GetNameHashKey");
        // Original Initialize preserves the hash. An unassigned private hash
        // cannot become a fabricated zero, while unrelated fields remain usable.
        Require(value.hashDefined ? ClockResult::Ok : ClockResult::NotInitialized, "GetNameHashKey");
        return value.hash;
    }
}
