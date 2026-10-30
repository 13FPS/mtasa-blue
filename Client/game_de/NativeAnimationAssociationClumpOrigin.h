/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Synchronous closed clump origins for actual issued dynamic receivers
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationAccess.h"
#include "NativeAnimationAssociationClumpTypes.h"
#include "NativeStaticAssociationInitializeCalls.h"
#include "NativeModelBankReader.h"
#include "NativePedReferenceToken.h"
namespace mta::game_de
{
    class NativeAnimationAssociationController;
    class NativePrivateAnimationHierarchyController;
    class NativePedReferenceController;
    class NativePedReferenceMemory;
    class NativeTemporaryModelClumpBorrow;
    struct NativeAnimationAssociationClumpOwnershipAccess;
    class NativeAnimationAssociationClumpOrigin final
    {
    public:
        NativeAnimationAssociationClumpOrigin(const NativeAnimationAssociationClumpOrigin&) = delete;
        NativeAnimationAssociationClumpOrigin&                   operator=(const NativeAnimationAssociationClumpOrigin&) = delete;
        std::uintptr_t                                           Image() const noexcept { return image; }
        std::uint64_t                                            Generation() const noexcept { return generation; }
        const NativeAnimationAssociationReferenceToken&          Receiver() const noexcept { return receiver; }
        const NativeStaticAssociationInitializeCalls::Arguments& Input() const noexcept { return input; }
        const NativeAnimationHierarchyReferenceToken&            Hierarchy() const noexcept { return hierarchy; }
        NativePrivateAnimationHierarchyController&               Hierarchies() const noexcept { return *hierarchies; }
        bool                                                     IsPrivateHierarchy() const noexcept { return privateHierarchy; }
        bool                                                     IsTemporary() const noexcept { return temporary != nullptr; }
        bool                                                     HasHeldGate() const noexcept { return IsTemporary(); }
        const NativeModelBankReader::Value*                      TemporaryModel() const noexcept { return IsTemporary() ? &model : nullptr; }
        ClockResult                                              Recheck() const noexcept;
        ClockResult                                              Claim() const noexcept;
        void                                                     Entered() const noexcept;

    private:
        friend struct NativeAnimationAssociationClumpOwnershipAccess;
        // Only the two actual owner routes can construct this stack object.
        // A raw SDK clump/H or observation receipt cannot forge a held scope.
        NativeAnimationAssociationClumpOrigin() noexcept = default;
        std::uintptr_t                                    image = 0;
        std::uint64_t                                     generation = 0;
        NativeAnimationAssociationReferenceToken          receiver{};
        NativeStaticAssociationInitializeCalls::Arguments input{};
        NativeAnimationHierarchyReferenceToken            hierarchy{};
        NativePrivateAnimationHierarchyController*        hierarchies = nullptr;
        NativePedReferenceController*                     references = nullptr;
        const NativePedReferenceMemory*                   referenceMemory = nullptr;
        NativePedReferenceToken                           ped{};
        const NativeTemporaryModelClumpBorrow*            temporary = nullptr;
        NativeModelBankReader::Value                      model{};
        ClockResult (*owner)() noexcept = nullptr;
        bool         privateHierarchy = false;
        mutable bool claimed = false, entered = false;
    };
    struct NativeAnimationAssociationClumpOwnershipAccess
    {
        // Registered clumps must match an actually published ped reference.
        // Temporary clumps consume the existing noncopyable active borrow.
        static ClockResult ApplyRegistered(std::uintptr_t, NativeAnimationAssociationController&, NativePrivateAnimationHierarchyController&,
                                           NativePedReferenceController&, const NativePedReferenceMemory&, std::uint64_t,
                                           const NativeAnimationAssociationReferenceToken&, NativeAnimationAssociationClumpOperation, std::uintptr_t,
                                           std::uintptr_t, ClockResult (*)() noexcept, std::uintptr_t&,
                                           NativeAnimationAssociationCalls::ClumpReceipt* = nullptr,
                                           const NativeAnimationAssociationCalls::ClumpInvocations* = nullptr) noexcept;
        static ClockResult ApplyTemporary(std::uintptr_t, NativeAnimationAssociationController&, std::uint64_t, const NativeAnimationAssociationReferenceToken&,
                                          NativeAnimationAssociationClumpOperation, const NativeTemporaryModelClumpBorrow&, std::uintptr_t,
                                          NativePrivateAnimationHierarchyController&, ClockResult (*)() noexcept, std::uintptr_t&,
                                          NativeAnimationAssociationCalls::ClumpReceipt* = nullptr,
                                          const NativeAnimationAssociationCalls::ClumpInvocations* = nullptr) noexcept;

    private:
        friend class NativeAnimationAssociationClumpOrigin;
        static ClockResult ResolveHierarchy(NativeAnimationAssociationClumpOrigin&, std::uintptr_t) noexcept;
        static ClockResult Recheck(const NativeAnimationAssociationClumpOrigin&) noexcept;
        static ClockResult Claim(const NativeAnimationAssociationClumpOrigin&) noexcept;
    };
}
