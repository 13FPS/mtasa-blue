/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Strict animation environment for actual common IFP runtime tests
 ****************************************************************************/
#pragma once
#include "StdInc.h"
#include "CAnimSequenceStorageSA.h"
#include "CAnimBlendHierarchySA.h"
#include <game/CAnimBlock.h>
#include <game/CAnimBlendAssociation.h>
#include <game/CAnimBlendStaticAssociation.h>
#include <game/CAnimBlendAssocGroup.h>

namespace ifp_parser_tests
{
    [[noreturn]] inline void Unexpected()
    {
        std::fputs("Unqualified IFP environment operation or incorrect resource owner\n", stderr);
        std::abort();
    }
    class Manager;
    class Hierarchy final : public CAnimBlendHierarchy
    {
    public:
        Hierarchy(Manager& manager, CAnimBlendHierarchySAInterface& value) noexcept : owner(manager), row(value) {}
        void Initialize() override
        {
            row.pSequences = nullptr;
            row.usNumSequences = 0;
            row.bRunningCompressed = false;
            row.pad = 0;
            row.iAnimBlockID = -1;
            row.fTotalTime = 0;
            row.pLinkPtr = nullptr;
        }
        void                            SetName(const char* name) override { row.uiHashKey = HashString(name); }
        void                            SetSequences(CAnimBlendSequenceSAInterface* value) override;
        void                            SetNumSequences(unsigned short value) override { row.usNumSequences = value; }
        void                            SetRunningCompressed(bool value) override { row.bRunningCompressed = value; }
        void                            SetAnimationBlockID(int value) override { row.iAnimBlockID = value; }
        void                            RemoveAnimSequences() override { Unexpected(); }
        void                            RemoveFromUncompressedCache() override;
        void                            RemoveQuaternionFlips() override { Unexpected(); }
        void                            CalculateTotalTime() override { Unexpected(); }
        float                           GetTotalTime() const override { Unexpected(); }
        CAnimBlendSequenceSAInterface*  GetSequence(DWORD index) override;
        CAnimBlendSequenceSAInterface*  GetSequences() override { return row.pSequences; }
        unsigned short                  GetNumSequences() override { return row.usNumSequences; }
        bool                            IsRunningCompressed() override { return row.bRunningCompressed; }
        bool                            IsCustom() override { return row.iAnimBlockID == -1; }
        int                             GetAnimBlockID() override { return row.iAnimBlockID; }
        CAnimBlendHierarchySAInterface* GetInterface() override { return &row; }
        unsigned int                    GetNameHashKey() override { return row.uiHashKey; }

    private:
        Manager&                        owner;
        CAnimBlendHierarchySAInterface& row;
    };
    class Sequence final : public CAnimBlendSequence
    {
    public:
        Sequence(Manager& manager, CAnimBlendSequenceSAInterface& value) noexcept : owner(manager), row(value) {}
        void Initialize() override;
        void SetName(const char* name) override { row.m_hash = HashString(name); }
        void SetBoneTag(std::int32_t value) override
        {
            if (value != -1)
            {
                row.sFlags |= 0x10;
                row.m_boneId = static_cast<std::uint16_t>(value);
            }
        }
        void                           SetKeyFrames(std::size_t count, bool root, bool compressed, void* frames) override;
        void*                          GetKeyFrame(std::size_t index, std::uint32_t size) override;
        std::uint32_t                  GetHash() override { return row.m_hash; }
        std::uint16_t                  GetBoneTag() override { return row.m_boneId; }
        BYTE*                          GetKeyFrames() override { return row.pKeyFrames; }
        unsigned short                 GetKeyFramesCount() override { return row.sNumKeyFrames; }
        bool                           IsBigChunkForAllSequences() override { return (row.sFlags & 8) != 0; }
        void                           CopySequenceProperties(CAnimBlendSequenceSAInterface* source) override;
        CAnimBlendSequenceSAInterface* GetInterface() override { return &row; }

