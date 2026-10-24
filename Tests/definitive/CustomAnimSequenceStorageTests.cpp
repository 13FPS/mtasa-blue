/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute actual backend storage policy and shared SDK CPU lifetime
 ****************************************************************************/
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <game/CAnimManager.h>
#include "CAnimSequenceStorageSA.h"
#include "CAnimBlendHierarchySA.h"

namespace
{
    namespace Storage = MtaGame::CAnimSequenceStorageSA;
    unsigned checks = 0;
    void     Check(bool value)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Custom sequence storage check %u failed\n", checks);
            std::abort();
        }
    }
    [[noreturn]] void Unsupported()
    {
        std::fputs("Unqualified hierarchy/sequence fixture operation invoked\n", stderr);
        std::abort();
    }
    class HierarchyLifetime final : public CAnimBlendHierarchy
    {
    public:
        explicit HierarchyLifetime(unsigned& count) noexcept : destructions(count) {}
        ~HierarchyLifetime() override { ++destructions; }
        void                            Initialize() override { Unsupported(); }
        void                            SetName(const char*) override { Unsupported(); }
        void                            SetSequences(CAnimBlendSequenceSAInterface*) override { Unsupported(); }
        void                            SetNumSequences(unsigned short) override { Unsupported(); }
        void                            SetRunningCompressed(bool) override { Unsupported(); }
        void                            SetAnimationBlockID(int) override { Unsupported(); }
        void                            RemoveAnimSequences() override { Unsupported(); }
        void                            RemoveFromUncompressedCache() override { Unsupported(); }
        void                            RemoveQuaternionFlips() override { Unsupported(); }
        void                            CalculateTotalTime() override { Unsupported(); }
        float                           GetTotalTime() const override { throw std::runtime_error("borrow refused"); }
        CAnimBlendSequenceSAInterface*  GetSequence(DWORD) override { Unsupported(); }
        CAnimBlendSequenceSAInterface*  GetSequences() override { Unsupported(); }
        unsigned short                  GetNumSequences() override { Unsupported(); }
        bool                            IsRunningCompressed() override { Unsupported(); }
        bool                            IsCustom() override { Unsupported(); }
        int                             GetAnimBlockID() override { Unsupported(); }
        CAnimBlendHierarchySAInterface* GetInterface() override { Unsupported(); }
        unsigned int                    GetNameHashKey() override { Unsupported(); }

    private:
        unsigned& destructions;
    };
    class SequenceLifetime final : public CAnimBlendSequence
    {
    public:
        explicit SequenceLifetime(unsigned& count) noexcept : destructions(count) {}
        ~SequenceLifetime() override { ++destructions; }
        void                           Initialize() override { Unsupported(); }
        void                           SetName(const char*) override { Unsupported(); }
        void                           SetBoneTag(std::int32_t) override { Unsupported(); }
        void                           SetKeyFrames(std::size_t, bool, bool, void*) override { Unsupported(); }
        void*                          GetKeyFrame(std::size_t, std::uint32_t) override { Unsupported(); }
        std::uint32_t                  GetHash() override { Unsupported(); }
        std::uint16_t                  GetBoneTag() override { Unsupported(); }
        BYTE*                          GetKeyFrames() override { Unsupported(); }
        unsigned short                 GetKeyFramesCount() override { Unsupported(); }
        bool                           IsBigChunkForAllSequences() override { Unsupported(); }
        void                           CopySequenceProperties(CAnimBlendSequenceSAInterface*) override { Unsupported(); }
        CAnimBlendSequenceSAInterface* GetInterface() override { Unsupported(); }

    private:
        unsigned& destructions;
    };
    std::size_t Cookie(const Storage::Sequence* sequences)
    {
        std::size_t count = 0;
        std::memcpy(&count, reinterpret_cast<const unsigned char*>(sequences) - Storage::CookieBytes, sizeof(count));
        return count;
    }
    void StorageChecks()
    {
        Check(Storage::Allocate(0) == nullptr);
        Storage::Free(nullptr);
        // Reuse actual SA declarations and its production initialization,
        // including untouched high union bytes, rather than a mirror record.
        Storage::Sequence   known;
        const std::uint32_t original = 0xa5b61234;
        std::memcpy(&known, &original, sizeof(original));
        known.sFlags = 0xffff;
        known.sNumKeyFrames = 17;
        known.pKeyFrames = reinterpret_cast<BYTE*>(static_cast<std::uintptr_t>(1));
        Storage::Initialize(known);
        std::uint32_t preserved = 0;
        std::memcpy(&preserved, &known, sizeof(preserved));
        Check(preserved == 0xa5b6ffff);
        Check(known.m_boneId == 0xffff && !known.sFlags && !known.sNumKeyFrames && !known.pKeyFrames);
        for (const std::size_t capacity : {std::size_t{1}, std::size_t{2}, std::size_t{64}, std::size_t{65536}})
        {
            auto* sequences = Storage::Allocate(capacity);
            Check(sequences != nullptr);
            Check(reinterpret_cast<std::uintptr_t>(sequences) % alignof(Storage::Sequence) == 0);
            Check(Cookie(sequences) == capacity);
            bool initialized = true;
            for (std::size_t index = 0; index != capacity; ++index)
                initialized = initialized && sequences[index].m_boneId == 0xffff && sequences[index].sFlags == 0 && sequences[index].sNumKeyFrames == 0 &&
                              sequences[index].pKeyFrames == nullptr;
            Check(initialized);
            CAnimBlendHierarchySAInterface hierarchy{};
            hierarchy.pSequences = sequences;
            hierarchy.usNumSequences = 1;
            Check(Cookie(hierarchy.pSequences) == capacity);
            Check(reinterpret_cast<unsigned char*>(sequences + capacity) - reinterpret_cast<unsigned char*>(sequences) ==
                  static_cast<std::ptrdiff_t>(capacity * sizeof(Storage::Sequence)));
            // Storage free must not interpret flags, count or frame pointers.
            // This separate live frame owner remains valid after array free.
            auto frames = std::make_unique<BYTE[]>(32);
            frames[0] = 0x6d;
            frames[31] = 0x2a;
            sequences[0].sFlags = 8;
            sequences[0].sNumKeyFrames = 1;
            sequences[0].pKeyFrames = frames.get();
            Storage::Free(sequences);
            Check(frames[0] == 0x6d && frames[31] == 0x2a);
        }
        const auto maximum = (std::numeric_limits<std::size_t>::max)();
        for (const auto capacity : {(maximum - Storage::CookieBytes) / sizeof(Storage::Sequence) + 1, maximum})
        {
            bool refused = false;
            try
            {
                auto* unexpected = Storage::Allocate(capacity);
                Storage::Free(unexpected);
            }
            catch (const std::bad_array_new_length&)
            {
                refused = true;
            }
            Check(refused);
        }
        std::puts("Actual classic backend sequence storage policy fixture completed");
    }
    void LifetimeChecks()
    {
        unsigned                             hierarchyDestructions = 0, sequenceDestructions = 0;
        std::unique_ptr<CAnimBlendHierarchy> hierarchy = std::make_unique<HierarchyLifetime>(hierarchyDestructions);
        std::unique_ptr<CAnimBlendSequence>  sequence = std::make_unique<SequenceLifetime>(sequenceDestructions);
        bool                                 refused = false;
        try
        {
            static_cast<void>(hierarchy->GetTotalTime());
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused);
        hierarchy.reset();
        sequence.reset();
        Check(hierarchyDestructions == 1 && sequenceDestructions == 1);
        std::puts("Actual hierarchy and sequence SDK polymorphic CPU lifetime fixture completed");
    }
}
static_assert(std::has_virtual_destructor_v<CAnimBlendHierarchy> && std::has_virtual_destructor_v<CAnimBlendSequence>);
static_assert(!noexcept(std::declval<const CAnimBlendHierarchy&>().GetTotalTime()));
static_assert(noexcept(std::declval<const CAnimBlendHierarchySA&>().GetTotalTime()));
static_assert(std::is_same_v<decltype(&CAnimManager::AllocateCustomAnimSequences), CAnimBlendSequenceSAInterface* (CAnimManager::*)(std::size_t)>);
static_assert(std::is_same_v<decltype(&CAnimManager::FreeCustomAnimSequences), void (CAnimManager::*)(CAnimBlendSequenceSAInterface*)>);
int main()
{
    StorageChecks();
    LifetimeChecks();
    std::printf("Passed %u custom animation storage/lifetime checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
}
