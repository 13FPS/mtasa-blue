/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain first scalar defaults independently of each SDK facade's flag
 ****************************************************************************/
#include "NativeObjectGroupPropertiesDomain.h"
#include "NativeObjectGroupPropertiesProfile.h"
#include <game/CObjectGroupPropertiesPolicy.h>
#include <cstring>

namespace mta::game_de
{
    namespace ObjectPropertiesDomainDetail
    {
        constexpr std::uint8_t All = 0x1f;
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
        pending.store(!originals.empty() || uncertain.load());
    }
    ClockResult NativeObjectGroupPropertiesDomain::Write(std::uintptr_t image, const NativeObjectGroupPropertiesMemory::Value& current,
                                                         NativeObjectGroupProperty field, std::uint32_t desired, CheckOwner owner,
                                                         CheckOwner afterApplication) noexcept
    {
        NativeObjectGroupPropertiesMemory::Receipt receipt;
        auto result = NativeObjectGroupPropertiesMemory::Write(image, current, field, desired, owner, afterApplication, receipt);
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
        const auto index = static_cast<std::size_t>(field);
        if (index >= NativeObjectGroupPropertiesProfile::Fields)
            return ClockResult::InvalidArgument;
        NativeObjectGroupPropertiesMemory::Value current;
        const auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return result;
        output = ObjectPropertiesDomainDetail::Float(current.bits[index]);
        return ClockResult::Ok;
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
        ObjectPropertiesDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load() || bulkInProgress)
            return ClockResult::RestoreFailure;
        const auto index = static_cast<std::size_t>(field);
        if (index >= NativeObjectGroupPropertiesProfile::Fields)
            return ClockResult::InvalidArgument;
        NativeObjectGroupPropertiesMemory::Value current;
        auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return result;
        if (MtaGame::CObjectGroupPropertiesPolicy::UnchangedFloat(ObjectPropertiesDomainDetail::Float(current.bits[index]), desired))
            return ClockResult::Ok;
        auto found = originals.find(group);
        if (found != originals.end() && !NativeObjectGroupPropertiesMemory::SameIdentity(found->second.origin, current))
            return ClockResult::BindingChanged;
        if ((modified && found == originals.end()) ||
            (found != originals.end() && found->second.acknowledged && found->second.acknowledged != ObjectPropertiesDomainDetail::All))
            return ClockResult::RestoreFailure;
        const bool wasModified = modified;
        try
        {
            MtaGame::CObjectGroupPropertiesPolicy::ChangeSafeguard(
                modified,
                [&]
                {
                    if (found == originals.end())
                        found = originals.emplace(group, Saved{NativeObjectGroupPropertiesMemory::Identity(current), current.bits, 0}).first;
                });
        }
        catch (...)
        {
            // The classic flag precedes first-copy allocation. A qualified DE
            // pre-entry allocation refusal may roll back only that CPU flag;
            // no native effect or existing original is discarded here.
            modified = wasModified;
            PublishPending();
            return ClockResult::ReadFailure;
        }
        found->second.acknowledged = 0;
        PublishPending();
        result = Write(image, current, field, ObjectPropertiesDomainDetail::Bits(desired), owner, afterApplication);
        PublishPending();
        return result;
    }
    ClockResult NativeObjectGroupPropertiesDomain::Restore(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, Saved& saved, CheckOwner owner,
                                                           CheckOwner afterApplication) noexcept
    {
        if (saved.origin.image != image || saved.origin.generation != generation || saved.origin.group != group)
            return ClockResult::BindingChanged;
        for (std::size_t index = 0; index < NativeObjectGroupPropertiesProfile::Fields; ++index)
        {
            const auto bit = static_cast<std::uint8_t>(1u << index);
            if (saved.acknowledged & bit)
                continue;
            NativeObjectGroupPropertiesMemory::Value current;
            auto result = NativeObjectGroupPropertiesMemory::Capture(image, generation, group, owner, afterApplication, current);
            if (result != ClockResult::Ok)
                return result;
            if (!NativeObjectGroupPropertiesMemory::SameIdentity(saved.origin, current))
                return ClockResult::BindingChanged;
            result = Write(image, current, static_cast<NativeObjectGroupProperty>(index), saved.bits[index], owner, afterApplication);
            if (result != ClockResult::Ok)
                return result;
            saved.acknowledged = static_cast<std::uint8_t>(saved.acknowledged | bit);
        }
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
        // This is private admitted-field cleanup, not CGame's cached-wrapper
        // bulk contract. The latter remains unadmitted until its actual SDK
        // factory/flag ownership is qualified; no whole-row/FX restore occurs.
        const bool completed = MtaGame::CObjectGroupPropertiesPolicy::RestoreAll(originals,
                                                                                 [&](std::uint8_t group, Saved& saved) noexcept
                                                                                 {
                                                                                     result = Restore(image, generation, group, saved, owner, afterApplication);
                                                                                     return result == ClockResult::Ok;
                                                                                 });
        if (completed)
            bulkInProgress = false;
        PublishPending();
        return completed ? ClockResult::Ok : result;
    }
}
