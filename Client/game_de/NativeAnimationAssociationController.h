/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Runtime-owned issued dynamic association records, no live-pointer adoption
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationStorage.h"
#include "NativeAnimationAssociationIssuedView.h"
#include "NativeAnimationAssociationClumpCalls.h"
#include <list>
#include <memory>
namespace mta::game_de
{
    class NativeAnimationAssociationController final
    {
    public:
        using Token = NativeAnimationAssociationReferenceToken;
        using Values = NativeAnimationAssociationValues;
        using CheckOwner = NativeAnimationAssociationCalls::CheckOwner;
        using Receipt = NativeAnimationAssociationCalls::Receipt;
        using Scalar = NativeAnimationAssociationCalls::Scalar;
        struct RetireProgress
        {
            std::uint32_t scanned = 0, attempted = 0, released = 0, retained = 0, codeRetained = 0;
        };
        NativeAnimationAssociationController() noexcept;
        ~NativeAnimationAssociationController();
        NativeAnimationAssociationController(const NativeAnimationAssociationController&) = delete;
        NativeAnimationAssociationController& operator=(const NativeAnimationAssociationController&) = delete;
        ClockResult CopyFromGroup(std::uintptr_t, std::uint64_t, const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t, CheckOwner,
                                  std::uintptr_t&, NativeAnimationAssociationCalls::GroupCopyReceipt* = nullptr) noexcept;
        ClockResult CopyFromGroupIssued(std::uintptr_t, std::uint64_t, const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t, CheckOwner,
                                        NativeAnimationAssociationIssuedCopy&, NativeAnimationAssociationCalls::GroupCopyReceipt* = nullptr) noexcept;
        void        CloseFacadeIssuance(std::uint64_t generation) noexcept;
        ClockResult PruneFacadeCode(std::uintptr_t image, std::uint64_t generation, CheckOwner) noexcept;
        ClockResult Create(std::uintptr_t, std::uint64_t, CheckOwner, Token&, Receipt* = nullptr) noexcept;
        ClockResult Construct(std::uintptr_t, std::uint64_t, const Token&, std::uintptr_t, CheckOwner, std::uintptr_t&, Receipt* = nullptr) noexcept;
        ClockResult ApplyClump(std::uintptr_t, std::uint64_t, const Token&, NativeAnimationAssociationClumpOperation,
                               const NativeAnimationAssociationClumpOrigin&, CheckOwner, std::uintptr_t&,
                               NativeAnimationAssociationCalls::ClumpReceipt* = nullptr,
                               const NativeAnimationAssociationCalls::ClumpInvocations* = nullptr) noexcept;
        ClockResult Read(std::uintptr_t, std::uint64_t, const Token&, CheckOwner, Values&) noexcept;
        ClockResult ReadLength(std::uintptr_t, std::uint64_t, const Token&, CheckOwner, std::uint32_t&) noexcept;
        ClockResult Set(std::uintptr_t, std::uint64_t, const Token&, Scalar, std::uint32_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult SetCurrentProgress(std::uintptr_t, std::uint64_t, const Token&, std::uint32_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult FreeNodeArray(std::uintptr_t, std::uint64_t, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult RetireOne(std::uintptr_t, std::uint64_t, const Token&, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult Retire(std::uintptr_t, std::uint64_t, CheckOwner, std::uint32_t, RetireProgress&) noexcept;
        ClockResult ResolveHierarchy(std::uintptr_t, std::uint64_t, const Token&, CheckOwner, NativeAnimationHierarchyReferenceToken&) noexcept;
        ClockResult RestrictToBones(std::uintptr_t, std::uint64_t, const Token&, std::uint64_t, CheckOwner, Receipt* = nullptr) noexcept;
        ClockResult RestrictToBonesOf(std::uintptr_t, std::uint64_t, const Token&, std::uintptr_t, CheckOwner, Receipt* = nullptr) noexcept;
        bool        HasPendingNative() const noexcept { return busy.load() || retained.load(); }
        bool        HasPendingFacadeCode() const noexcept { return busy.load() || codeRetained.load(); }
        bool        IsRetained() const noexcept { return busy.load() || retained.load() || codeRetained.load(); }
        bool        HasPending() const noexcept { return IsRetained(); }
        bool        RecoveryPending() const noexcept { return recovery.load(); }

    private:
        struct Entry;
        std::list<std::unique_ptr<Entry>> records;
        std::uint64_t                     serial = 0;
        std::atomic<bool>                 busy{false}, retained{false}, codeRetained{false}, recovery{false};
        std::atomic<std::uint64_t>        closedFacadeThrough{0};
        std::uint64_t                     facadeGeneration = 0;
        ClockResult CopyFromGroupImpl(std::uintptr_t, std::uint64_t, const NativeAnimationAssociationGroupReferenceToken&, std::uint32_t, CheckOwner,
                                      std::uintptr_t&, NativeAnimationAssociationIssuedCopy*, NativeAnimationAssociationCalls::GroupCopyReceipt*) noexcept;
        static bool CodePending(const Entry&) noexcept;
        static bool CanErase(const Entry&) noexcept;
        void        Publish() noexcept;
        Entry*      Find(std::uint64_t, const Token&) noexcept;
    };
}
