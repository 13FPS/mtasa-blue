/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve native identity before publishing a borrowed SDK ped link
 ****************************************************************************/
#include "CPoolsDE.h"
#include "CPedVitalsDE.h"
#include "NativeGameTimingAccess.h"
#include <exception>
#include <limits>
#include <stdexcept>

namespace mta::game_de
{
    CPoolsDE::BorrowedPedRegistration::~BorrowedPedRegistration()
    {
        Reset();
    }
    ClockResult CPoolsDE::BorrowedPedRegistration::Reset() noexcept
    {
        return registry ? registry->Detach(*this) : ClockResult::Ok;
    }
    CPoolsDE::~CPoolsDE()
    {
        // A pool cannot disappear while caller-owned guards still name its
        // rows. Failing the explicit borrow contract must not leave a dangling
        // registry pointer that a later guard destructor would dereference.
        for (const auto& control : pedControls)
            if (control.registration)
                std::terminate();
    }
    bool CPoolsDE::Same(const NativePedPoolReader::Value& first, const NativePedPoolReader::Value& second) noexcept
    {
        return first.handle == second.handle && first.slot == second.slot && first.pool == second.pool && first.objects == second.objects &&
               first.flags == second.flags && first.nativePed == second.nativePed && first.vtable == second.vtable && first.firstMethod == second.firstMethod &&
               first.capacity == second.capacity && first.cursor == second.cursor && first.generation == second.generation &&
               first.ownsAllocations == second.ownsAllocations;
    }
    bool CPoolsDE::HasExactBorrowedPedBinding(const CPedVitalsDE& wrapper, const NativePedReferenceToken& expected) const noexcept
    {
        return &wrapper.native == static_cast<const NativePedVitalsAccess*>(&pedRegistryOwner) && wrapper.reference.version == expected.version &&
               wrapper.reference.ownerGeneration == expected.ownerGeneration && wrapper.reference.slot == expected.slot;
    }
    bool CPoolsDE::HasBorrowedPedLink(const CPed& wrapper) const noexcept
    {
        for (const auto& link : pedLinks)
            if (link.pEntity == &wrapper)
                return true;
        return false;
    }
    bool CPoolsDE::Matches(std::size_t index) const noexcept
    {
        if (index >= PedCapacity)
            return false;
        const auto& control = pedControls[index];
        const auto& link = pedLinks[index];
        return control.registration && control.expectedPed && control.version && link.pEntity == control.expectedPed &&
               link.pClientEntity == control.expectedClient;
    }
    ClockResult CPoolsDE::BindBorrowedPed(CPedVitalsDE& wrapper, CClientEntity* client, BorrowedPedRegistration& registration) noexcept
    {
        if (registration.registry)
            return ClockResult::Busy;
        // Ordinary C++ base conversion compares actual owner subobjects. It
        // neither reinterprets a template row nor trusts an unrelated CPed.
        if (&wrapper.native != static_cast<NativePedVitalsAccess*>(&pedRegistryOwner))
            return ClockResult::OwnerConflict;
        auto result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        NativePedPoolReader::Value before, after;
        result = pedRegistryOwner.ResolvePedReference(wrapper.reference, before);
        if (result != ClockResult::Ok)
            return result;
        const auto index = static_cast<std::size_t>(before.slot);
        if (index >= PedCapacity || before.slot != before.handle >> 8)
            return ClockResult::BindingChanged;
        auto& control = pedControls[index];
        auto& link = pedLinks[index];
        // The shared classic Register policy is used only after controlled
        // admission. Never overwrite another live guard or a caller-mutated
        // foreign pair merely because a native slot number happens to match.
        if (control.registration || link.pEntity || link.pClientEntity || !MtaGame::CPedRegistryPolicy::MayCreate(pedRegistrationCount, PedCapacity))
            return ClockResult::Busy;
        if (control.version == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::Busy;
        result = pedRegistryOwner.RecheckPedPoolHandle(before);
        if (result != ClockResult::Ok)
            return result;
        result = pedRegistryOwner.ResolvePedReference(wrapper.reference, after);
        if (result != ClockResult::Ok)
            return result;
        if (!Same(before, after) || control.registration || link.pEntity || link.pClientEntity)
            return ClockResult::BindingChanged;
        result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        if (registration.registry || control.registration || link.pEntity || link.pClientEntity ||
            !MtaGame::CPedRegistryPolicy::MayCreate(pedRegistrationCount, PedCapacity) || control.version == (std::numeric_limits<std::uint64_t>::max)())
            return ClockResult::BindingChanged;
        if (!MtaGame::CPedRegistryPolicy::Register<CPed>(pedLinks, pedRegistrationCount, index, &wrapper, client))
            return ClockResult::InvalidArgument;
        ++control.version;
        control.reference = wrapper.reference;
        control.expectedPed = &wrapper;
        control.expectedClient = client;
        control.registration = &registration;
        registration.registry = this;
        registration.index = index;
        registration.version = control.version;
        return ClockResult::Ok;
    }
    ClockResult CPoolsDE::Detach(BorrowedPedRegistration& registration) noexcept
    {
        const auto index = registration.index;
        const auto version = registration.version;
        registration.registry = nullptr;
        registration.index = PedCapacity;
        registration.version = 0;
        if (index >= PedCapacity)
            return ClockResult::BindingChanged;
        auto& control = pedControls[index];
        if (control.registration != &registration || !version || version != control.version)
            return ClockResult::BindingChanged;
        const bool ownPair = Matches(index);
        // Public SDK pairs remain mutable. A stale guard may withdraw its own
        // control record, but cannot clear somebody else's replacement/client
        // association or decrement that row's unrelated registration count.
        control.registration = nullptr;
        control.expectedPed = nullptr;
        control.expectedClient = nullptr;
        control.reference = {};
        if (!ownPair)
            return ClockResult::BindingChanged;
        MtaGame::CPedRegistryPolicy::Clear(pedLinks, index);
        // This operation owns no wrapper destruction. Borrow withdrawal is
        // complete now; an enclosing factory destroys its wrapper afterward.
        MtaGame::CPedRegistryPolicy::FinishRemoval(pedRegistrationCount);
        return ClockResult::Ok;
    }
    ClockResult CPoolsDE::ReadControlled(std::size_t index, NativePedPoolReader::Value& output) noexcept
    {
        if (!Matches(index))
            return ClockResult::BindingChanged;
        auto&                      control = pedControls[index];
        const auto                 version = control.version;
        const auto                 token = control.reference;
        NativePedPoolReader::Value value;
        auto                       result = pedRegistryOwner.ResolvePedReference(token, value);
        if (result != ClockResult::Ok)
            return result;
        if (value.slot != index || !Matches(index) || control.version != version || control.reference.version != token.version ||
            control.reference.ownerGeneration != token.ownerGeneration || control.reference.slot != token.slot)
            return ClockResult::BindingChanged;
        output = value;
        return ClockResult::Ok;
    }
    ClockResult CPoolsDE::ReadPedFromRef(std::uint32_t handle, CPed*& output) noexcept
    {
        auto result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        NativePedPoolReader::Value native;
        bool                       present = false;
        // The actual shared policy always invokes the genuine native resolver
        // first, including when no SDK wrapper exists at this native index.
        CPed* const wrapper = MtaGame::CPedRegistryPolicy::FindFromRef(pedLinks, handle,
                                                                       [&](std::uint32_t selected)
                                                                       {
                                                                           result = pedRegistryOwner.ProbePedPoolHandle(selected, present, native);
                                                                           return result == ClockResult::Ok && present;
                                                                       });
        const auto  checked = pedRegistryOwner.CheckPedPoolAccess();
        if (checked != ClockResult::Ok)
            return checked;
        if (result != ClockResult::Ok)
            return result;
        if (!wrapper)
        {
            output = nullptr;
            return ClockResult::Ok;
        }
        NativePedPoolReader::Value referenced;
        result = ReadControlled(native.slot, referenced);
        if (result != ClockResult::Ok)
            return result;
        if (!Same(native, referenced))
            return ClockResult::BindingChanged;
        result = pedRegistryOwner.RecheckPedPoolHandle(native);
        if (result != ClockResult::Ok)
            return result;
        if (!Matches(native.slot) || pedLinks[native.slot].pEntity != wrapper)
            return ClockResult::BindingChanged;
        output = wrapper;
        return ClockResult::Ok;
    }
    ClockResult CPoolsDE::ReadPedByIndex(std::size_t index, SClientEntity<CPed>*& output) noexcept
    {
        if (index >= PedCapacity)
            return ClockResult::InvalidArgument;
        auto result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        if (!pedControls[index].registration && !pedLinks[index].pEntity && !pedLinks[index].pClientEntity)
        {
            // Preserve the actual indexed SDK view of an empty bounded row.
            output = &pedLinks[index];
            return ClockResult::Ok;
        }
        NativePedPoolReader::Value value;
        result = ReadControlled(index, value);
        if (result != ClockResult::Ok)
            return result;
        result = pedRegistryOwner.RecheckPedPoolHandle(value);
        if (result != ClockResult::Ok)
            return result;
        if (!Matches(index))
            return ClockResult::BindingChanged;
        output = &pedLinks[index];
        return ClockResult::Ok;
    }
    ClockResult CPoolsDE::ReadPedByAddress(std::uintptr_t address, SClientEntity<CPed>*& output) noexcept
    {
        auto result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        if (address)
            for (std::size_t index = 0; index < PedCapacity; ++index)
            {
                if (!pedLinks[index].pEntity && !pedLinks[index].pClientEntity)
                    continue;
                NativePedPoolReader::Value value;
                result = ReadControlled(index, value);
                if (result != ClockResult::Ok)
                    return result;
                if (value.nativePed != address)
                    continue;
                result = pedRegistryOwner.RecheckPedPoolHandle(value);
                if (result != ClockResult::Ok)
                    return result;
                if (!Matches(index))
                    return ClockResult::BindingChanged;
                output = &pedLinks[index];
                return ClockResult::Ok;
            }
        result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        output = nullptr;
        return ClockResult::Ok;
    }
    ClockResult CPoolsDE::ReadPedClientEntity(CPed* wrapper, CClientEntity*& output) noexcept
    {
        auto result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        // Pointer identity only: no foreign wrapper or native interface is
        // dereferenced, and no count, reference cell or registration is added.
        auto* const row = MtaGame::CPedRegistryPolicy::FindByWrapper(pedLinks, wrapper);
        if (!row)
        {
            result = pedRegistryOwner.CheckPedPoolAccess();
            if (result != ClockResult::Ok)
                return result;
            if (MtaGame::CPedRegistryPolicy::FindByWrapper(pedLinks, wrapper))
                return ClockResult::BindingChanged;
            output = nullptr;
            return ClockResult::Ok;
        }
        const auto                 index = static_cast<std::size_t>(row - pedLinks.data());
        const auto                 version = pedControls[index].version;
        auto* const                client = row->pClientEntity;
        NativePedPoolReader::Value before, after;
        result = ReadControlled(index, before);
        if (result != ClockResult::Ok)
            return result;
        result = pedRegistryOwner.RecheckPedPoolHandle(before);
        if (result != ClockResult::Ok)
            return result;
        result = ReadControlled(index, after);
        if (result != ClockResult::Ok)
            return result;
        if (!Same(before, after))
            return ClockResult::BindingChanged;
        result = pedRegistryOwner.CheckPedPoolAccess();
        if (result != ClockResult::Ok)
            return result;
        result = pedRegistryOwner.RecheckPedPoolHandle(after);
        if (result != ClockResult::Ok)
            return result;
        if (!Matches(index) || pedControls[index].version != version || row->pEntity != wrapper || row->pClientEntity != client ||
            MtaGame::CPedRegistryPolicy::FindByWrapper(pedLinks, wrapper) != row)
            return ClockResult::BindingChanged;
        // Invalidation deliberately updates the controlled expected client to
        // null. Arbitrary caller replacement remains a refusal, like all other
        // existing controlled DE lookups; neither case borrows StoredPointer.
        output = client;
        return ClockResult::Ok;
    }
    void CPoolsDE::Require(ClockResult result, const char* operation)
    {
        if (result != ClockResult::Ok)
            throw std::runtime_error(std::string("DE ped pool owner refused ") + operation + ": " + std::to_string(static_cast<unsigned>(result)));
    }
    CPed* CPoolsDE::GetPedFromRef(DWORD handle)
    {
        CPed* value = nullptr;
        Require(ReadPedFromRef(handle, value), "GetPedFromRef");
        return value;
    }
    SClientEntity<CPed>* CPoolsDE::GetPed(DWORD* nativeInterface)
    {
        SClientEntity<CPed>* value = nullptr;
        Require(ReadPedByAddress(reinterpret_cast<std::uintptr_t>(nativeInterface), value), "GetPed(native interface)");
        return value;
    }
    SClientEntity<CPed>* CPoolsDE::GetPed(std::size_t index)
    {
        SClientEntity<CPed>* value = nullptr;
        Require(ReadPedByIndex(index, value), "GetPed(index)");
        return value;
    }
    CClientEntity* CPoolsDE::GetPedClientEntity(CPed* wrapper)
    {
        CClientEntity* value = nullptr;
        Require(ReadPedClientEntity(wrapper, value), "GetPedClientEntity");
        return value;
    }
    void CPoolsDE::ResetPedPoolCount()
    {
        MtaGame::CPedRegistryPolicy::ResetCount(pedRegistrationCount);
    }
    void CPoolsDE::InvalidateLocalPlayerClientEntity()
    {
        const bool ownPair = Matches(0);
        MtaGame::CPedRegistryPolicy::InvalidateLocalClient(pedLinks);
        if (ownPair)
            pedControls[0].expectedClient = nullptr;
    }
}
