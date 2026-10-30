/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit only current registered clumps or an actual active temporary borrow
 ****************************************************************************/
#include "NativeAnimationAssociationClumpOrigin.h"
#include "NativeAnimationAssociationController.h"
#include "NativePrivateAnimationHierarchyController.h"
#include "NativeAnimationHierarchyMemory.h"
#include "NativePedReferenceController.h"
#include "NativePedRenderMemory.h"
#include "NativeTemporaryModelClump.h"
namespace mta::game_de
{
    namespace AnimationAssociationClumpOriginDetail
    {
        bool SamePed(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
    }
    ClockResult NativeAnimationAssociationClumpOrigin::Recheck() const noexcept
    {
        return NativeAnimationAssociationClumpOwnershipAccess::Recheck(*this);
    }
    ClockResult NativeAnimationAssociationClumpOrigin::Claim() const noexcept
    {
        return NativeAnimationAssociationClumpOwnershipAccess::Claim(*this);
    }
    void NativeAnimationAssociationClumpOrigin::Entered() const noexcept
    {
        entered = claimed;
    }
    ClockResult NativeAnimationAssociationClumpOwnershipAccess::ResolveHierarchy(NativeAnimationAssociationClumpOrigin& origin, std::uintptr_t raw) noexcept
    {
        if (!raw || !origin.hierarchies)
            return ClockResult::InvalidArgument;
        const NativeAnimationHierarchyReferenceToken request{origin.generation, raw};
        // Only the real issued hierarchy list chooses the private route. The
        // later held reader validates its live issue/link without a nested gate.
        origin.privateHierarchy = origin.hierarchies->Owns(request);
        if (!origin.privateHierarchy)
        {
            NativeAnimationHierarchyValues values;
            const auto                     result = NativeAnimationHierarchyMemory({origin.image}, origin.owner).Read(raw, values);
            if (result != ClockResult::Ok)
                return result;
            if (values.interfaceAddress != raw)
                return ClockResult::BindingChanged;
        }
        origin.hierarchy = request;
        origin.input.hierarchy = raw;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationAssociationClumpOwnershipAccess::Recheck(const NativeAnimationAssociationClumpOrigin& origin) noexcept
    {
        if (!origin.owner || !origin.image || !origin.generation || origin.receiver.ownerGeneration != origin.generation || !origin.receiver.serial ||
            !origin.input.clump || !origin.hierarchy.locator || origin.hierarchy.ownerGeneration != origin.generation ||
            origin.input.hierarchy != origin.hierarchy.locator || !origin.hierarchies)
            return ClockResult::OwnerConflict;
        auto result = origin.owner();
        if (result != ClockResult::Ok)
            return result;
        if (origin.temporary)
        {
            const auto& borrow = *origin.temporary;
            if (borrow.image != origin.image || borrow.generation != origin.generation || !borrow.record || borrow.record->clump != origin.input.clump ||
                borrow.record->container == 0)
                return ClockResult::OwnerConflict;
            result = borrow.RecheckOrigin();
        }
        else
        {
            if (!origin.references || !origin.referenceMemory)
                return ClockResult::OwnerConflict;
            NativePedPoolReader::Value current;
            result = origin.references->Resolve(*origin.referenceMemory, origin.generation, origin.ped, current);
            if (result != ClockResult::Ok)
                return result;
            if (!AnimationAssociationClumpOriginDetail::SamePed(current, origin.input.ped))
                return ClockResult::BindingChanged;
            std::uintptr_t clump = 0;
            result = NativePedRenderMemory({origin.image}, origin.owner).Read(current, clump);
            if (result == ClockResult::Ok && clump != origin.input.clump)
                result = ClockResult::BindingChanged;
        }
        if (result != ClockResult::Ok)
            return result;
        if (origin.privateHierarchy)
        {
            if (!origin.hierarchies->Owns(origin.hierarchy))
                return ClockResult::BindingChanged;
        }
        else
        {
            NativeAnimationHierarchyValues values;
            result = NativeAnimationHierarchyMemory({origin.image}, origin.owner).Read(origin.hierarchy.locator, values);
            if (result != ClockResult::Ok)
                return result;
            if (values.interfaceAddress != origin.hierarchy.locator)
                return ClockResult::BindingChanged;
        }
        return origin.owner();
    }
    ClockResult NativeAnimationAssociationClumpOwnershipAccess::Claim(const NativeAnimationAssociationClumpOrigin& origin) noexcept
    {
        if (origin.claimed || origin.entered)
            return ClockResult::OwnerConflict;
        const auto result = Recheck(origin);
        if (result != ClockResult::Ok)
            return result;
        if (origin.temporary)
        {
            const auto claimed = origin.temporary->ClaimInitialization();
            if (claimed != ClockResult::Ok)
                return claimed;
        }
        origin.claimed = true;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationAssociationClumpOwnershipAccess::ApplyRegistered(
        std::uintptr_t image, NativeAnimationAssociationController& controller, NativePrivateAnimationHierarchyController& hierarchies,
        NativePedReferenceController& references, const NativePedReferenceMemory& memory, std::uint64_t generation,
        const NativeAnimationAssociationReferenceToken& requested, NativeAnimationAssociationClumpOperation operation, std::uintptr_t clump,
        std::uintptr_t rawHierarchy, ClockResult (*owner)() noexcept, std::uintptr_t& returned, NativeAnimationAssociationCalls::ClumpReceipt* output,
        const NativeAnimationAssociationCalls::ClumpInvocations* invocations) noexcept
    {
        if (output)
            *output = {};
        const auto receiver = requested;
        if (!image || !generation || receiver.ownerGeneration != generation || !receiver.serial || !owner)
            return ClockResult::OwnerConflict;
        if (!clump || !rawHierarchy)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok)
            return result;
        NativeAnimationAssociationClumpOrigin origin;
        origin.image = image;
        origin.generation = generation;
        origin.receiver = receiver;
        origin.input.clump = clump;
        origin.hierarchies = &hierarchies;
        origin.references = &references;
        origin.referenceMemory = &memory;
        origin.owner = owner;
        const auto match = [](const NativePedReferenceToken&, const NativePedPoolReader::Value& value, bool& matches, void* opaque) noexcept
        {
            const auto&    current = *static_cast<NativeAnimationAssociationClumpOrigin*>(opaque);
            std::uintptr_t render = 0;
            const auto     observedResult = NativePedRenderMemory({current.image}, current.owner).Read(value, render);
            if (observedResult == ClockResult::Ok)
                matches = render == current.input.clump;
            return observedResult;
        };
        result = references.FindPublished(memory, generation, match, &origin, origin.ped, origin.input.ped);
        if (result != ClockResult::Ok)
            return result;
        if ((result = ResolveHierarchy(origin, rawHierarchy)) != ClockResult::Ok || (result = Recheck(origin)) != ClockResult::Ok)
            return result;
        // The stack origin expires on return. Storage retains only its own
        // causal receiver/H journals, never this ped or provider pointer.
        return controller.ApplyClump(image, generation, receiver, operation, origin, owner, returned, output, invocations);
    }
    ClockResult NativeAnimationAssociationClumpOwnershipAccess::ApplyTemporary(
        std::uintptr_t image, NativeAnimationAssociationController& controller, std::uint64_t generation,
        const NativeAnimationAssociationReferenceToken& requested, NativeAnimationAssociationClumpOperation operation,
        const NativeTemporaryModelClumpBorrow& borrow, std::uintptr_t rawHierarchy, NativePrivateAnimationHierarchyController& hierarchies,
        ClockResult (*owner)() noexcept, std::uintptr_t& returned, NativeAnimationAssociationCalls::ClumpReceipt* output,
        const NativeAnimationAssociationCalls::ClumpInvocations* invocations) noexcept
    {
        if (output)
            *output = {};
        const auto receiver = requested;
        if (!image || !generation || receiver.ownerGeneration != generation || !receiver.serial || !owner || borrow.image != image ||
            borrow.generation != generation || borrow.owner != owner)
            return ClockResult::OwnerConflict;
        auto result = borrow.RecheckOrigin();
        if (result != ClockResult::Ok)
            return result;
        if (!borrow.record || !borrow.record->clump || !borrow.record->container)
            return ClockResult::OwnerConflict;
        NativeAnimationAssociationClumpOrigin origin;
        origin.image = image;
        origin.generation = generation;
        origin.receiver = receiver;
        origin.input.clump = borrow.record->clump;
        origin.hierarchies = &hierarchies;
        origin.temporary = &borrow;
        origin.model = borrow.record->model;
        origin.owner = owner;
        if ((result = ResolveHierarchy(origin, rawHierarchy)) != ClockResult::Ok || (result = Recheck(origin)) != ClockResult::Ok)
            return result;
        // WithTemporary already owns the actual native gate. The closed
        // origin exposes that fact without a caller-supplied skip-gate flag.
        return controller.ApplyClump(image, generation, receiver, operation, origin, owner, returned, output, invocations);
    }
}
