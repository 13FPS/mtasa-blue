/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Keep source scalar semantics distinct from owner/read refusals
 ****************************************************************************/
#include "CAnimBlendAssocGroupDE.h"
#include "CAnimBlockRequestDE.h"
#include "CAnimBlendStaticAssociationDE.h"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    namespace
    {
        void Require(ClockResult result, const char* operation)
        {
            if (result != ClockResult::Ok)
                throw std::runtime_error(std::string("DE animation association-group owner refused ") + operation + ": " +
                                         std::to_string(static_cast<unsigned>(result)));
        }
    }
    NativeAnimationAssociationGroupValues CAnimBlendAssocGroupDE::Read(const char* operation) const
    {
        const auto                            token = reference;
        NativeAnimationAssociationGroupValues value;
        Require(native.ReadAnimationAssociationGroup(token, value), operation);
        return value;
    }
    NativeAnimationAssociationGroupChildBorrowInvalidator CAnimBlendAssocGroupDE::ChildBorrowInvalidator() noexcept
    {
        return {this, [](void* context) noexcept
                {
                    auto& group = *static_cast<CAnimBlendAssocGroupDE*>(context);
                    // Native replacement expires these raw child borrows even when a
                    // later allocation/model step fails. Deleting CPU facades neither
                    // frees native rows nor rolls back the accepted engine effects.
                    group.staticFacades.clear();
                    group.blockFacade.reset();
                    group.blockIdentity = {};
                }};
    }
    void CAnimBlendAssocGroupDE::CreateAssociations(const char* name)
    {
        if (!name)
            throw std::invalid_argument("DE animation association-group CreateAssociations requires a nonnull block name");
        const auto        token = reference;
        const std::string copied(name);
        const auto        invalidator = ChildBorrowInvalidator();
        // Retirement precedes lookup in the real source. Forward the complete
        // name and let that owner decide when CPU child borrows expire, even
        // if a later lookup/allocation step refuses after an accepted effect.
        Require(native.CreateAnimationAssociationGroup(token, copied, invalidator), "CreateAssociations");
    }
    CAnimBlendAssociationSAInterface* CAnimBlendAssocGroupDE::CopyAnimation(unsigned int requested)
    {
        static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
        const auto     token = reference;
        const auto     id = static_cast<std::uint32_t>(requested);
        std::uintptr_t result = 0;
        Require(native.CopyAnimationAssociationGroup(token, id, result), "CopyAnimation");
        // The actual SDK result is an opaque native interface, unlike
        // GetAnimation's C++ static facade. Only the existing issued dynamic
        // record owns this unpublished original copy; no CPU Group destructor
        // may retire it or infer an engine publication/ownership transfer.
        return result ? reinterpret_cast<CAnimBlendAssociationSAInterface*>(result) : nullptr;
    }
    void CAnimBlendAssocGroupDE::InitEmptyAssociations(RpClump* requested)
    {
        const auto token = reference;
        const auto clump = reinterpret_cast<std::uintptr_t>(requested);
        // Source zero-count and already-valid paths do not use the clump.
        // Native row qualification, the exact reverse itinerary and partial
        // original-call receipts belong to this group's existing owner.
        Require(native.InitializeEmptyAnimationAssociationGroup(token, clump), "InitEmptyAssociations");
    }
    CAnimBlock* CAnimBlendAssocGroupDE::GetAnimBlock()
    {
        const auto                           token = reference;
        NativeAnimationAssociationGroupBlock next;
        Require(native.ReadAnimationAssociationGroupBlock(token, next), "GetAnimBlock");
        if (!next.interfaceAddress)
        {
            // Classic SetupAnimBlock retires its cached CPU wrapper when the
            // actual parent pointer becomes null. It owns no native ref/free.
            blockFacade.reset();
            blockIdentity = {};
            return nullptr;
        }
        if (!reference.ownerGeneration || next.identity.ownerGeneration != reference.ownerGeneration ||
            std::find(next.identity.name.begin(), next.identity.name.end(), 0) == next.identity.name.end())
            Require(ClockResult::BindingChanged, "GetAnimBlock");
        if (!blockAccess || !gameProvider)
            Require(ClockResult::NotInitialized, "GetAnimBlock");
        if (blockFacade && blockIdentity.interfaceAddress == next.interfaceAddress)
        {
            // A stable native address does not authorize reviving a facade
            // whose immutable DE name/index affinity has expired in-place.
            if (blockIdentity.identity.index != next.identity.index || blockIdentity.identity.name != next.identity.name ||
                blockIdentity.identity.ownerGeneration != next.identity.ownerGeneration)
                Require(ClockResult::BindingChanged, "GetAnimBlock");
            return blockFacade.get();
        }
        auto product = std::unique_ptr<CAnimBlock>(new CAnimBlockRequestDE(*blockAccess, next.identity, *gameProvider));
        blockFacade = std::move(product);
        blockIdentity = next;
        return blockFacade.get();
    }
    CAnimBlendStaticAssociation* CAnimBlendAssocGroupDE::GetAnimation(unsigned int requested)
    {
        static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
        const auto                                     group = reference;
        const auto                                     id = static_cast<std::uint32_t>(requested);
        NativeAnimationStaticAssociationReferenceToken next;
        Require(native.ResolveAnimationAssociationGroupStaticById(group, id, next), "GetAnimation");
        if (!group.ownerGeneration || next.ownerGeneration != group.ownerGeneration || next.groupIndex != group.index || !next.locator)
            Require(ClockResult::BindingChanged, "GetAnimation");
        if (!staticAccess)
            Require(ClockResult::NotInitialized, "GetAnimation");
        NativeAnimationStaticAssociationValues current;
        Require(staticAccess->ReadAnimationStaticAssociation(next, current), "GetAnimation");
        if (current.interfaceAddress != next.locator)
            Require(ClockResult::BindingChanged, "GetAnimation");
        for (const auto& product : staticFacades)
            if (product.token.ownerGeneration == next.ownerGeneration && product.token.groupIndex == next.groupIndex && product.token.index == next.index &&
                product.token.locator == next.locator)
                return product.facade.get();
        // Classic exposes a raw result. A DE engine row cannot be cast into
        // this C++ SDK vtable: keep its actual completed CPU facade here, with
        // the group's raw-borrow lifetime and the existing native owner.
        auto  product = std::unique_ptr<CAnimBlendStaticAssociation>(new CAnimBlendStaticAssociationDE(*staticAccess, next));
        auto* result = product.get();
        staticFacades.push_back({next, std::move(product)});
        return result;
    }
    bool CAnimBlendAssocGroupDE::IsCreated()
    {
        // Source4D37A0 tests the whole count against zero, independently of
        // child pointers, loaded state and the sign of the preserved bits.
        return Read("IsCreated").numAnimations != 0;
    }
    int CAnimBlendAssocGroupDE::GetNumAnimations()
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        return Read("GetNumAnimations").numAnimations;
    }
    eAnimGroup CAnimBlendAssocGroupDE::GetGroupID()
    {
        static_assert(std::is_same_v<std::underlying_type_t<eAnimGroup>, int>);
        // The actual source casts its raw signed32 field to this fixed enum.
        // Preserve out-of-enum bits; a manager's separate enum gate is not
        // part of an existing group's GetGroupID operation.
        return static_cast<eAnimGroup>(Read("GetGroupID").groupId);
    }
    bool CAnimBlendAssocGroupDE::IsLoaded()
    {
        const auto token = reference;
        bool       value = false;
        Require(native.ReadAnimationAssociationGroupLoaded(token, value), "IsLoaded");
        return value;
    }
    void CAnimBlendAssocGroupDE::SetIDOffset(int requested)
    {
        static_assert(sizeof(int) == sizeof(std::int32_t));
        const auto token = reference;
        const auto value = static_cast<std::int32_t>(requested);
        // The classic inline implementation stores every signed32 bit. A
        // group-count or enum check here would change MTA's actual contract.
        Require(native.SetAnimationAssociationGroupIdOffset(token, value), "SetIDOffset");
    }
    CAnimBlendAssocGroupSAInterface* CAnimBlendAssocGroupDE::GetInterface()
    {
        // Common MTA consumers compare or round-trip this backend tag; they
        // do not dereference the classic structure. This current borrow
        // asserts neither an SA layout nor a new native lifetime lease.
        return reinterpret_cast<CAnimBlendAssocGroupSAInterface*>(Read("GetInterface").interfaceAddress);
    }
}
