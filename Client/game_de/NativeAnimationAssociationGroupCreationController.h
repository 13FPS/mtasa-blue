/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: One source-ordered MTA group creation with durable native journals
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationGroupReplacement.h"
#include "NativeTemporaryStaticAssociationInitializeCalls.h"
#include "NativeTemporaryModelClump.h"
#include "NativeModelHashReader.h"
#include <atomic>
#include <memory>
#include <string>
namespace mta::game_de
{
    namespace NativeAnimationAssociationGroupCreation
    {
        using CheckOwner = NativeAnimationManagerMemory::CheckOwner;
        enum class Phase : std::uint32_t
        {
            Preparing,
            RetireOld,
            FindBlock,
            PrepareRows,
            PublishBlock,
            AllocateArray,
            PublishChildren,
            PublishCount,
            ReadHierarchy,
            FindModel,
            Temporary,
            WriteID,
            WriteGroup,
            Finish,
            Completed,
            DrainPrivate
        };
        struct StepFacts
        {
            std::uint32_t                                               index = 0;
            NativeAnimationHierarchyReferenceToken                      hierarchy{};
            NativeModelHashReader::Value                                model{};
            NativeTemporaryModelClumpCalls::Receipt                     temporary{};
            NativeStaticAssociationInitializeCalls::Receipt             initialization{};
            NativeAnimationManagerMemory::StaticAssociationWriteReceipt idStore{}, groupStore{};
            ClockResult                                                 result = ClockResult::NotInitialized;
            bool visited = false, missingModel = false, initialized = false, idStored = false, groupStored = false;
        };
        struct Receipt
        {
            std::uint64_t                                 generation = 0, serial = 0, nameLength = 0;
            NativeAnimationAssociationGroupReferenceToken group{};
            NativeAnimationBlockName                      query{};
            NativeAnimationBlockIdentity                  block{};
            std::uint32_t count = 0, visited = 0, missingModels = 0, initialized = 0, idsStored = 0, groupsStored = 0, failedIndex = 0xffffffffu;
            Phase         phase = Phase::Preparing;
            ClockResult   result = ClockResult::NotInitialized;
            bool          nameCopied = false, queryPrepared = false, hadEffects = false, completed = false, partialAccepted = false;
            bool          enteredUnknown = false, nativeRecovery = false, privateCleanupPending = false, retained = false;
            NativeAnimationAssociationGroupReplacementCalls::Receipt replacement{};
            NativeStaticAssociationArrayStorageCalls::Receipt        array{};
            StepFacts                                                lastStep{};
        };
        struct Invocations
        {
            NativeAnimationAssociationGroupReplacementCalls::InvokeRetire      retireGroup = nullptr;
            NativeAnimationAssociationGroupReplacementCalls::InvokeQword       qword = nullptr;
            NativeAnimationAssociationGroupReplacementCalls::InvokeDword       dword = nullptr;
            NativeCustomAnimSequenceStorageCalls::InvokeMalloc                 allocateArray = nullptr;
            NativeStaticAssociationArrayStorageCalls::InvokeDeletingDestructor retireArray = nullptr;
            NativeCustomAnimSequenceStorageCalls::InvokeFree                   freeArray = nullptr;
            NativeTemporaryModelClumpCalls::Invocations                        temporary{};
            NativeStaticAssociationInitializeCalls::Invoke                     initialize = nullptr;
            NativeAnimationManagerMemory::StaticAssociationExchange            word = nullptr;
        };
        struct DrainProgress
        {
            std::uint32_t scanned = 0, attempted = 0, released = 0, retained = 0;
            ClockResult   ownerStatus = ClockResult::Ok;
        };
    }
    class NativeAnimationAssociationGroupCreationController final
    {
    public:
        using CheckOwner = NativeAnimationAssociationGroupCreation::CheckOwner;
        using Receipt = NativeAnimationAssociationGroupCreation::Receipt;
        using Invocations = NativeAnimationAssociationGroupCreation::Invocations;
        using StepFacts = NativeAnimationAssociationGroupCreation::StepFacts;
        using DrainProgress = NativeAnimationAssociationGroupCreation::DrainProgress;
        NativeAnimationAssociationGroupCreationController() noexcept;
        ~NativeAnimationAssociationGroupCreationController();
        NativeAnimationAssociationGroupCreationController(const NativeAnimationAssociationGroupCreationController&) = delete;
        NativeAnimationAssociationGroupCreationController& operator=(const NativeAnimationAssociationGroupCreationController&) = delete;
        ClockResult Create(std::uintptr_t image, std::uint64_t generation, const NativeAnimationAssociationGroupReferenceToken&,
                           const std::string& copiedBlockName, CheckOwner, NativeAnimationAssociationGroupChildBorrowInvalidator = {}, Receipt* = nullptr,
                           const Invocations* = nullptr) noexcept;
        ClockResult DrainPrivatePending(std::uintptr_t image, std::uint64_t generation, CheckOwner, std::uint32_t budget, DrainProgress&) noexcept;
        ClockResult ReadStep(std::uint64_t generation, std::uint64_t serial, std::uint32_t index, StepFacts&) noexcept;
        bool        IsRetained() const noexcept { return busy.load() || retained.load(); }
        bool        RecoveryPending() const noexcept { return recovery.load(); }

    private:
        struct Data;
        std::unique_ptr<Data> data;
        std::atomic<bool>     busy{false}, retained{false}, recovery{false};
        std::uint64_t         serial = 0;
        void                  Publish() noexcept;
    };
}
