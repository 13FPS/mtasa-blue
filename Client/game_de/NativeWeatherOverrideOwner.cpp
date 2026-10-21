/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Persistent native overrides with narrow rollback and retained debt
 *****************************************************************************/
#include "NativeWeatherOverrideOwner.h"
#include <cmath>

namespace mta::game_de
{
    namespace
    {
        using Atomic = NativeWeatherOverridePlatform;
        namespace Profile = NativeWeatherOverrideProfile;
        namespace Windows = NativeWeatherOverrideAtomicProfile;
        void FirstFailure(ClockResult candidate, ClockResult& result) noexcept
        {
            if (result == ClockResult::Ok && candidate != ClockResult::Ok)
                result = candidate;
        }
    }
    NativeWeatherOverrideCodeView NativeWeatherOverrideOwner::CodeView() const noexcept
    {
        std::array<const Profile::Ledger*, 8> ledgers{};
        for (std::size_t index = 0; index < groups.size(); ++index)
            if (groups[index].ledger)
                ledgers[index] = &*groups[index].ledger;
        return NativeWeatherOverrideCodeView(ledgers, generation, platform.Reader());
    }
    bool NativeWeatherOverrideOwner::Admits(Field field) const noexcept
    {
        return Windows::Supports(field, platform.CompareExchange128());
    }
    ClockResult NativeWeatherOverrideOwner::Bind(const Binding& qualified, std::uint64_t ownerGeneration) noexcept
    {
        if (!ownerGeneration || !qualified.image || qualified.image % 16 || !platform.Reader())
            return ClockResult::InvalidArgument;
        if (HasReferences() || NeedsRecovery())
            return ClockResult::Busy;
        const auto checked = host.Check(ownerGeneration, false);
        if (checked != ClockResult::Ok)
            return checked;
        const std::array<const Profile::Ledger*, 8> empty{};
        if (!platform.Revalidate(qualified, NativeWeatherOverrideCodeView(empty, ownerGeneration, platform.Reader())))
            return ClockResult::BindingChanged;
        for (auto& group : groups)
            group.ledger.reset();
        binding = qualified;
        generation = ownerGeneration;
        superseded.store(0);
        return ClockResult::Ok;
    }
    ClockResult NativeWeatherOverrideOwner::Admit(bool recovering) noexcept
    {
        if (!generation)
            return ClockResult::NotInitialized;
        const auto checked = host.Check(generation, recovering);
        if (checked != ClockResult::Ok)
            return checked;
        return platform.Revalidate(binding, CodeView()) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
    void NativeWeatherOverrideOwner::Publish(bool failed) noexcept
    {
        bool retained = false;
        for (const auto& group : groups)
        {
            retained |= group.scratch != 0 || group.scalarPending || (group.ledger && group.ledger->HasPending());
            for (const auto& site : group.sites)
                retained |= site.protectionPending || site.cachePending;
        }
        references.store(retained);
        recovery.store(failed && retained);
    }
    ClockResult NativeWeatherOverrideOwner::Prepare(Field field, Group& group) noexcept
    {
        if (group.ledger)
            return ClockResult::Ok;
        Profile::Range range;
        auto           result = Profile::Plan::AccumulatorRange(binding, field, range);
        if (result != ClockResult::Ok)
            return result;
        group.scratch = platform.AllocateNear(range);
        if (!group.scratch)
            return ClockResult::WriteFailure;
        if (!platform.ScratchValid(group.scratch))
            result = ClockResult::WriteFailure;
        Profile::Plan plan;
        if (result == ClockResult::Ok)
            result = Profile::Plan::Prepare(binding, field, group.scratch, generation, platform.Reader(), plan);
        if (result == ClockResult::Ok)
        {
            // Preparing a seven-group logical plan does not prove an atomic
            // mutation. Every actual site must have the stronger window proof.
            if (plan.Count() > group.sites.size())
                result = ClockResult::UnsupportedBuild;
            for (std::size_t index = 0; result == ClockResult::Ok && index < plan.Count(); ++index)
            {
                const auto*   entry = plan.At(index);
                const auto*   window = Windows::Find(entry->writer.rva);
                Atomic::Bytes desired{}, actual{};
                if (!window || (binding.image + window->windowRva) % window->windowSize || !Windows::Build(*window, *entry, desired))
                    result = ClockResult::UnsupportedBuild;
                else if (!platform.Reader()(binding.image + window->windowRva, actual.data(), window->windowSize))
                    result = ClockResult::ReadFailure;
                else if (actual != window->stock)
                    result = ClockResult::BindingChanged;
            }
        }
        if (result == ClockResult::Ok)
            group.ledger.emplace(plan);
        else if (platform.ReleaseScratch(group.scratch))
            group.scratch = 0;
        Publish(result != ClockResult::Ok);
        return result;
    }
    ClockResult NativeWeatherOverrideOwner::Patch(Group& group, bool redirect) noexcept
    {
        if (!group.ledger)
            return ClockResult::Ok;
        auto&       ledger = *group.ledger;
        ClockResult result = ClockResult::Ok;
        for (std::size_t index = 0; index < ledger.Prepared().Count(); ++index)
        {
            auto checked = host.Check(generation, !redirect);
            if (checked != ClockResult::Ok)
            {
                FirstFailure(checked, result);
                break;
            }
            const auto*   entry = ledger.Prepared().At(index);
            const auto*   window = Windows::Find(entry->writer.rva);
            Atomic::Bytes redirected{}, observed{};
            if (!window || !Windows::Build(*window, *entry, redirected))
                return ClockResult::UnsupportedBuild;
            const auto  address = binding.image + window->windowRva;
            const auto& desired = redirect ? redirected : window->stock;
            const auto& opposite = redirect ? window->stock : redirected;
            auto&       site = group.sites[index];
            if (!platform.Reader()(address, observed.data(), window->windowSize))
                checked = ClockResult::ReadFailure;
            else if (observed != desired && observed != opposite)
                checked = ClockResult::OwnerConflict;
            else if (ledger.Observe(index, generation, platform.Reader()) !=
                     (observed == redirected ? Profile::Observation::Redirected : Profile::Observation::Stock))
                checked = ClockResult::BindingChanged;
            if (checked != ClockResult::Ok)
            {
                FirstFailure(checked, result);
                if (redirect)
                    break;
                continue;
            }
            if (redirect && !ledger.RetainPossiblePatch(index, generation))
                return ClockResult::InvalidArgument;
            if (observed != desired)
            {
                // Retain before the first possible native reference. Unlike
                // a later scalar value, foreign code cannot retire this debt.
                if (!ledger.RetainPossiblePatch(index, generation))
                    return ClockResult::InvalidArgument;
                std::uint32_t previous = 0;
                if (!platform.OpenCode(binding, address, window->windowSize, previous))
                    checked = ClockResult::PatchFailure;
                else
                {
                    if (!site.protectionPending)
                        site.protection = previous;
                    site.protectionPending = true;
                    // Page/protection helpers can run arbitrary OS work. The
                    // original grant cannot be cached across those calls.
                    checked = host.Check(generation, !redirect);
                    if (checked == ClockResult::Ok)
                    {
                        site.cachePending = true;
                        const auto exchange = platform.ExchangeCode(binding, address, window->windowSize, observed, desired);
                        if (exchange != Atomic::Exchange::Exchanged)
                            checked = exchange == Atomic::Exchange::Conflict ? ClockResult::OwnerConflict : ClockResult::PatchFailure;
                    }
                    // Exchange is indivisible even if this separate read or
                    // cache/protection acknowledgement fails. Every possible
                    // instruction remains wholly stock or wholly redirected;
                    // the accumulator remains owned until recovery proves it
                    // cannot be referenced by either old or current code.
                    Atomic::Bytes after{};
                    if (!platform.Reader()(address, after.data(), window->windowSize))
                        FirstFailure(ClockResult::ReadFailure, checked);
                    else if (after != desired)
                        FirstFailure(after == opposite ? ClockResult::PatchFailure : ClockResult::OwnerConflict, checked);
                }
            }
            if (site.cachePending && platform.Flush(address, window->windowSize))
                site.cachePending = false;
            if (site.protectionPending && platform.CloseCode(binding, address, window->windowSize, site.protection))
                site.protectionPending = false;
            if (site.protectionPending || site.cachePending)
                FirstFailure(ClockResult::RestoreFailure, checked);
            if (!redirect && !site.protectionPending && !site.cachePending)
                FirstFailure(ledger.ClearIfStock(index, generation, platform.Reader()), checked);
            FirstFailure(checked, result);
            if (redirect && result != ClockResult::Ok)
                break;
        }
        return result;
    }
    ClockResult NativeWeatherOverrideOwner::RestoreScalar(Field field, Group& group) noexcept
    {
        if (!group.scalarPending)
            return ClockResult::Ok;
        std::uint32_t observed = 0;
        if (!platform.ReadScalar(binding, field, observed))
            return ClockResult::ReadFailure;
        if (observed == group.original)
        {
            group.scalarPending = false;
            return ClockResult::Ok;
        }
        if (observed != group.replacement)
        {
            // Later native progression or another qualified owner supersedes
            // only this scalar. Preserve its value and report the conflict;
            // it does not prove that retained code no longer refers to us.
            group.scalarPending = false;
            superseded.fetch_add(1);
            return ClockResult::OwnerConflict;
        }
        const auto checked = host.Check(generation, true);
        if (checked != ClockResult::Ok)
            return checked;
        const auto exchange = platform.ExchangeScalar(binding, field, group.replacement, group.original);
        if (exchange == Atomic::Exchange::Unavailable)
            return ClockResult::RestoreFailure;
        if (!platform.ReadScalar(binding, field, observed))
            return ClockResult::ReadFailure;
        if (observed == group.original)
        {
            group.scalarPending = false;
            return ClockResult::Ok;
        }
        if (observed != group.replacement)
        {
            group.scalarPending = false;
            superseded.fetch_add(1);
            return ClockResult::OwnerConflict;
        }
        return ClockResult::RestoreFailure;
    }
    ClockResult NativeWeatherOverrideOwner::Retire(Field field, Group& group) noexcept
    {
        group.active = false;
        auto result = RestoreScalar(field, group);
        FirstFailure(Patch(group, false), result);
        bool retained = group.scalarPending || (group.ledger && group.ledger->HasPending());
        for (const auto& site : group.sites)
            retained |= site.protectionPending || site.cachePending;
        if (!retained)
        {
            if (group.scratch && !platform.ReleaseScratch(group.scratch))
                FirstFailure(ClockResult::RestoreFailure, result);
            else
            {
                group.scratch = 0;
                group.ledger.reset();
            }
        }
        return result;
    }
    ClockResult NativeWeatherOverrideOwner::Finish(ClockResult result) noexcept
    {
        // The platform has no partial code-write operation, so it never needs
        // an unsafe return gate. Failed OS resumes remain the existing host's
        // peer-handle debt; this owner retains all its own unresolved resources.
        if (!host.FinishPeers(true))
            result = ClockResult::RestoreFailure;
        Publish(result != ClockResult::Ok);
        return result;
    }
    ClockResult NativeWeatherOverrideOwner::Set(Field field, float value) noexcept
    {
        if (static_cast<std::size_t>(field) >= groups.size() || !std::isfinite(value))
            return ClockResult::InvalidArgument;
        if (!Admits(field))
            return ClockResult::UnsupportedBuild;
        if (NeedsRecovery())
            return ClockResult::Busy;
        auto result = Admit(false);
        if (result != ClockResult::Ok)
        {
            // Existing active redirects still own scratch/code lifetime when
            // validation fails. Publish that recovery debt before returning a
            // refusal, so the borrowed runtime cannot treat it as ordinary idle.
            Publish(true);
            return result;
        }
        auto& group = groups[static_cast<std::size_t>(field)];
        result = Prepare(field, group);
        if (result != ClockResult::Ok)
            return result;
        result = host.ParkPeers(Windows::Windows.data(), Windows::Windows.size());
        if (result != ClockResult::Ok)
            return Finish(result);
        if (!host.PeersHeld())
            return Finish(ClockResult::Busy);
        result = Admit(false);
        if (result == ClockResult::Ok)
            result = Patch(group, true);
        if (result == ClockResult::Ok)
        {
            if (!platform.ReadScalar(binding, field, group.original))
                result = ClockResult::ReadFailure;
            else
            {
                std::memcpy(&group.replacement, &value, sizeof(value));
                group.scalarPending = true;
                result = host.Check(generation, false);
                if (result == ClockResult::Ok)
                {
                    const auto exchange = platform.ExchangeScalar(binding, field, group.original, group.replacement);
                    if (exchange != Atomic::Exchange::Exchanged)
                        result = exchange == Atomic::Exchange::Conflict ? ClockResult::OwnerConflict : ClockResult::WriteFailure;
                }
                std::uint32_t observed = 0;
                if (!platform.ReadScalar(binding, field, observed))
                    FirstFailure(ClockResult::ReadFailure, result);
                else if (observed != group.replacement)
                    FirstFailure(ClockResult::WriteFailure, result);
                if (result == ClockResult::Ok)
                    result = Admit(false);
                if (result == ClockResult::Ok)
                {
                    // A successful SDK setter is persistent engine intent.
                    // Reset/End restores writers, never this accepted value.
                    group.scalarPending = false;
                    group.active = true;
                }
            }
        }
        if (result != ClockResult::Ok)
        {
            const auto authority = host.Check(generation, true);
            if (authority == ClockResult::Ok)
                FirstFailure(Retire(field, group), result);
        }
        return Finish(result);
    }
    ClockResult NativeWeatherOverrideOwner::Reset(Field field) noexcept
    {
        if (static_cast<std::size_t>(field) >= groups.size())
            return ClockResult::InvalidArgument;
        if (!Admits(field))
            return ClockResult::UnsupportedBuild;
        auto result = Admit(true);
        if (result != ClockResult::Ok)
        {
            Publish(true);
            return result;
        }
        result = host.ParkPeers(Windows::Windows.data(), Windows::Windows.size());
        if (result != ClockResult::Ok)
            return Finish(result);
        if (!host.PeersHeld())
            return Finish(ClockResult::Busy);
        result = Admit(true);
        if (result == ClockResult::Ok)
            result = Retire(field, groups[static_cast<std::size_t>(field)]);
        return Finish(result);
    }
    ClockResult NativeWeatherOverrideOwner::RestoreAll() noexcept
    {
        if (!generation || !HasReferences())
            return ClockResult::Ok;
        auto result = Admit(true);
        if (result != ClockResult::Ok)
        {
            Publish(true);
            return result;
        }
        result = host.ParkPeers(Windows::Windows.data(), Windows::Windows.size());
        if (result != ClockResult::Ok)
            return Finish(result);
        if (!host.PeersHeld())
            return Finish(ClockResult::Busy);
        result = Admit(true);
        if (result == ClockResult::Ok)
            for (std::size_t index = 0; index < groups.size(); ++index)
                FirstFailure(Retire(static_cast<Field>(index), groups[index]), result);
        return Finish(result);
    }
}
