/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the controlled registry inverse before exposing a native tag
 ****************************************************************************/
#include "CPedPooledDE.h"
#include "CPoolsDE.h"
#include "NativeGameTimingAccess.h"
#include <stdexcept>
#include <string>
namespace mta::game_de
{
    bool CPedPooledDE::SameReference(const NativePedReferenceToken& a, const NativePedReferenceToken& b) noexcept
    {
        return a.version && a.version == b.version && a.ownerGeneration == b.ownerGeneration && a.slot == b.slot;
    }
    bool CPedPooledDE::SameIdentity(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
    {
        return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
               a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
               a.ownsAllocations == b.ownsAllocations;
    }
    ClockResult CPedPooledDE::ReadRegisteredIdentity(CPoolsDE& pools, NativeGameTimingAccess& owner, CPed* ped, NativePedPoolReader::Value& output,
                                                     NativePedReferenceToken& reference) noexcept
    {
        if (!ped)
            return ClockResult::InvalidArgument;
        if (!pools.HasNativeOwner(owner))
            return ClockResult::OwnerConflict;
        NativePedReferenceToken firstReference, secondReference, confirmedReference;
        auto                    result = pools.ReadBorrowedPedReference(ped, firstReference);
        if (result != ClockResult::Ok)
            return result;
        NativePedPoolReader::Value before, after;
        if ((result = owner.ResolvePedReference(firstReference, before)) != ClockResult::Ok ||
            (result = pools.ReadBorrowedPedReference(ped, secondReference)) != ClockResult::Ok)
            return result;
        if (!SameReference(firstReference, secondReference))
            return ClockResult::BindingChanged;
        if ((result = owner.ResolvePedReference(secondReference, after)) != ClockResult::Ok ||
            (result = pools.ReadBorrowedPedReference(ped, confirmedReference)) != ClockResult::Ok)
            return result;
        if (!SameReference(firstReference, confirmedReference) || !SameIdentity(before, after) || !after.nativePed || !pools.HasNativeOwner(owner))
            return ClockResult::BindingChanged;
        // The registry owns SDK identity and the existing reference controller
        // owns current native identity. This copy acquires neither one anew.
        output = after;
        reference = confirmedReference;
        return ClockResult::Ok;
    }
    ClockResult CPedPooledDE::ReadRegisteredCameraTarget(CPoolsDE& pools, NativeGameTimingAccess& owner, CameraTargetReader read, const void* context,
                                                         CEntity*& output) noexcept
    {
        RegisteredPedIdentity current;
        const auto            result = ReadRegisteredEntityTarget(pools, owner, read, context, current);
        if (result == ClockResult::Ok)
            output = current.entity;
        return result;
    }
    ClockResult CPedPooledDE::ReadRegisteredEntityTarget(CPoolsDE& pools, NativeGameTimingAccess& owner, CameraTargetReader read, const void* context,
                                                         RegisteredPedIdentity& output) noexcept
    {
        if (!read || !context)
            return ClockResult::InvalidArgument;
        if (!pools.HasNativeOwner(owner))
            return ClockResult::OwnerConflict;
        std::uintptr_t target = 0, confirmed = 0;
        auto           result = read(context, target);
        if (result != ClockResult::Ok)
            return result;
        if (!target)
        {
            if ((result = read(context, confirmed)) != ClockResult::Ok)
                return result;
            if (confirmed)
                return ClockResult::BindingChanged;
            output = {};
            return ClockResult::Ok;
        }
        SClientEntity<CPed>* first = nullptr;
        if ((result = pools.ReadPedByAddress(target, first)) != ClockResult::Ok)
            return result;
        // A ped-only registry miss must not impersonate general GetEntity's
        // object/vehicle/building coverage or a qualified absent target.
        if (!first || !first->pEntity)
            return ClockResult::NotInitialized;
        CPed* const                ped = first->pEntity;
        NativePedPoolReader::Value before, after;
        NativePedReferenceToken    beforeReference, afterReference;
        if ((result = ReadRegisteredIdentity(pools, owner, ped, before, beforeReference)) != ClockResult::Ok)
            return result;
        if (before.nativePed != target)
            return ClockResult::BindingChanged;
        if ((result = read(context, confirmed)) != ClockResult::Ok)
            return result;
        if (confirmed != target)
            return ClockResult::BindingChanged;
        SClientEntity<CPed>* current = nullptr;
        if ((result = pools.ReadPedByAddress(confirmed, current)) != ClockResult::Ok)
            return result;
        if (current != first || !current || current->pEntity != ped)
            return ClockResult::BindingChanged;
        if ((result = ReadRegisteredIdentity(pools, owner, ped, after, afterReference)) != ClockResult::Ok)
            return result;
        if (!SameIdentity(before, after) || !SameReference(beforeReference, afterReference))
            return ClockResult::BindingChanged;
        if ((result = read(context, confirmed)) != ClockResult::Ok)
            return result;
        if (confirmed != target)
            return ClockResult::BindingChanged;
        NativePedReferenceToken reference;
        if ((result = pools.ReadBorrowedPedReference(ped, reference)) != ClockResult::Ok)
            return result;
        // Same SDK/native address values can recur after CPU withdrawal.
        // Keep the first confirmed registration token through the last seam.
        if (!SameReference(beforeReference, reference))
            return ClockResult::BindingChanged;
        // Ordinary virtual-base conversion returns the actual borrowed SDK
        // entity. Native bits and StoredPointer never become a CEntity object.
        output = {ped, static_cast<CEntity*>(ped), after, beforeReference};
        return ClockResult::Ok;
    }
    CEntitySAInterface* CPedPooledDE::GetInterface()
    {
        NativePedPoolReader::Value current;
        NativePedReferenceToken    reference;
        const auto                 result = ReadRegisteredIdentity(registry, nativeOwner, static_cast<CPed*>(this), current, reference);
        if (result != ClockResult::Ok)
            throw std::runtime_error("DE ped pool owner refused GetInterface: " + std::to_string(static_cast<unsigned>(result)));
        // The shared legacy spelling is an opaque backend tag. It grants no
        // classic CEntitySA layout, persistent raw-pointer lease or adoption.
        return reinterpret_cast<CEntitySAInterface*>(current.nativePed);
    }
}
