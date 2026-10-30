/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Pre-reserved owner journals for the original clump operations
 ****************************************************************************/
#pragma once
#include "NativeAnimationAssociationClumpTypes.h"
#include "NativeAnimationAssociationClumpVerification.h"
#include "NativeAnimationClumpInitializationPlan.h"
#include "NativeAnimationAssociationHierarchyDependency.h"
#include "NativeAnimationAssociationProgressDetail.h"
#include <memory>
#include <vector>
namespace mta::game_de::AnimationAssociationClumpStorageDetail
{
    // The containing actual association storage remains the authority. These
    // journals neither adopt an address nor release a hierarchy from a C++
    // destructor; unresolved keys deliberately remain with the actual record.
    template <class BankObservation>
    struct Pending
    {
        NativeAnimationAssociationCalls::ClumpReceipt                  receipt{};
        NativeAnimationHierarchyReferenceToken                         targetHierarchy{};
        bool                                                           targetPrivate = false, copyReference = false, acceptedSource = false;
        bool                                                           nodeClearDefaultAccepted = false;
        BankObservation                                                targetBank{};
        NativeAnimationClumpInitializationPlan::Snapshot               plan;
        NativeAnimationAssociationClumpVerification::Bytes             before{};
        NativeAnimationAssociationClumpVerification::Observation       observation;
        std::vector<unsigned char>                                     suppressed;
        std::vector<AnimationAssociationProgressDetail::Cache>         cache;
        std::unique_ptr<NativeAnimationAssociationHierarchyDependency> newHold;
        std::unique_ptr<NativeAnimationAssociationHierarchyDependency> oldHold;
    };
    template <class BankObservation>
    struct State
    {
        NativeAnimationAssociationReferenceToken                       receiver{};
        NativeAnimationHierarchyReferenceToken                         hierarchy{};
        bool                                                           acceptedSource = false, privateHierarchy = false, copyReference = false;
        std::uint64_t                                                  nextCommand = 0;
        std::uintptr_t                                                 sequences = 0;
        std::uint16_t                                                  sequenceCount = 0;
        std::unique_ptr<NativeAnimationAssociationHierarchyDependency> hold;
        // A new command is fully allocated before native effects. Old/new
        // holds survive an uncertain call; a known replacement can release
        // only its old key after the new source has been verified.
        std::unique_ptr<Pending<BankObservation>> pending;
    };
}
