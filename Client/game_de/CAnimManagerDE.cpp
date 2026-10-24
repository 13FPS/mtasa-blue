/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native counter and missing-name facts without fallback
 ****************************************************************************/
#include "CAnimManagerDE.h"
#include "CAnimBlockRequestDE.h"
#include "CKeyGenDE.h"
#include <algorithm>
#include <cstring>
#include <game/CAnimGatewayNames.h>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        NativeAnimationBlockName CopyName(const char* name)
        {
            if (!name)
                throw std::invalid_argument("DE animation block lookup requires a name");
            NativeAnimationBlockName copied{};
            for (std::size_t i = 0; i != copied.size(); ++i)
            {
                copied[i] = static_cast<unsigned char>(name[i]);
                if (!copied[i])
                    return copied;
            }
            throw std::invalid_argument("DE animation block lookup requires a terminated name within16bytes");
        }
        bool SameName(const NativeAnimationBlockName& query, const NativeAnimationBlockName& actual) noexcept
        {
            // The pinned native lookup folds ASCII only; equal high bytes
            // remain valid and a mismatch containing high bytes never folds.
            const auto fold = [](unsigned char value) { return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value; };
            for (std::size_t i = 0; i != query.size(); ++i)
            {
                if (query[i] != actual[i] && ((query[i] | actual[i]) & 0x80 || fold(query[i]) != fold(actual[i])))
                    return false;
                if (!query[i])
                    return true;
            }
            return false;
        }
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE animation manager owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
        }
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, std::uint64_t generation)
        : native(manager), ownerGeneration(generation), blockAccess(&blocks)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&blocks))
            throw std::invalid_argument("DE animation references require one native owner for manager and blocks");
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, CGame& game, std::uint64_t generation)
        : native(manager), ownerGeneration(generation), blockAccess(&blocks), gameProvider(&game)
    {
        // Both polymorphic references must be subobjects of one CPU service
        // owner. A coincidental native image or callback generation cannot
        // make two independent owners interchangeable for a factory product.
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&blocks))
            throw std::invalid_argument("DE animation factory requires one native owner for manager and blocks");
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationSequenceStorageAccess& storage, std::uint64_t generation)
        : native(manager), ownerGeneration(generation), storageAccess(&storage)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&storage))
            throw std::invalid_argument("DE custom animation storage requires one native owner for manager and storage");
    }
    CAnimManagerDE::CAnimManagerDE(NativeAnimationManagerAccess& manager, NativeAnimationBlockAccess& blocks, NativeAnimationSequenceStorageAccess& storage,
                                   CGame& game, std::uint64_t generation)
        : CAnimManagerDE(manager, blocks, game, generation)
    {
        if (dynamic_cast<void*>(&manager) != dynamic_cast<void*>(&storage))
            throw std::invalid_argument("DE custom animation storage requires one native owner for manager and storage");
        storageAccess = &storage;
    }
    CAnimBlendSequenceSAInterface* CAnimManagerDE::AllocateCustomAnimSequences(std::size_t capacity)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "AllocateCustomAnimSequences");
        // Zero capacity still dispatches: only the current admitted runtime
        // may authorize even a null result for this borrowed generation.
        const auto     generation = ownerGeneration;
        std::uintptr_t tag = 0;
        Require(storageAccess->AllocateAnimationSequences(generation, capacity, tag), "AllocateCustomAnimSequences");
        if (capacity && !tag)
            Require(ClockResult::WriteFailure, "AllocateCustomAnimSequences");
        if (!capacity && tag)
            Require(ClockResult::BindingChanged, "AllocateCustomAnimSequences");
        return reinterpret_cast<CAnimBlendSequenceSAInterface*>(tag);
    }
    void CAnimManagerDE::FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* tag)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "FreeCustomAnimSequences");
        // The real controller maps only its currently issued, unfreed first
        // tags to retained records. A raw SDK tag adds no serial/ABA promise,
        // native bank adoption, frame cleanup or destructor-owned retirement.
        const auto generation = ownerGeneration;
        const auto issuedTag = reinterpret_cast<std::uintptr_t>(tag);
        Require(storageAccess->FreeAnimationSequences(generation, issuedTag), "FreeCustomAnimSequences");
    }
    BYTE* CAnimManagerDE::AllocateKeyFramesMemory(std::uint32_t bytes)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "AllocateKeyFramesMemory");
        const auto     generation = ownerGeneration;
        const auto     requestedBytes = bytes;
        std::uintptr_t address = 0;
        Require(storageAccess->AllocateAnimationKeyFrames(generation, requestedBytes, address), "AllocateKeyFramesMemory");
        // Actual MTA callers handle an allocator's null result. The owner
        // admits that fact separately from a binding/read/write refusal;
        // array-specific nonzero/null rules must not replace this contract.
        return reinterpret_cast<BYTE*>(address);
    }
    void CAnimManagerDE::FreeKeyFramesMemory(void* frames)
    {
        if (!storageAccess)
            Require(ClockResult::NotInitialized, "FreeKeyFramesMemory");
        // Null still reaches current-owner admission. The SDK adds neither
        // allocation adoption nor frame ownership to CPU facade destruction.
        const auto generation = ownerGeneration;
        const auto address = reinterpret_cast<std::uintptr_t>(frames);
        Require(storageAccess->FreeAnimationKeyFrames(generation, address), "FreeKeyFramesMemory");
    }
    NativeAnimationBlockIdentity CAnimManagerDE::ReadCounterIdentity(int index, const char* operation) const
    {
        if (!blockAccess)
            Require(ClockResult::NotInitialized, operation);
        static_assert(sizeof(int) == sizeof(std::int32_t));
        const int requestedIndex = index;
        if (requestedIndex < 0)
            Require(ClockResult::InvalidArgument, operation);
        const auto                   generation = ownerGeneration;
        const auto                   ordinal = static_cast<std::uint32_t>(requestedIndex);
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockCounterIdentity(generation, ordinal, identity), operation);
        if (identity.index != ordinal || identity.ownerGeneration != generation)
            Require(ClockResult::BindingChanged, operation);
        // This is raw16 affinity, not a C-string lookup or factory identity.
        // Native counter admission owns current row/count/name coherence.
        return identity;
    }
    void CAnimManagerDE::AddAnimBlockRef(int index)
    {
        const auto identity = ReadCounterIdentity(index, "AddAnimBlockRef");
        Require(blockAccess->AddAnimationBlockCounterRef(identity), "AddAnimBlockRef");
    }
    int CAnimManagerDE::GetNumRefsToAnimBlock(int index)
    {
        const auto    identity = ReadCounterIdentity(index, "GetNumRefsToAnimBlock");
        std::uint16_t bits = 0;
        Require(blockAccess->ReadAnimationBlockCounterRefs(identity, bits), "GetNumRefsToAnimBlock");
        // The addressed manager getter promotes a signed int16 field, unlike
        // CAnimBlock::GetRefs which deliberately exposes raw unsigned16 bits.
        std::int16_t value = 0;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    void CAnimManagerDE::RemoveAnimBlockRefWithoutDelete(int index)
    {
        const auto identity = ReadCounterIdentity(index, "RemoveAnimBlockRefWithoutDelete");
        // Genuine original policy decrements modulo16 even at zero. Cleanup,
        // unload and the distinct RemoveAnimBlockRef remain separate APIs.
        Require(blockAccess->RemoveAnimationBlockRefWithoutDelete(identity), "RemoveAnimBlockRefWithoutDelete");
    }
    void CAnimManagerDE::RequireFactory(const char* operation) const
    {
        if (!blockAccess || !gameProvider)
            Require(ClockResult::NotInitialized, operation);
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::MakeBlock(const NativeAnimationBlockIdentity& identity, const char* operation)
    {
        RequireFactory(operation);
        if (identity.ownerGeneration != ownerGeneration || std::find(identity.name.begin(), identity.name.end(), 0) == identity.name.end())
            Require(ClockResult::BindingChanged, operation);
        // This one allocation owns only the CPU facade. Native block refs,
        // loaded state and the borrowed game remain unchanged on destruction.
        return std::unique_ptr<CAnimBlock>(new CAnimBlockRequestDE(*blockAccess, identity, *gameProvider));
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::GetAnimationBlock(int index)
    {
        RequireFactory("GetAnimationBlock");
        if (index < 0)
            Require(ClockResult::InvalidArgument, "GetAnimationBlock");
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockIdentity(ownerGeneration, static_cast<std::uint32_t>(index), identity), "GetAnimationBlock");
        if (identity.index != static_cast<std::uint32_t>(index))
            Require(ClockResult::BindingChanged, "GetAnimationBlock");
        // The current native row, rather than a name lookup or cached count,
        // supplies the exact identity for this integer factory overload too.
        return MakeBlock(identity, "GetAnimationBlock");
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::GetAnimationBlock(const char* name)
    {
        const auto query = CopyName(name);
        RequireFactory("GetAnimationBlock");
        std::int32_t index = -1;
        Require(native.FindAnimationBlockIndex(ownerGeneration, query, index), "GetAnimationBlock");
        if (index == -1)
            return nullptr;
        if (index < -1)
            Require(ClockResult::BindingChanged, "GetAnimationBlock");
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockIdentity(ownerGeneration, static_cast<std::uint32_t>(index), identity), "GetAnimationBlock");
        if (identity.index != static_cast<std::uint32_t>(index) || !SameName(query, identity.name))
            Require(ClockResult::BindingChanged, "GetAnimationBlock");
        // Preserve the actual selected row's complete mutable16-byte name,
        // not the query's case-fold spelling or a fabricated zero-filled tail.
        return MakeBlock(identity, "GetAnimationBlock");
    }
    std::unique_ptr<CAnimBlock> CAnimManagerDE::GetAnimBlock(CAnimBlockSAInterface* tag)
    {
        // Classic's null clone path never queries a game or native service.
        if (!tag)
            return nullptr;
        RequireFactory("GetAnimBlock");
        NativeAnimationBlockIdentity identity;
        Require(native.ReadAnimationBlockInterfaceIdentity(ownerGeneration, reinterpret_cast<std::uintptr_t>(tag), identity), "GetAnimBlock");
        return MakeBlock(identity, "GetAnimBlock");
    }
    bool CAnimManagerDE::isGateWayAnimationHierarchy(CAnimBlendHierarchySAInterface* hierarchy)
    {
        const auto    generation = ownerGeneration;
        const auto    locator = reinterpret_cast<std::uintptr_t>(hierarchy);
        std::uint32_t actualHash = 0;
        Require(native.ReadAnimationHierarchyHash(generation, locator, actualHash), "isGateWayAnimationHierarchy");
        // Actual MTA compares its authored gateway key with this raw hash.
        // The existing key service is pure CPU policy, not the native name
        // interning helper, and the opaque pointer is never dereferenced here.
        CKeyGenDE keys;
        return keys.GetUppercaseKey(MtaGame::CAnimGatewayNames::GetAnimationName()) == actualHash;
    }
    NativeAnimationManagerCounts CAnimManagerDE::Read(const char* operation) const
    {
        NativeAnimationManagerCounts next;
        Require(native.ReadAnimationManagerCounts(ownerGeneration, next), operation);
        return next;
    }
    int CAnimManagerDE::GetNumAnimations()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetNumAnimations").animations;
    }
    int CAnimManagerDE::GetNumAnimBlocks()
    {
        return Read("GetNumAnimBlocks").blocks;
    }
    // Gateway names belong to MTA itself. A native observation refusal must
    // not turn these authored constants into missing or synthetic data.
    const char* CAnimManagerDE::GetGateWayBlockName() const
    {
        return MtaGame::CAnimGatewayNames::GetBlockName();
    }
    const char* CAnimManagerDE::GetGateWayAnimationName() const
    {
        return MtaGame::CAnimGatewayNames::GetAnimationName();
    }
    int CAnimManagerDE::GetAnimationBlockIndex(const char* name)
    {
        const auto   copied = CopyName(name);
        std::int32_t index = -1;
        Require(native.FindAnimationBlockIndex(ownerGeneration, copied, index), "GetAnimationBlockIndex");
        if (index < -1)
            throw std::runtime_error("DE animation manager owner returned an invalid missing-name sentinel");
        // The genuine named native caller returns -1 for a missing block. Do
        // not turn an owner failure into that valid, observable result.
        return index;
    }
}
