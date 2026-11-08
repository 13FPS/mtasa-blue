/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve timed-model originals, alias identity and reset prefixes
 ****************************************************************************/
#include "NativeModelTimeDomain.h"
#include "NativeModelTimeProfile.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <game/CModelTimePolicy.h>
#include <cstring>
namespace mta::game_de
{
    namespace ModelTimeDomainDetail
    {
        struct Guard
        {
            std::atomic_flag&  operation;
            std::atomic<bool>& active;
            bool               held;
            Guard(std::atomic_flag& operation, std::atomic<bool>& active) noexcept : operation(operation), active(active), held(!operation.test_and_set())
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
        std::uint16_t Hours(const std::array<unsigned char, 4>& tuple) noexcept
        {
            std::uint16_t hours = 0;
            std::memcpy(&hours, tuple.data(), sizeof(hours));
            return hours;
        }
        ClockResult Admit(std::uintptr_t image, std::uint64_t generation, NativeModelTimeDomain::CheckOwner owner,
                          NativeModelTimeDomain::CheckOwner after) noexcept
        {
            if (!image || !generation || !owner || !after)
                return ClockResult::InvalidArgument;
            auto result = owner();
            if (result != ClockResult::Ok || (result = after()) != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(image, state))
                return ClockResult::BindingChanged;
            return state == 9 ? ClockResult::Ok : ClockResult::NotInitialized;
        }
    }
    void NativeModelTimeDomain::Publish() noexcept
    {
        pending.store(!originals.empty() || uncertain.load());
    }
    ClockResult NativeModelTimeDomain::Write(std::uintptr_t image, const NativeModelTimeMemory::Value& value, std::uint16_t hours, CheckOwner owner,
                                             CheckOwner after, Exchange exchange, bool& accepted) noexcept
    {
        NativeModelTimeMemory::Receipt receipt;
        auto                           result = NativeModelTimeMemory::Write(image, value, hours, owner, after, receipt, exchange);
        accepted = receipt.accepted;
        if (result == ClockResult::Ok && (!receipt.entered || !receipt.accepted || receipt.uncertain))
            result = ClockResult::WriteFailure;
        if (receipt.uncertain || (receipt.entered && receipt.accepted && result != ClockResult::Ok))
        {
            if (!uncertain.load())
            {
                firstUnknown.origin = NativeModelTimeMemory::Identity(receipt.before);
                firstUnknown.desired = receipt.desired;
                firstUnknown.observed = receipt.observed;
                firstUnknown.exceptionCode = receipt.exceptionCode;
                firstUnknown.result = result;
                firstUnknown.entered = receipt.entered;
                firstUnknown.accepted = receipt.accepted;
                uncertain.store(true);
            }
            pending.store(true);
        }
        return result;
    }
    ClockResult NativeModelTimeDomain::Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, std::uint16_t hours, CheckOwner owner,
                                           CheckOwner after, bool& applied, Exchange exchange) noexcept
    {
        ModelTimeDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        NativeModelTimeMemory::Value current;
        auto                         result = NativeModelTimeMemory::Capture(image, generation, model, owner, after, current);
        if (result != ClockResult::Ok)
            return result;
        if (!current.timed)
        {
            applied = false;
            return ClockResult::Ok;
        }
        const auto key = current.model.nativeModelInfo + NativeModelTimeProfile::TimeOffset;
        auto       saved = originals.find(key);
        // An alias can select the same actual time-info tuple, but cannot
        // replace the first default or overwrite externally changed metadata.
        if (saved != originals.end() && (!NativeModelTimeMemory::SameStorageIdentity(saved->second.origin, current) ||
                                         ModelTimeDomainDetail::Hours(current.tuple) != saved->second.lastOwned))
            return ClockResult::OwnerConflict;
        try
        {
            const bool completed = MtaGame::CModelTimePolicy::Set(
                hours,
                [&](std::uint16_t& value) noexcept
                {
                    value = ModelTimeDomainDetail::Hours(current.tuple);
                    return true;
                },
                [&](std::uint16_t original)
                {
                    if (saved == originals.end())
                        saved = originals.emplace(key, Saved{NativeModelTimeMemory::Identity(current), current.tuple, model, original, false}).first;
                    // First-default retention precedes even an unchanged CAS,
                    // exactly as classic SetTime remembers a same-hours call.
                    Publish();
                    return true;
                },
                [&](std::uint16_t requested) noexcept
                {
                    bool accepted = false;
                    result = Write(image, current, requested, owner, after, exchange, accepted);
                    if (accepted)
                    {
                        saved->second.lastOwned = requested;
                        saved->second.route = model;
                        saved->second.restored = false;
                    }
                    return result == ClockResult::Ok;
                });
            Publish();
            if (completed)
                applied = true;
            return result;
        }
        catch (...)
        {
            // Map allocation precedes native entry. Its failure cannot erase
            // earlier first defaults or be acknowledged as a completed store.
            Publish();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeModelTimeDomain::ResetAll(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, CheckOwner after, Exchange exchange) noexcept
    {
        ModelTimeDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        auto result = ModelTimeDomainDetail::Admit(image, generation, owner, after);
        if (result != ClockResult::Ok)
            return result;
        const bool completed = MtaGame::CModelTimePolicy::ResetAll(
            originals,
            [&](std::uintptr_t key, Saved& saved) noexcept
            {
                NativeModelTimeMemory::Value current;
                result = NativeModelTimeMemory::Capture(image, generation, saved.route, owner, after, current);
                if (result != ClockResult::Ok)
                    return false;
                // A disappeared/retargeted bank route cannot certify that the
                // original static time-info row is still this game's object.
                // Keep the default reachable instead of dropping that debt.
                if (!current.timed || current.model.nativeModelInfo + NativeModelTimeProfile::TimeOffset != key ||
                    !NativeModelTimeMemory::SameStorageIdentity(saved.origin, current) || ModelTimeDomainDetail::Hours(current.tuple) != saved.lastOwned)
                {
                    result = ClockResult::OwnerConflict;
                    return false;
                }
                if (saved.restored)
                    return true;
                const auto original = ModelTimeDomainDetail::Hours(saved.first);
                bool       accepted = false;
                result = Write(image, current, original, owner, after, exchange, accepted);
                if (accepted)
                    saved.lastOwned = original;
                if (result == ClockResult::Ok)
                    saved.restored = true;
                return result == ClockResult::Ok;
            });
        // Known accepted prefixes stay marked until all entries are restored.
        // Entered-unknown writes remain sticky and are never automatically
        // replayed by Reset, End, a producer drain or a later callback.
        Publish();
        return completed ? ClockResult::Ok : result;
    }
}
