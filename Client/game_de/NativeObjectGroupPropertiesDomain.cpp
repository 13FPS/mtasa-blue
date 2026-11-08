/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve typed MTA group defaults without replaying native prefixes
 ****************************************************************************/
#include "NativeObjectGroupPropertiesDomain.h"
#include "NativeObjectGroupPropertiesProfile.h"
#include <game/CObjectGroupPropertiesPolicy.h>
// Preserve the actual shared vector equality policy without a legacy PCH.
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include <cstring>

namespace mta::game_de
{
    namespace ObjectPropertiesDomainDetail
    {
        namespace P = NativeObjectGroupPropertiesProfile;
        using Field = NativeObjectGroupProperty;
        constexpr std::uint32_t All = (std::uint32_t{1} << P::PropertyCount) - 1;
        static_assert(P::PropertyCount == static_cast<std::size_t>(Field::Count) && P::PropertyCount < 32);
        struct Guard
        {
            std::atomic_flag&  operation;
            std::atomic<bool>& active;
            bool               held;
            Guard(std::atomic_flag& flag, std::atomic<bool>& busy) noexcept : operation(flag), active(busy), held(!flag.test_and_set())
            {
                if (held)
                    active.store(true);
            }
            ~Guard()
            {
                if (held)
                {
                    active.store(false);
                    operation.clear();
                }
            }
        };
        float Float(std::uint32_t bits) noexcept
        {
            float result = 0;
            std::memcpy(&result, &bits, sizeof(result));
            return result;
        }
        std::uint32_t Bits(float value) noexcept
        {
            std::uint32_t result = 0;
            std::memcpy(&result, &value, sizeof(result));
            return result;
        }
        bool Scalar(Field field) noexcept
        {
            return field <= Field::CollisionDamageMultiplier || field == Field::SmashMultiplier || field == Field::BreakVelocityRandomness;
        }
        bool Raw(Field field) noexcept
        {
            return (field >= Field::CollisionDamageEffect && field <= Field::FxType) || field == Field::BreakMode || field == Field::SparksOnImpact;
        }
        bool Boolean(Field field) noexcept
        {
            return field == Field::CameraAvoidObject || field == Field::CausesExplosion || field == Field::SparksOnImpact;
        }
        bool Vector(Field field) noexcept
        {
            return field == Field::FxOffsetX || field == Field::BreakVelocityX;
        }
        CVector Vector(const NativeObjectGroupVectorBits& bits) noexcept
        {
            return CVector(Float(bits[0]), Float(bits[1]), Float(bits[2]));
        }
        ClockResult Admit(std::uintptr_t image, std::uint64_t generation, NativeObjectGroupPropertiesMemory::CheckOwner owner,
                          NativeObjectGroupPropertiesMemory::CheckOwner afterApplication) noexcept
        {
            if (!image || !generation || !owner || !afterApplication)
                return ClockResult::InvalidArgument;
            auto result = afterApplication();
            return result == ClockResult::Ok ? NativeStreamingCompletionContext::Owner(image, owner) : result;
        }
    }
    void NativeObjectGroupPropertiesDomain::PublishPending() noexcept
    {
        pending.store(!originals.empty() || vectorChange.active || uncertain.load());
    }
    ClockResult NativeObjectGroupPropertiesDomain::Write(std::uintptr_t image, const NativeObjectGroupPropertiesMemory::Value& current,
                                                         NativeObjectGroupProperty field, std::uint32_t desired, CheckOwner owner, CheckOwner afterApplication,
                                                         bool& accepted) noexcept
    {
        NativeObjectGroupPropertiesMemory::Receipt receipt;
        auto result = NativeObjectGroupPropertiesMemory::Write(image, current, field, desired, owner, afterApplication, receipt);
        accepted = receipt.accepted;
        if (result == ClockResult::Ok && (!receipt.entered || !receipt.accepted || receipt.uncertain))
            result = ClockResult::WriteFailure;
        if (receipt.uncertain || (receipt.entered && receipt.accepted && result != ClockResult::Ok))
        {
            if (!uncertain.load())
            {
                firstUncertain = receipt;
                uncertain.store(true);
            }
            pending.store(true);
        }
        return result;
    }
    ClockResult NativeObjectGroupPropertiesDomain::Read(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty field,
                                                        CheckOwner owner, CheckOwner afterApplication, float& output) noexcept
    {
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (!ObjectPropertiesDomainDetail::Scalar(field))
            return ClockResult::InvalidArgument;
        NativeObjectGroupPropertiesMemory::Value current;
        const auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result == ClockResult::Ok)
            output = ObjectPropertiesDomainDetail::Float(current.bits[static_cast<std::size_t>(field)]);
        return result;
    }
    ClockResult NativeObjectGroupPropertiesDomain::ReadRaw(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty field,
                                                           CheckOwner owner, CheckOwner afterApplication, std::uint32_t& output) noexcept
    {
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (!ObjectPropertiesDomainDetail::Raw(field))
            return ClockResult::InvalidArgument;
        NativeObjectGroupPropertiesMemory::Value current;
        const auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result == ClockResult::Ok)
            output = current.bits[static_cast<std::size_t>(field)];
        return result;
    }
    ClockResult NativeObjectGroupPropertiesDomain::ReadVector(std::uintptr_t image, std::uint64_t generation, std::uint8_t group,
                                                              NativeObjectGroupProperty field, CheckOwner owner, CheckOwner afterApplication,
                                                              NativeObjectGroupVectorBits& output) noexcept
    {
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (!ObjectPropertiesDomainDetail::Vector(field))
            return ClockResult::InvalidArgument;
        NativeObjectGroupPropertiesMemory::Value current;
        const auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result == ClockResult::Ok)
            for (std::size_t axis = 0; axis != output.size(); ++axis)
                output[axis] = current.bits[static_cast<std::size_t>(field) + axis];
        return result;
    }
    ClockResult NativeObjectGroupPropertiesDomain::HasOriginal(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, CheckOwner owner,
                                                               CheckOwner afterApplication, bool& output) noexcept
    {
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        NativeObjectGroupPropertiesMemory::Value current;
        const auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return result;
        const auto found = originals.find(group);
        if (found != originals.end() && !NativeObjectGroupPropertiesMemory::SameIdentity(found->second.origin, current))
            return ClockResult::BindingChanged;
        output = found != originals.end();
        return ClockResult::Ok;
    }
    ClockResult NativeObjectGroupPropertiesDomain::Set(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty field,
                                                       float desired, bool& modified, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        return Change(image, generation, group, field, {ObjectPropertiesDomainDetail::Bits(desired), 0, 0}, Kind::Float, modified, owner, afterApplication);
    }
    ClockResult NativeObjectGroupPropertiesDomain::SetRaw(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty field,
                                                          std::uint32_t desired, bool& modified, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        return Change(image, generation, group, field, {desired, 0, 0}, Kind::Raw, modified, owner, afterApplication);
    }
    ClockResult NativeObjectGroupPropertiesDomain::SetVector(std::uintptr_t image, std::uint64_t generation, std::uint8_t group,
                                                             NativeObjectGroupProperty field, const NativeObjectGroupVectorBits& desired, bool& modified,
                                                             CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        return Change(image, generation, group, field, desired, Kind::Vector, modified, owner, afterApplication);
    }
    ClockResult NativeObjectGroupPropertiesDomain::Change(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, NativeObjectGroupProperty field,
                                                          const NativeObjectGroupVectorBits& desired, Kind kind, bool& modified, CheckOwner owner,
                                                          CheckOwner afterApplication) noexcept
    {
        using namespace ObjectPropertiesDomainDetail;
        Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load() || bulkInProgress)
            return ClockResult::RestoreFailure;
        if ((kind == Kind::Float && !Scalar(field)) || (kind == Kind::Raw && !Raw(field)) || (kind == Kind::Vector && !Vector(field)) ||
            (kind == Kind::Raw && Boolean(field) && desired[0] > 1))
            return ClockResult::InvalidArgument;
        const bool resume = vectorChange.active;
        if (resume && (kind != Kind::Vector || vectorChange.group != group || vectorChange.first != field || vectorChange.desired != desired))
            return ClockResult::RestoreFailure;
        NativeObjectGroupPropertiesMemory::Value current;
        auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return result;
        const auto first = static_cast<std::size_t>(field);
        bool       unchanged = false;
        if (kind == Kind::Float)
            unchanged = MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(Float(current.bits[first]), Float(desired[0]));
        else if (kind == Kind::Raw)
            unchanged = Boolean(field) ? MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(current.bits[first] != 0, desired[0] != 0)
                                       : MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(current.bits[first], desired[0]);
        else
        {
            const NativeObjectGroupVectorBits before{current.bits[first], current.bits[first + 1], current.bits[first + 2]};
            // Use the actual SDK vector equality, including its component
            // epsilon and NaN behavior. The native store still receives bits.
            unchanged = MtaGame::CObjectGroupPropertiesPolicy::UnchangedValue(Vector(before), Vector(desired));
        }
        if (!resume && unchanged)
            return ClockResult::Ok;
        auto found = originals.find(group);
        if (resume && found == originals.end())
            return ClockResult::RestoreFailure;
        if (found != originals.end() && !NativeObjectGroupPropertiesMemory::SameIdentity(found->second.origin, current))
            return ClockResult::BindingChanged;
        if (resume)
            for (std::size_t axis = 0; axis != desired.size(); ++axis)
                if ((vectorChange.acknowledged & (1u << axis)) && current.bits[first + axis] != found->second.lastOwned[first + axis])
                    return ClockResult::OwnerConflict;
        if ((modified && found == originals.end()) || (found != originals.end() && found->second.acknowledged && found->second.acknowledged != All))
            return ClockResult::RestoreFailure;
        const bool wasModified = modified;
        try
        {
            MtaGame::CObjectGroupPropertiesPolicy::ChangeSafeguard(
                modified,
                [&]
                {
                    if (found == originals.end())
                        found = originals.emplace(group, Saved{NativeObjectGroupPropertiesMemory::Identity(current), current.bits, current.bits, 0}).first;
                });
        }
        catch (...)
        {
            // Allocation precedes native entry. Roll back this facade's CPU
            // flag without losing an older original or an accepted effect.
            modified = wasModified;
            PublishPending();
            return ClockResult::ReadFailure;
        }
        auto& saved = found->second;
        saved.acknowledged = 0;
        if (kind == Kind::Vector && !resume)
            vectorChange = {true, group, 0, field, desired};
        PublishPending();
        const std::size_t count = kind == Kind::Vector ? 3 : 1;
        for (std::size_t axis = 0; axis != count; ++axis)
        {
            if (kind == Kind::Vector && (vectorChange.acknowledged & (1u << axis)))
                continue;
            // Every axis is a new immediate borrow. A previous accepted axis
            // is retained as intent and owned bits, never as a cached Value.
            result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeObjectGroupPropertiesMemory::SameIdentity(saved.origin, current))
                return ClockResult::BindingChanged;
            if (kind == Kind::Vector)
                for (std::size_t prior = 0; prior != desired.size(); ++prior)
                    if ((vectorChange.acknowledged & (1u << prior)) && current.bits[first + prior] != saved.lastOwned[first + prior])
                        return ClockResult::OwnerConflict;
            const auto index = first + axis;
            const auto selected = static_cast<Field>(index);
            // The classic enum comparison used the full request above. Only
            // the genuine native byte store narrows it, preserving cache and
            // modified-flag behavior even when the stored bits already match.
            const auto bits = P::Descriptors[index].bytes == 1 ? desired[axis] & 0xffu : desired[axis];
            bool       accepted = false;
            result = Write(image, current, selected, bits, owner, afterApplication, accepted);
            if (accepted)
                saved.lastOwned[index] = bits;
            if (result != ClockResult::Ok)
                return result;
            if (kind == Kind::Vector)
                vectorChange.acknowledged = static_cast<std::uint8_t>(vectorChange.acknowledged | (1u << axis));
        }
        if (kind == Kind::Vector)
            vectorChange = {};
        PublishPending();
        return ClockResult::Ok;
    }
    ClockResult NativeObjectGroupPropertiesDomain::Restore(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, Saved& saved, CheckOwner owner,
                                                           CheckOwner afterApplication) noexcept
    {
        using namespace ObjectPropertiesDomainDetail;
        if (saved.origin.image != image || saved.origin.generation != generation || saved.origin.group != group)
            return ClockResult::BindingChanged;
        for (std::size_t index = 0; index != P::PropertyCount; ++index)
        {
            NativeObjectGroupPropertiesMemory::Value current;
            auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeObjectGroupPropertiesMemory::SameIdentity(saved.origin, current))
                return ClockResult::BindingChanged;
            const auto bit = std::uint32_t{1} << index;
            // Group RestoreDefault intentionally differs from model Time/LOD
            // ownership journals: classic restores every unacknowledged field
            // from its retained first copy even if another facade edited it.
            // A previously accepted prefix is skipped, including later field
            // drift; replaying it would change the established restore policy.
            // The fresh current raw row still seals every actual native CAS.
            if (saved.acknowledged & bit)
                continue;
            bool accepted = false;
            result = Write(image, current, static_cast<Field>(index), saved.bits[index], owner, afterApplication, accepted);
            if (accepted)
                saved.lastOwned[index] = saved.bits[index];
            if (result != ClockResult::Ok)
                return result;
            saved.acknowledged |= bit;
        }
        if (vectorChange.active && vectorChange.group == group)
            vectorChange = {};
        return ClockResult::Ok;
    }
    ClockResult NativeObjectGroupPropertiesDomain::RestoreOne(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, bool& modified,
                                                              CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        auto result = ObjectPropertiesDomainDetail::Admit(image, generation, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        if (group >= NativeObjectGroupPropertiesProfile::BuiltinRows)
            return ClockResult::InvalidArgument;
        const bool restored =
            MtaGame::CObjectGroupPropertiesPolicy::RestoreDefault(modified,
                                                                  [&]() noexcept
                                                                  {
                                                                      const auto found = originals.find(group);
                                                                      if (found == originals.end())
                                                                      {
                                                                          result = ClockResult::RestoreFailure;
                                                                          return false;
                                                                      }
                                                                      if (!bulkInProgress && found->second.acknowledged == ObjectPropertiesDomainDetail::All)
                                                                          found->second.acknowledged = 0;
                                                                      result = Restore(image, generation, group, found->second, owner, afterApplication);
                                                                      return result == ClockResult::Ok;
                                                                  });
        PublishPending();
        return restored ? ClockResult::Ok : result;
    }
    ClockResult NativeObjectGroupPropertiesDomain::RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner owner,
                                                              CheckOwner afterApplication) noexcept
    {
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        auto result = ObjectPropertiesDomainDetail::Admit(image, generation, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        if (!bulkInProgress)
        {
            for (auto& entry : originals)
                entry.second.acknowledged = 0;
            bulkInProgress = true;
        }
        // This drains only the admitted fields of existing native rows. The
        // unresolved FX ownership and cached-wrapper factory remain outside
        // this private cleanup; no whole-row copy or fake CGame reset is used.
        const bool completed = MtaGame::CObjectGroupPropertiesPolicy::RestoreAll(originals,
                                                                                 [&](std::uint8_t group, Saved& saved) noexcept
                                                                                 {
                                                                                     result = Restore(image, generation, group, saved, owner, afterApplication);
                                                                                     return result == ClockResult::Ok;
                                                                                 });
        if (completed)
        {
            bulkInProgress = false;
            vectorChange = {};
        }
        PublishPending();
        return completed ? ClockResult::Ok : result;
    }
}
