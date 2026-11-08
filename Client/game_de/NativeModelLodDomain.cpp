/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve ordered model-ID defaults across shared native LOD storage
 ****************************************************************************/
#include "NativeModelLodDomain.h"
#include "NativeModelBankProfile.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <game/CModelLodDistancePolicy.h>
namespace mta::game_de
{
    namespace ModelLodDomainDetail
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
        ClockResult Admit(std::uintptr_t image, std::uint64_t generation, NativeModelLodDomain::CheckOwner owner,
                          NativeModelLodDomain::CheckOwner after) noexcept
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
    void NativeModelLodDomain::Publish() noexcept
    {
        pending.store(!originals.empty() || !storageOwners.empty() || uncertain.load());
    }
    ClockResult NativeModelLodDomain::Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner after,
                                           std::uint32_t& bits) noexcept
    {
        ModelLodDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        NativeModelLodMemory::Value value;
        const auto                  result = NativeModelLodMemory::Capture(image, generation, model, owner, after, value);
        if (result == ClockResult::Ok)
            bits = value.present ? value.bits : 0;
        return result;
    }
    ClockResult NativeModelLodDomain::ReadOriginal(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner after,
                                                   std::uint32_t& bits) noexcept
    {
        ModelLodDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (model >= NativeModelBankProfile::ModelCount)
            return ClockResult::InvalidArgument;
        const auto result = ModelLodDomainDetail::Admit(image, generation, owner, after);
        if (result != ClockResult::Ok)
            return result;
        const auto found = originals.find(model);
        if (found != originals.end() && found->second.origin.generation != generation)
            return ClockResult::BindingChanged;
        // Classic asks the model-ID cache only, even if its native bank row
        // later disappears. This CPU result neither reads the current distance
        // nor grants write authority to the previously observed metadata.
        bits = found == originals.end() ? 0 : found->second.original;
        return ClockResult::Ok;
    }
    ClockResult NativeModelLodDomain::Write(std::uintptr_t image, const NativeModelLodMemory::Value& value, std::uint32_t bits, CheckOwner owner,
                                            CheckOwner after, Exchange exchange, bool& accepted) noexcept
    {
        NativeModelLodMemory::Receipt receipt;
        auto                          result = NativeModelLodMemory::Write(image, value, bits, owner, after, receipt, exchange);
        accepted = receipt.accepted;
        if (result == ClockResult::Ok && (!receipt.entered || !receipt.accepted || receipt.uncertain))
            result = ClockResult::WriteFailure;
        if (receipt.uncertain || (receipt.entered && receipt.accepted && result != ClockResult::Ok))
        {
            if (!uncertain.load())
            {
                firstUnknown.origin = NativeModelLodMemory::Identity(receipt.before);
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
    ClockResult NativeModelLodDomain::Set(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, std::uint32_t bits, CheckOwner owner,
                                          CheckOwner after, Exchange exchange) noexcept
    {
        ModelLodDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        NativeModelLodMemory::Value current;
        auto                        result = NativeModelLodMemory::Capture(image, generation, model, owner, after, current);
        if (result != ClockResult::Ok || !current.present)
            return result;
        const auto key = current.model.nativeModelInfo + NativeModelLodMemory::FieldOffset;
        auto       saved = originals.find(model);
        auto       storage = storageOwners.find(key);
        if (saved != originals.end() &&
            (saved->second.storage != key || !NativeModelLodMemory::SameIdentity(saved->second.origin, current) || storage == storageOwners.end()))
            return ClockResult::OwnerConflict;
        if (storage != storageOwners.end() &&
            (!NativeModelLodMemory::SameStorageIdentity(storage->second.origin, current) || current.bits != storage->second.lastOwned))
            return ClockResult::OwnerConflict;
        bool insertedRecord = false, insertedStorage = false;
        try
        {
            MtaGame::CModelLodDistancePolicy::Set(
                bits,
                [&](std::uint32_t& value) noexcept
                {
                    value = current.bits;
                    return true;
                },
                [&](std::uint32_t original)
                {
                    if (storage == storageOwners.end())
                    {
                        storage = storageOwners.emplace(key, Storage{NativeModelLodMemory::Identity(current), current.bits}).first;
                        insertedStorage = true;
                    }
                    if (saved == originals.end())
                    {
                        saved = originals.emplace(model, Saved{NativeModelLodMemory::Identity(current), key, original, Phase::Pending}).first;
                        insertedRecord = true;
                    }
                    // Cache before every first setter, including same bits.
                    // A second model-ID alias intentionally captures the
                    // current shared value as its distinct classic default.
                    Publish();
                    return true;
                },
                [&](std::uint32_t requested) noexcept
                {
                    bool accepted = false;
                    result = Write(image, current, requested, owner, after, exchange, accepted);
                    if (accepted)
                    {
                        storage->second.lastOwned = requested;
                        saved->second.phase = Phase::Pending;
                    }
                    return result == ClockResult::Ok;
                });
            Publish();
            return result;
        }
        catch (...)
        {
            // Only map insertion can throw here, before native entry. Remove
            // this attempt's partial CPU insertions without losing older IDs
            // or the shared storage ownership of an already modified alias.
            if (insertedRecord)
                originals.erase(model);
            if (insertedStorage)
                storageOwners.erase(key);
            Publish();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeModelLodDomain::ResetAll(std::uintptr_t image, std::uint64_t generation, CheckOwner owner, CheckOwner after, Exchange exchange) noexcept
    {
        ModelLodDomainDetail::Guard guard(operation, active);
        if (!guard.held)
            return ClockResult::Busy;
        if (uncertain.load())
            return ClockResult::RestoreFailure;
        auto result = ModelLodDomainDetail::Admit(image, generation, owner, after);
        if (result != ClockResult::Ok)
            return result;
        const bool completed = MtaGame::CModelLodDistancePolicy::ResetAll(
            originals,
            [&](std::uint32_t model, Saved& saved) noexcept
            {
                if (saved.origin.generation != generation)
                {
                    result = ClockResult::BindingChanged;
                    return false;
                }
                NativeModelLodMemory::Value current;
                result = NativeModelLodMemory::Capture(image, generation, model, owner, after, current);
                if (result != ClockResult::Ok)
                    return false;
                if (!current.present)
                {
                    if (saved.phase == Phase::Restored)
                    {
                        result = ClockResult::OwnerConflict;
                        return false;
                    }
                    // Classic skips a freshly absent bank association. Keep
                    // that distinct acknowledgement during a failed suffix;
                    // a later reappearing row cannot silently inherit it.
                    saved.phase = Phase::SkippedAbsent;
                    return true;
                }
                const auto key = current.model.nativeModelInfo + NativeModelLodMemory::FieldOffset;
                auto       storage = storageOwners.find(key);
                if (saved.phase == Phase::SkippedAbsent || key != saved.storage || storage == storageOwners.end() ||
                    !NativeModelLodMemory::SameIdentity(saved.origin, current) || !NativeModelLodMemory::SameStorageIdentity(storage->second.origin, current) ||
                    current.bits != storage->second.lastOwned)
                {
                    result = ClockResult::OwnerConflict;
                    return false;
                }
                if (saved.phase == Phase::Restored)
                    return true;
                bool accepted = false;
                result = Write(image, current, saved.original, owner, after, exchange, accepted);
                if (accepted)
                    storage->second.lastOwned = saved.original;
                if (result == ClockResult::Ok)
                    saved.phase = Phase::Restored;
                return result == ClockResult::Ok;
            });
        if (completed)
            storageOwners.clear();
        // An ordered alias suffix can legitimately change the value restored
        // by an earlier ID. Shared last-owned bits distinguish that accepted
        // prefix from foreign edits without replaying its already returned CAS.
        Publish();
        return completed ? ClockResult::Ok : result;
    }
}
