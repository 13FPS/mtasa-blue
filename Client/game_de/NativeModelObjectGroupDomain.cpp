/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse MTA's group cache ordering without losing native restore debt
 ****************************************************************************/
#include "NativeModelObjectGroupDomain.h"
#include <game/CModelObjectGroupPolicy.h>

namespace mta::game_de
{
    namespace ObjectGroupDomainDetail
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
        ClockResult Admit(std::uintptr_t image, std::uint64_t generation, NativeModelObjectGroupMemory::CheckOwner owner,
                          NativeModelObjectGroupMemory::CheckOwner afterApplication) noexcept
        {
            if (!image || !generation || !owner || !afterApplication)
                return ClockResult::InvalidArgument;
            auto result = owner();
            return result == ClockResult::Ok ? afterApplication() : result;
        }
    }
    void NativeModelObjectGroupDomain::PublishPending() noexcept
    {
        pending.store(!originals.empty() || uncertain.load());
    }
    ClockResult NativeModelObjectGroupDomain::Write(std::uintptr_t image, const NativeModelObjectGroupMemory::Value& before, std::uint16_t desired,
                                                    CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        NativeModelObjectGroupMemory::Receipt receipt;
        auto                                  result = NativeModelObjectGroupMemory::Write(image, before, desired, owner, afterApplication, receipt);
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
    ClockResult NativeModelObjectGroupDomain::Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner,
                                                   CheckOwner afterApplication, std::uint16_t& output) noexcept
    {
        ObjectGroupDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        NativeModelObjectGroupMemory::Value current;
        const auto                          result = NativeModelObjectGroupMemory::Capture(image, generation, model, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return result;
        if (!current.present)
            return ClockResult::NotInitialized;
        output = current.group;
        return ClockResult::Ok;
    }
    ClockResult NativeModelObjectGroupDomain::Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, std::uint16_t desired, CheckOwner owner,
                                                  CheckOwner afterApplication) noexcept
    {
        ObjectGroupDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        NativeModelObjectGroupMemory::Value current;
        auto                                result = ClockResult::NotInitialized;
        const bool                          completed = MtaGame::CModelObjectGroupPolicy::Set(
            desired,
            [&](std::uint16_t& observed) noexcept
            {
                result = NativeModelObjectGroupMemory::Capture(image, generation, model, owner, afterApplication, current);
                if (result == ClockResult::Ok && !current.present)
                    result = ClockResult::NotInitialized;
                if (result == ClockResult::Ok)
                    observed = current.group;
                return result == ClockResult::Ok;
            },
            [&](std::uint16_t original) noexcept
            {
                const auto found = originals.find(model);
                if (found != originals.end())
                {
                    if (!NativeModelObjectGroupMemory::SameIdentity(found->second.origin, current))
                        result = ClockResult::BindingChanged;
                    return result == ClockResult::Ok;
                }
                try
                {
                    originals.emplace(model, Saved{NativeModelObjectGroupMemory::Identity(current), original, false});
                    PublishPending();
                    return true;
                }
                catch (...)
                {
                    result = ClockResult::ReadFailure;
                    return false;
                }
            },
            [&](std::uint16_t value) noexcept
            {
                result = Write(image, current, value, owner, afterApplication);
                if (result == ClockResult::Ok)
                    originals.at(model).restored = false;
                return result == ClockResult::Ok;
            });
        PublishPending();
        return completed ? ClockResult::Ok : result;
    }
    ClockResult NativeModelObjectGroupDomain::RestoreOne(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner,
                                                         CheckOwner afterApplication) noexcept
    {
        ObjectGroupDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        auto result = ObjectGroupDomainDetail::Admit(image, generation, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        auto       saved = originals.end();
        const bool completed = MtaGame::CModelObjectGroupPolicy::RestoreOne(
            [&](std::uint16_t& original, bool& found) noexcept
            {
                saved = originals.find(model);
                found = saved != originals.end();
                if (found)
                {
                    if (saved->second.origin.generation != generation)
                    {
                        result = ClockResult::BindingChanged;
                        return false;
                    }
                    original = saved->second.group;
                }
                return true;
            },
            [&](std::uint16_t original) noexcept
            {
                if (saved->second.restored)
                    return true;
                NativeModelObjectGroupMemory::Value current;
                result = NativeModelObjectGroupMemory::Capture(image, generation, model, owner, afterApplication, current);
                if (result == ClockResult::Ok && !current.present)
                    result = ClockResult::NotInitialized;
                if (result == ClockResult::Ok && !NativeModelObjectGroupMemory::SameIdentity(saved->second.origin, current))
                    result = ClockResult::BindingChanged;
                if (result == ClockResult::Ok)
                    result = Write(image, current, original, owner, afterApplication);
                return result == ClockResult::Ok;
            },
            [&]() noexcept { originals.erase(saved); });
        PublishPending();
        return completed ? ClockResult::Ok : result;
    }
    ClockResult NativeModelObjectGroupDomain::RestoreAll(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        ObjectGroupDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        auto result = ObjectGroupDomainDetail::Admit(image, generation, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        const bool completed = MtaGame::CModelObjectGroupPolicy::RestoreAll(originals,
                                                                            [&](std::uint32_t model, Saved& saved) noexcept
                                                                            {
                                                                                if (saved.origin.generation != generation)
                                                                                {
                                                                                    result = ClockResult::BindingChanged;
                                                                                    return false;
                                                                                }
                                                                                if (saved.restored)
                                                                                    return true;
                                                                                NativeModelObjectGroupMemory::Value current;
                                                                                result = NativeModelObjectGroupMemory::Capture(image, generation, model, owner,
                                                                                                                               afterApplication, current);
                                                                                if (result != ClockResult::Ok)
                                                                                    return false;
                                                                                if (!current.present)
                                                                                {
                                                                                    // Classic's bulk restore skips an absent metadata row.
                                                                                    // A fresh null bank association supplies only that skip.
                                                                                    saved.restored = true;
                                                                                    return true;
                                                                                }
                                                                                if (!NativeModelObjectGroupMemory::SameIdentity(saved.origin, current))
                                                                                {
                                                                                    result = ClockResult::BindingChanged;
                                                                                    return false;
                                                                                }
                                                                                result = Write(image, current, saved.group, owner, afterApplication);
                                                                                if (result == ClockResult::Ok)
                                                                                    saved.restored = true;
                                                                                return result == ClockResult::Ok;
                                                                            });
        // Unlike single restore, a bulk reset retains all originals until its
        // complete traversal acknowledges. A later explicit retry can skip a
        // known accepted prefix; an entered-unknown store is never replayed.
        PublishPending();
        return completed ? ClockResult::Ok : result;
    }
}
