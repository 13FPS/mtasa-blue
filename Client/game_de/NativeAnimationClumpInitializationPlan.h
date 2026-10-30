/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One admitted clump/provider/frame/map/sequence input plan
 ****************************************************************************/
#pragma once
#include "NativeStaticAssociationInitializeCalls.h"
#include "NativeStaticAssociationInitializeProfile.h"
#include "NativeCustomAnimSequenceStorageProfile.h"
#include "NativeStreamingCompletionContext.h"
#include "NativePedMetadataMemory.h"
#include "NativeModelLoadedReader.h"
#include "NativeModelBankReader.h"
#include "NativeModelReferenceHeap.h"
#include "NativeAnimationHierarchyAccess.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace mta::game_de
{
    struct NativeStaticAssociationInitializeCoreAccess;
    struct NativeAnimationAssociationClumpOwnershipAccess;
    struct NativeAnimationAssociationStorageAccess;
    namespace NativeAnimationClumpInitializationPlan
    {
        namespace P = NativeStaticAssociationInitializeProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace H = NativeModelReferenceHeap;
        namespace C = NativeStreamingModelContext;
        namespace S = NativeCustomAnimSequenceStorageProfile;
        class Origin final
        {
        public:
            Origin(const Origin&) = delete;
            Origin& operator=(const Origin&) = delete;

        private:
            friend struct ::mta::game_de::NativeStaticAssociationInitializeCoreAccess;
            friend struct ::mta::game_de::NativeAnimationAssociationClumpOwnershipAccess;
            friend struct ::mta::game_de::NativeAnimationAssociationStorageAccess;
            friend struct Access;
            Origin() noexcept = default;
            std::uintptr_t                                     image = 0;
            NativeStaticAssociationInitializeCalls::Arguments  input{};
            NativeStaticAssociationInitializeCalls::CheckOwner owner = nullptr;
            std::size_t                                        receiverBytes = 0;
            bool                                               allowEmptyFrames = false;
            const void*                                        context = nullptr;
            const NativeModelBankReader::Value*                temporaryModel = nullptr;
            ClockResult (*recheck)(const void*) noexcept = nullptr;
            ClockResult (*readHierarchy)(const void*, NativeAnimationHierarchyValues&, std::uintptr_t&) noexcept = nullptr;
        };
        struct BoneRow
        {
            std::int32_t key = 0, frame = 0, next = -1, bucket = -1;
            bool         operator==(const BoneRow& b) const noexcept { return key == b.key && frame == b.frame && next == b.next && bucket == b.bucket; }
        };
        static_assert(sizeof(BoneRow) == 16);
        struct BoneMap
        {
            std::array<unsigned char, P::MapHeaderBytes> header{};
            std::uintptr_t                               rowsAddress = 0, bitsAddress = 0, bucketsAddress = 0;
            std::vector<BoneRow>                         rows;
            std::vector<std::uint32_t>                   bits;
            std::vector<std::int32_t>                    buckets;
        };
        struct Snapshot
        {
            D::Value                       world;
            H::Value                       heap;
            NativePedMetadata              metadata;
            NativeModelBankReader::Value   model;
            NativeModelLoadedReader::Value loaded;
            C::Value                       modelContext;
            NativeAnimationHierarchyValues hierarchy;
            std::array<unsigned char, 6>   typeBytes{};
            std::uint8_t                   modelType = 0;
            std::uintptr_t                 container = 0, cleanup = 0, renderVtable = 0, actorClass = 0, renderClass = 0, classRows = 0, classCell = 0;
            std::uintptr_t                 eventMethod = 0, nameMethod = 0, skeletonMethod = 0, component = 0, skeleton = 0, fallbackControl = 0;
            std::int32_t                   classDepth = -1, renderDepth = -1;
            std::uint16_t                  frameCount = 0;
            std::array<std::uint32_t, P::FrameCapacity>         frameKeys{};
            std::array<std::int32_t, P::FrameCapacity>          nameIndices{};
            std::uintptr_t                                      sequences = 0;
            std::uint64_t                                       sequenceCapacity = 0;
            std::size_t                                         sequenceAllocationBytes = 0;
            std::vector<std::array<unsigned char, S::RowBytes>> sequenceRows;
            BoneMap                                             map;
        };
        bool QualifyProfile(std::uintptr_t) noexcept;
        bool Same(const Snapshot&, const Snapshot&) noexcept;
        bool Same(const BoneMap&, const BoneMap&) noexcept;
        struct Access
        {
            // The caller supplies an actual closed origin in its current
            // native scope. This reader neither enters a gate nor claims or
            // executes an initializer, and grants no receiver ownership.
            static ClockResult CaptureInHeldScope(const Origin&, Snapshot&);
            static ClockResult RecheckInHeldScope(const Origin&, const Snapshot&);
        };
    }
}
