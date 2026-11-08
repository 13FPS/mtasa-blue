/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA wheel defaults with current native identities and sticky debt
 *****************************************************************************/
#include "NativeModelWheelDomain.h"
#include <game/CModelWheelSizePolicy.h>
#include <cstring>

namespace mta::game_de
{
    namespace WheelDomainDetail
    {
        struct Guard
        {
            std::atomic_flag&  operation;
            std::atomic<bool>& active;
            bool               held;
            Guard(std::atomic_flag& flag, std::atomic<bool>& activity) noexcept : operation(flag), active(activity), held(!flag.test_and_set())
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
        NativeModelWheelSizes Sizes(const std::array<std::uint32_t, 2>& bits) noexcept
        {
            NativeModelWheelSizes value;
            static_assert(sizeof(value.front) == sizeof(bits[0]));
            std::memcpy(&value.front, &bits[0], sizeof(value.front));
            std::memcpy(&value.rear, &bits[1], sizeof(value.rear));
            return value;
        }
        std::array<std::uint32_t, 2> Bits(const NativeModelWheelSizes& value) noexcept
        {
            std::array<std::uint32_t, 2> bits{};
            std::memcpy(&bits[0], &value.front, sizeof(value.front));
            std::memcpy(&bits[1], &value.rear, sizeof(value.rear));
            return bits;
        }
    }
    void NativeModelWheelDomain::PublishPending() noexcept
    {
        pending.store(!defaults.empty() || !retired.empty() || uncertain.load());
    }
    ClockResult NativeModelWheelDomain::Write(std::uintptr_t image, const NativeModelWheelMemory::Value& before, const std::array<std::uint32_t, 2>& desired,
                                              CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        NativeModelWheelMemory::Receipt receipt;
        auto                            result = NativeModelWheelMemory::Write(image, before, desired, owner, afterApplication, receipt);
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
    ClockResult NativeModelWheelDomain::Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner afterApplication,
                                             NativeModelWheelSizes& output) noexcept
    {
        WheelDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        NativeModelWheelMemory::Value current;
        const auto                    result = NativeModelWheelMemory::Capture(image, generation, model, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return result;
        output = current.vehicle ? WheelDomainDetail::Sizes(current.bits) : NativeModelWheelSizes{};
        return ClockResult::Ok;
    }
    ClockResult NativeModelWheelDomain::Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, NativeModelWheelGroup group, float value,
                                            CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        WheelDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        NativeModelWheelMemory::Value current;
        auto                          result = NativeModelWheelMemory::Capture(image, generation, model, owner, afterApplication, current);
        if (result != ClockResult::Ok || !current.vehicle)
            return result;
        if (retired.find(model) != retired.end())
            return ClockResult::RestoreFailure;
        auto found = defaults.find(model);
        if (found != defaults.end() && !NativeModelWheelMemory::SameIdentity(found->second.origin, current))
            return ClockResult::BindingChanged;
        try
        {
            auto& saved = MtaGame::CModelWheelSizePolicy::Remember(defaults, model,
                                                                   [&] { return Saved{NativeModelWheelMemory::Identity(current), current.bits, false}; });
            PublishPending();
            auto          desired = current.bits;
            std::uint32_t raw = 0;
            std::memcpy(&raw, &value, sizeof(raw));
            // Capture the first default even for an ignored wheel group, just
            // like MTA. Every valid group writes raw bits without a new clamp.
            if (!MtaGame::CModelWheelSizePolicy::Apply(
                    static_cast<std::uint32_t>(group), raw, [&](std::uint32_t bits) { desired[0] = bits; }, [&](std::uint32_t bits) { desired[1] = bits; }))
                return ClockResult::Ok;
            result = Write(image, current, desired, owner, afterApplication);
            if (result == ClockResult::Ok)
                saved.restored = false;
            PublishPending();
            return result;
        }
        catch (...)
        {
            // No native call precedes a failed default-map allocation.
            PublishPending();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeModelWheelDomain::ResetOne(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, const NativeModelWheelSizes* supplied,
                                                 CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        WheelDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        NativeModelWheelMemory::Value current;
        auto                          result = NativeModelWheelMemory::Capture(image, generation, model, owner, afterApplication, current);
        if (result != ClockResult::Ok || !current.vehicle)
            return result;
        const auto cached = defaults.find(model);
        auto       old = retired.find(model);
        if ((cached != defaults.end() && !NativeModelWheelMemory::SameIdentity(cached->second.origin, current)) ||
            (old != retired.end() && !NativeModelWheelMemory::SameIdentity(old->second.origin, current)))
            return ClockResult::BindingChanged;
        std::array<std::uint32_t, 2> desired{};
        if (supplied)
        {
            if (old != retired.end())
                return ClockResult::RestoreFailure;
            desired = WheelDomainDetail::Bits(*supplied);
        }
        else
        {
            if (cached == defaults.end() && old == retired.end())
                return ClockResult::Ok;
            if (old == retired.end())
            {
                try
                {
                    old = retired.emplace(model, cached->second).first;
                }
                catch (...)
                {
                    return ClockResult::ReadFailure;
                }
                Saved copied;
                if (!MtaGame::CModelWheelSizePolicy::Take(defaults, model, copied))
                {
                    PublishPending();
                    return ClockResult::OwnerConflict;
                }
            }
            // MTA's default entry is erased before the native store. Preserve
            // its exact intent separately until a DE store is acknowledged;
            // even a known pre-entry refusal must not lose the reset debt.
            desired = old->second.bits;
            PublishPending();
        }
        result = Write(image, current, desired, owner, afterApplication);
        if (result == ClockResult::Ok)
        {
            if (!supplied)
                retired.erase(model);
            else if (cached != defaults.end())
                cached->second.restored = desired == cached->second.bits;
        }
        PublishPending();
        return result;
    }
    ClockResult NativeModelWheelDomain::ResetAll(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        WheelDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        if (!image || !generation || !owner || !afterApplication)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        const auto restore = [&](std::uint32_t model, Saved& saved) noexcept
        {
            if (saved.origin.generation != generation)
            {
                result = ClockResult::BindingChanged;
                return false;
            }
            if (saved.restored)
                return true;
            NativeModelWheelMemory::Value current;
            result = NativeModelWheelMemory::Capture(image, generation, model, owner, afterApplication, current);
            if (result != ClockResult::Ok)
                return false;
            if (!current.vehicle)
            {
                // Classic skips deallocated/nonvehicle models during its bulk
                // reset. This qualified current observation grants no lease.
                saved.restored = true;
                return true;
            }
            if (!NativeModelWheelMemory::SameIdentity(saved.origin, current))
            {
                result = ClockResult::BindingChanged;
                return false;
            }
            result = Write(image, current, saved.bits, owner, afterApplication);
            if (result == ClockResult::Ok)
                saved.restored = true;
            return result == ClockResult::Ok;
        };
        // Acknowledged entries stay alive until all callbacks complete, as in
        // MTA; their CPU journal prevents a later explicit retry from replaying
        // that accepted prefix after another entry's pre-entry refusal.
        if (!MtaGame::CModelWheelSizePolicy::RestoreAll(defaults, restore) || !MtaGame::CModelWheelSizePolicy::RestoreAll(retired, restore))
        {
            PublishPending();
            return result;
        }
        PublishPending();
        return ClockResult::Ok;
    }
}
