/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Retain defaults, actual model holds and entered vector-store debt
 *****************************************************************************/
#include "NativeModelDummiesDomain.h"
#include <game/CModelVehicleDummyPolicy.h>

namespace mta::game_de
{
    namespace
    {
        class Operation
        {
        public:
            explicit Operation(std::atomic<bool>& flag) noexcept : flag(flag)
            {
                bool expected = false;
                acquired = flag.compare_exchange_strong(expected, true);
            }
            ~Operation()
            {
                if (acquired)
                    flag.store(false);
            }
            bool acquired = false;

        private:
            std::atomic<bool>& flag;
        };
    }
    void NativeModelDummiesDomain::Publish() noexcept
    {
        if (references.RecoveryPending())
            uncertain.store(true);
        pending.store(!records.empty() || references.HasPending() || uncertain.load());
    }
    ClockResult NativeModelDummiesDomain::Admit(std::uint64_t current, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        if (!current || !owner || !afterApplication)
            return ClockResult::InvalidArgument;
        if (generation && generation != current)
            return ClockResult::BindingChanged;
        if (uncertain.load())
            return ClockResult::OwnerConflict;
        auto result = owner();
        if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        generation = current;
        return ClockResult::Ok;
    }
    ClockResult NativeModelDummiesDomain::Read(std::uintptr_t image, std::uint64_t current, std::uint32_t model, CheckOwner owner, CheckOwner afterApplication,
                                               NativeModelDummyPositions& output) noexcept
    {
        Operation operation(active);
        if (!operation.acquired)
            return ClockResult::Busy;
        auto result = Admit(current, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        NativeModelDummiesMemory::Value value;
        result = NativeModelDummiesMemory::Capture(image, current, model, owner, afterApplication, value);
        if (result != ClockResult::Ok)
            return result;
        if (!value.model.vehicle || !value.present || !value.loaded.streamedIn)
            return ClockResult::NotInitialized;
        NativeModelDummyPositions positions{};
        for (std::size_t n = 0; n < positions.size(); ++n)
            positions[n] = NativeModelDummiesMemory::Decode(value.bits[n]);
        output = positions;
        return ClockResult::Ok;
    }
    ClockResult NativeModelDummiesDomain::ReadCachedDefault(std::uintptr_t image, std::uint64_t current, std::uint32_t model, std::uint32_t dummy,
                                                            CheckOwner owner, CheckOwner afterApplication, NativeModelDummyVector& output, bool& found) noexcept
    {
        Operation operation(active);
        if (!operation.acquired)
            return ClockResult::Busy;
        if (dummy >= NativeModelDummyPositions{}.size())
            return ClockResult::InvalidArgument;
        auto result = Admit(current, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        const auto record = records.find(model);
        if (record == records.end() || !record->second.cacheActive || record->second.originals.find(dummy) == record->second.originals.end())
        {
            found = false;
            return ClockResult::Ok;
        }
        NativeModelDummiesMemory::Value value;
        result = NativeModelDummiesMemory::Capture(image, current, model, owner, afterApplication, value);
        if (result != ClockResult::Ok)
            return result;
        if (!NativeModelDummiesMemory::SameIdentity(record->second.origin, value))
            return ClockResult::BindingChanged;
        output = NativeModelDummiesMemory::Decode(record->second.originals.at(dummy).bits);
        found = true;
        return ClockResult::Ok;
    }
    ClockResult NativeModelDummiesDomain::PrepareReference(std::uint32_t model, Record& record, CheckOwner owner) noexcept
    {
        if (!record.token.serial)
        {
            auto result = references.Reserve(generation, model, owner, record.token);
            if (result != ClockResult::Ok)
                return result;
            return references.Bind(generation, record.token, owner);
        }
        NativeModelBackingReferenceState state;
        auto                             result = references.Read(generation, record.token, owner, state);
        if (result != ClockResult::Ok)
            return result;
        if (state.phase != NativeModelBackingReferencePhase::Released)
            return ClockResult::OwnerConflict;
        NativeModelBackingReferenceToken next;
        result = references.RenewCycle(generation, record.token, owner, next);
        if (result == ClockResult::Ok)
            record.token = next;
        return result;
    }
    ClockResult NativeModelDummiesDomain::Set(std::uintptr_t image, std::uint64_t current, std::uint32_t model, std::uint32_t dummy,
                                              const NativeModelDummyVector& requested, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        const auto desired = NativeModelDummiesMemory::Encode(requested);
        Operation  operation(active);
        if (!operation.acquired)
            return ClockResult::Busy;
        if (dummy >= NativeModelDummyPositions{}.size())
            return ClockResult::InvalidArgument;
        auto result = Admit(current, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        try
        {
            NativeModelDummiesMemory::Value value;
            if ((result = NativeModelDummiesMemory::Capture(image, current, model, owner, afterApplication, value)) != ClockResult::Ok)
                return result;
            if (!value.model.vehicle || !value.present || !value.loaded.streamedIn)
                return ClockResult::NotInitialized;
            auto& record = records.try_emplace(model).first->second;
            if (record.cacheActive && (!record.held || record.restored || !NativeModelDummiesMemory::SameIdentity(record.origin, value)))
                return ClockResult::OwnerConflict;
            MtaGame::CModelVehicleDummyPolicy::Mutate(
                [&](bool& first)
                {
                    first = !record.cacheActive;
                    if (!first)
                        return true;
                    record.cacheActive = true;
                    record.restored = record.released = false;
                    record.origin = NativeModelDummiesMemory::Identity(value);
                    result = PrepareReference(model, record, owner);
                    Publish();
                    return result == ClockResult::Ok;
                },
                [&]
                {
                    result = references.Acquire(image, current, record.token, owner);
                    record.held = result == ClockResult::Ok;
                    Publish();
                    return result == ClockResult::Ok;
                },
                [&]
                {
                    result = NativeModelDummiesMemory::Capture(image, current, model, owner, afterApplication, value);
                    if (result != ClockResult::Ok)
                        return false;
                    if (!value.present || !NativeModelDummiesMemory::SameIdentity(record.origin, value))
                    {
                        result = ClockResult::BindingChanged;
                        return false;
                    }
                    record.originals.emplace(dummy, Saved{value.bits[dummy], false});
                    return true;
                },
                [&]
                {
                    result = NativeModelDummiesMemory::Write(image, value, dummy, desired, references, record.token, owner, afterApplication, record.write);
                    if (record.write.uncertain)
                        uncertain.store(true);
                    return result == ClockResult::Ok;
                });
            Publish();
            return result;
        }
        catch (...)
        {
            Publish();
            return ClockResult::ReadFailure;
        }
    }
    ClockResult NativeModelDummiesDomain::Restore(std::uintptr_t image, std::uint32_t model, Record& record, bool remove, CheckOwner owner,
                                                  CheckOwner afterApplication) noexcept
    {
        if (!record.cacheActive)
            return ClockResult::Ok;
        if (record.released)
        {
            if (remove)
            {
                record.originals.clear();
                record.cacheActive = false;
            }
            return ClockResult::Ok;
        }
        if (!record.held)
            return ClockResult::OwnerConflict;
        ClockResult result = ClockResult::Ok;
        MtaGame::CModelVehicleDummyPolicy::RestoreOne(
            record.originals,
            [&](std::uint32_t dummy, Saved& saved)
            {
                if (saved.restored)
                    return true;
                NativeModelDummiesMemory::Value value;
                result = NativeModelDummiesMemory::Capture(image, generation, model, owner, afterApplication, value);
                if (result != ClockResult::Ok)
                    return false;
                // Classic skips a current null visual. Require the same typed
                // metadata/root family even for that skip; a replaced live row
                // cannot receive a saved position from the previous lifetime.
                if (!value.model.vehicle || !NativeModelWheelMemory::SameIdentity(record.origin.model, value.model) ||
                    (value.present && !NativeModelDummiesMemory::SameIdentity(record.origin, value)))
                {
                    result = ClockResult::BindingChanged;
                    return false;
                }
                if (value.present)
                    result = NativeModelDummiesMemory::Write(image, value, dummy, saved.bits, references, record.token, owner, afterApplication, record.write);
                if (record.write.uncertain)
                    uncertain.store(true);
                if (result != ClockResult::Ok)
                    return false;
                saved.restored = true;
                return true;
            },
            [&]
            {
                record.restored = true;
                result = references.Release(image, generation, record.token, NativeModelReferenceRelease::ZeroPolicy::KeepResident, owner, &record.release);
                if (result == ClockResult::Ok)
                {
                    record.held = false;
                    record.released = true;
                }
                else if (record.release.reference.entered || record.release.removal.entered || references.RecoveryPending())
                    uncertain.store(true);
                return result == ClockResult::Ok;
            },
            remove,
            [&]
            {
                record.originals.clear();
                record.cacheActive = false;
            });
        return result;
    }
    ClockResult NativeModelDummiesDomain::PurgeReleased() noexcept
    {
        for (const auto& entry : records)
            if (entry.second.cacheActive)
                return ClockResult::Ok;
        auto result = references.RevokeUnacquired(generation);
        if (result != ClockResult::Ok || references.HasPending())
            return result != ClockResult::Ok ? result : ClockResult::Busy;
        records.clear();
        return ClockResult::Ok;
    }
    ClockResult NativeModelDummiesDomain::ResetOne(std::uintptr_t image, std::uint64_t current, std::uint32_t model, bool remove, CheckOwner owner,
                                                   CheckOwner afterApplication) noexcept
    {
        Operation operation(active);
        if (!operation.acquired)
            return ClockResult::Busy;
        auto result = Admit(current, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        const auto record = records.find(model);
        if (record != records.end())
            result = Restore(image, model, record->second, remove, owner, afterApplication);
        if (result == ClockResult::Ok)
            result = PurgeReleased();
        Publish();
        return result;
    }
    ClockResult NativeModelDummiesDomain::ResetAll(std::uintptr_t image, std::uint64_t current, CheckOwner owner, CheckOwner afterApplication) noexcept
    {
        Operation operation(active);
        if (!operation.acquired)
            return ClockResult::Busy;
        auto result = Admit(current, owner, afterApplication);
        if (result != ClockResult::Ok)
            return result;
        // Keep the complete MTA cache alive across all model callbacks. Mark
        // known restores/releases so a later explicit call never repeats them.
        MtaGame::CModelVehicleDummyPolicy::RestoreAll(
            records,
            [&](std::uint32_t model, Record& record)
            {
                result = Restore(image, model, record, false, owner, afterApplication);
                return result == ClockResult::Ok;
            },
            [&]
            {
                result = references.RevokeUnacquired(generation);
                if (result == ClockResult::Ok && references.HasPending())
                    result = ClockResult::Busy;
                return result == ClockResult::Ok;
            });
        Publish();
        return result;
    }
    ClockResult NativeModelDummiesDomain::RetireUnacquired(std::uint64_t current) noexcept
    {
        Operation operation(active);
        if (!operation.acquired)
            return ClockResult::Busy;
        if (!current || (generation && generation != current))
            return ClockResult::BindingChanged;
        if (uncertain.load())
            return ClockResult::OwnerConflict;
        auto result = references.RevokeUnacquired(current);
        if (result == ClockResult::Ok && references.HasPending())
            result = ClockResult::Busy;
        if (result == ClockResult::Ok)
        {
            for (const auto& entry : records)
                if (entry.second.held || (entry.second.write.accepted && !entry.second.released))
                    return ClockResult::Busy;
            records.clear();
            generation = 0;
        }
        Publish();
        return result;
    }
}