    private:
        Manager&                       owner;
        CAnimBlendSequenceSAInterface& row;
    };
    class Manager final : public CAnimManager
    {
    public:
        explicit Manager(bool alternate = false) noexcept : alternateLayout(alternate) {}
        std::set<CAnimBlendHierarchySAInterface*>             hierarchies;
        std::set<CAnimBlendSequenceSAInterface*>              detached;
        std::map<CAnimBlendSequenceSAInterface*, std::size_t> arrays;
        std::map<BYTE*, std::size_t>                          frames;
        std::size_t                                           hierarchyAllocations = 0, hierarchyFrees = 0, detachedAllocations = 0, detachedFrees = 0;
        std::size_t                                           arrayAllocations = 0, arrayFrees = 0, frameAttempts = 0, frameAllocations = 0, frameFrees = 0;
        std::size_t                                           copyAttempts = 0, cacheRemovals = 0, attachAttempts = 0;
        std::size_t                                           failFrameAt = 0, failCopyAt = 0, failArrayAt = 0, failAttachAt = 0;
        bool                                                  copyFailureAfterWrite = false, attachFailureAfterWrite = false;
        enum class EventKind
        {
            Lookup,
            Populate,
            Attach,
            FreeFrame,
            FreeArray,
            DeleteHierarchy,
            Cache
        };
        struct Event
        {
            EventKind                      kind;
            CAnimBlendSequenceSAInterface* array;
            std::size_t                    index;
            bool                           attached;
        };
        std::vector<Event>                                              events;
        std::map<CAnimBlendSequenceSAInterface*, std::set<std::size_t>> constructedRows;
        std::size_t arrayAtAttempts = 0, classicAtCalls = 0, opaqueAtCalls = 0, atBeforeAttachment = 0, hierarchyRowRequests = 0,
                    populationsAfterAttachment = 0;
        std::size_t failArrayRowAt = 0, hierarchyAttachAttempts = 0, hierarchyAttachAccepted = 0, failHierarchyAttachAt = 0;
        bool        hierarchyAttachFailureAfterWrite = false, requireConstructedBeforeAttach = false, allCapacityInitialized = true;
        const bool  alternateLayout;
        std::map<CAnimBlendSequenceSAInterface*, BYTE*> alternateAllocations;
        bool                                            Empty() const
        {
            return hierarchies.empty() && detached.empty() && arrays.empty() && frames.empty() && alternateAllocations.empty() && constructedRows.empty();
        }
        ~Manager()
        {
            if (!Empty())
                Unexpected();
        }
        std::size_t Capacity(CAnimBlendSequenceSAInterface* first) const
        {
            const auto found = arrays.find(first);
            if (found == arrays.end())
                Unexpected();
            return found->second;
        }
        std::size_t                    Stride() const noexcept { return alternateLayout ? 80 : sizeof(CAnimBlendSequenceSAInterface); }
        CAnimBlendSequenceSAInterface* Row(CAnimBlendSequenceSAInterface* first, std::size_t index) const
        {
            if (index >= Capacity(first))
                Unexpected();
            return reinterpret_cast<CAnimBlendSequenceSAInterface*>(reinterpret_cast<BYTE*>(first) + index * Stride());
        }
        bool ArrayAttached(CAnimBlendSequenceSAInterface* first) const
        {
            for (auto* hierarchy : hierarchies)
                if (hierarchy->pSequences == first)
                    return true;
            return false;
        }
        void RecordPopulation(CAnimBlendSequenceSAInterface* value)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(value);
            for (const auto& array : arrays)
            {
                const auto begin = reinterpret_cast<std::uintptr_t>(array.first);
                if (address >= begin && address - begin < array.second * Stride() && (address - begin) % Stride() == 0)
                {
                    const auto index = (address - begin) / Stride();
                    const bool attached = ArrayAttached(array.first);
                    constructedRows[array.first].insert(index);
                    events.push_back({EventKind::Populate, array.first, index, attached});
                    if (attached)
                        ++populationsAfterAttachment;
                    return;
                }
            }
        }
        bool Owns(CAnimBlendSequenceSAInterface* row) const
        {
            if (detached.count(row))
                return true;
            const auto address = reinterpret_cast<std::uintptr_t>(row);
            for (const auto& array : arrays)
            {
                const auto begin = reinterpret_cast<std::uintptr_t>(array.first);
                if (address >= begin && address - begin < array.second * Stride() && (address - begin) % Stride() == 0)
                    return true;
            }
            return false;
        }
        std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy() override
        {
            auto* row = new CAnimBlendHierarchySAInterface{};
            hierarchies.insert(row);
            ++hierarchyAllocations;
            return std::make_unique<Hierarchy>(*this, *row);
        }
        std::unique_ptr<CAnimBlendHierarchy> GetCustomAnimBlendHierarchy(CAnimBlendHierarchySAInterface* row) override
        {
            if (!hierarchies.count(row))
                Unexpected();
            return std::make_unique<Hierarchy>(*this, *row);
        }
        std::unique_ptr<CAnimBlendSequence> GetCustomAnimBlendSequence() override
        {
            auto* row = new CAnimBlendSequenceSAInterface{};
            detached.insert(row);
            ++detachedAllocations;
            return std::make_unique<Sequence>(*this, *row);
        }
        std::unique_ptr<CAnimBlendSequence> GetCustomAnimBlendSequence(CAnimBlendSequenceSAInterface* row) override
        {
            if (!Owns(row))
                Unexpected();
            return std::make_unique<Sequence>(*this, *row);
        }
        // Before the hierarchy edge exists, only this exact owned-array
        // provider can identify a row. The classic branch executes real At;
        // the existing alternate layout remains instrumentation, not DE.
        std::unique_ptr<CAnimBlendSequence> GetCustomAnimBlendSequenceAt(CAnimBlendSequenceSAInterface* original, DWORD index) override
        {
            ++arrayAtAttempts;
            const auto found = arrays.find(original);
            if (found == arrays.end() || static_cast<std::size_t>(index) >= found->second)
                return nullptr;
            if (failArrayRowAt && arrayAtAttempts == failArrayRowAt)
                return nullptr;
            CAnimBlendSequenceSAInterface* current = nullptr;
            if (alternateLayout)
            {
                current = Row(original, index);
                ++opaqueAtCalls;
            }
            else
            {
                current = MtaGame::CAnimSequenceStorageSA::At(original, index);
                ++classicAtCalls;
            }
            if (!current || current != Row(original, index))
                Unexpected();
            const bool attached = ArrayAttached(original);
            if (!attached)
                ++atBeforeAttachment;
            events.push_back({EventKind::Lookup, original, index, attached});
            return GetCustomAnimBlendSequence(current);
        }
        void DeleteCustomAnimHierarchyInterface(CAnimBlendHierarchySAInterface* row) override
        {
            if (!row || hierarchies.erase(row) != 1)
                Unexpected();
            ++hierarchyFrees;
            events.push_back({EventKind::DeleteHierarchy, row->pSequences, 0, row->pSequences != nullptr});
            delete row;
        }
        void DeleteCustomAnimSequenceInterface(CAnimBlendSequenceSAInterface* row) override
        {
            if (!row || detached.erase(row) != 1)
                Unexpected();
            ++detachedFrees;
            delete row;
        }
        CAnimBlendSequenceSAInterface* AllocateCustomAnimSequences(std::size_t capacity) override
        {
            if (failArrayAt && arrayAllocations + 1 == failArrayAt)
                throw std::bad_alloc();
            CAnimBlendSequenceSAInterface* row = nullptr;
            if (alternateLayout && capacity)
            {
                // An instrumented opaque layout, not a claimed DE native
                // factory. Real common code must not inspect its80/8 shape.
                auto*      allocation = new BYTE[8 + 80 * capacity];
                const auto cookie = static_cast<std::uint64_t>(capacity);
                std::memcpy(allocation, &cookie, 8);
                row = reinterpret_cast<CAnimBlendSequenceSAInterface*>(allocation + 8);
                for (std::size_t index = 0; index != capacity; ++index)
                {
                    auto* current = ::new (static_cast<void*>(allocation + 8 + 80 * index)) CAnimBlendSequenceSAInterface;
                    MtaGame::CAnimSequenceStorageSA::Initialize(*current);
                }
                alternateAllocations.emplace(row, allocation);
            }
            else
                row = MtaGame::CAnimSequenceStorageSA::Allocate(capacity);
            if (row)
            {
                arrays.emplace(row, capacity);
                ++arrayAllocations;
                for (std::size_t index = 0; index != capacity; ++index)
                {
                    const auto* current = Row(row, index);
                    allCapacityInitialized =
                        allCapacityInitialized && !current->pKeyFrames && !current->sFlags && !current->sNumKeyFrames && current->m_boneId == 0xffff;
                }
            }
            return row;
        }
        void FreeCustomAnimSequences(CAnimBlendSequenceSAInterface* row) override
        {
            if (!row || arrays.erase(row) != 1)
                Unexpected();
            ++arrayFrees;
            constructedRows.erase(row);
            events.push_back({EventKind::FreeArray, row, 0, false});
            if (alternateLayout)
            {
                const auto found = alternateAllocations.find(row);
                if (found == alternateAllocations.end())
                    Unexpected();
                delete[] found->second;
                alternateAllocations.erase(found);
            }
            else
                MtaGame::CAnimSequenceStorageSA::Free(row);
        }
        BYTE* AllocateKeyFramesMemory(std::uint32_t bytes) override
        {
            ++frameAttempts;
            if (failFrameAt && frameAttempts == failFrameAt)
                return nullptr;
            if (bytes > 1024 * 1024)
                throw std::bad_alloc();
            auto* row = new BYTE[static_cast<std::size_t>(bytes) + 16];
            std::memset(row, 0, bytes);
            std::memset(row + bytes, 0xe7, 16);
            frames.emplace(row, bytes);
            ++frameAllocations;
            return row;
        }
        void FreeKeyFramesMemory(void* pointer) override
        {
            auto*      row = static_cast<BYTE*>(pointer);
            const auto found = frames.find(row);
            if (!row || found == frames.end())
                Unexpected();
            for (std::size_t index = 0; index != 16; ++index)
                if (row[found->second + index] != 0xe7)
                    Unexpected();
            frames.erase(found);
            ++frameFrees;
            events.push_back({EventKind::FreeFrame, reinterpret_cast<CAnimBlendSequenceSAInterface*>(row), 0, false});
            delete[] row;
        }
        void RemoveFromUncompressedCache(CAnimBlendHierarchySAInterface* row) override
        {
            if (!hierarchies.count(row))
                Unexpected();
            ++cacheRemovals;
            events.push_back({EventKind::Cache, row->pSequences, 0, row->pSequences != nullptr});
        }
        void RemoveFromUncompressedCache(CAnimBlendHierarchy* row) override
        {
            if (!row)
                Unexpected();
            RemoveFromUncompressedCache(row->GetInterface());
        }
        void ValidateFrameRange(BYTE* pointer, std::size_t offset, std::size_t bytes) const
        {
            const auto address = reinterpret_cast<std::uintptr_t>(pointer);
            for (const auto& allocation : frames)
            {
                const auto begin = reinterpret_cast<std::uintptr_t>(allocation.first);
                if (address >= begin && address - begin <= allocation.second && offset <= allocation.second - (address - begin) &&
                    bytes <= allocation.second - (address - begin) - offset)
                    return;
            }
            Unexpected();
        }
#include "IFPManagerRejectors.generated.inl"
    };
    // Observe the actual publication point rather than treating allocation
    // or an initialized capacity row as completed parser construction.
    inline void Hierarchy::SetSequences(CAnimBlendSequenceSAInterface* value)
    {
        ++owner.hierarchyAttachAttempts;
        const bool fail = owner.failHierarchyAttachAt && owner.hierarchyAttachAttempts == owner.failHierarchyAttachAt;
        if (value && owner.requireConstructedBeforeAttach)
        {
            if (row.usNumSequences > owner.Capacity(value))
                Unexpected();
            for (std::size_t index = 0; index < row.usNumSequences; ++index)
                if (!owner.constructedRows[value].count(index))
                    Unexpected();
        }
        if (fail && !owner.hierarchyAttachFailureAfterWrite)
            throw std::runtime_error("injected hierarchy edge refusal");
        row.pSequences = value;
        if (value)
            ++owner.hierarchyAttachAccepted;
        owner.events.push_back({Manager::EventKind::Attach, value, 0, value != nullptr});
        if (fail)
            throw std::runtime_error("injected accepted hierarchy edge refusal");
    }
    inline void Sequence::Initialize()
    {
        MtaGame::CAnimSequenceStorageSA::Initialize(row);
        owner.RecordPopulation(&row);
    }
    inline CAnimBlendSequenceSAInterface* Hierarchy::GetSequence(DWORD index)
    {
        ++owner.hierarchyRowRequests;
        return owner.Row(row.pSequences, index);
    }
    inline void Hierarchy::RemoveFromUncompressedCache()
    {
        owner.RemoveFromUncompressedCache(&row);
    }
    inline void Sequence::SetKeyFrames(std::size_t count, bool root, bool compressed, void* pointer)
    {
        ++owner.attachAttempts;
        const bool fail = owner.failAttachAt && owner.attachAttempts == owner.failAttachAt;
        if (fail && !owner.attachFailureAfterWrite)
            throw std::runtime_error("injected attachment refusal");
        row.sNumKeyFrames = static_cast<std::uint16_t>(count);
        row.sFlags = static_cast<unsigned short>((row.sFlags & ~7u) | 1u | (root ? 2u : 0u) | (compressed ? 4u : 0u));
        row.pKeyFrames = static_cast<BYTE*>(pointer);
        owner.RecordPopulation(&row);
        if (fail)
            throw std::runtime_error("injected entered attachment refusal");
    }
    inline void* Sequence::GetKeyFrame(std::size_t index, std::uint32_t size)
    {
        if (index >= row.sNumKeyFrames || size == 0 || index > (std::numeric_limits<std::size_t>::max)() / size)
            Unexpected();
        owner.ValidateFrameRange(row.pKeyFrames, index * size, size);
        return row.pKeyFrames + index * size;
    }
    inline void Sequence::CopySequenceProperties(CAnimBlendSequenceSAInterface* source)
    {
        if (!owner.Owns(source))
            Unexpected();
        ++owner.copyAttempts;
        const bool fail = owner.failCopyAt && owner.copyAttempts == owner.failCopyAt;
        if (fail && !owner.copyFailureAfterWrite)
            throw std::runtime_error("injected transfer refusal");
        row = *source;
        owner.RecordPopulation(&row);
        if (fail)
            throw std::runtime_error("injected entered transfer refusal");
    }
}
