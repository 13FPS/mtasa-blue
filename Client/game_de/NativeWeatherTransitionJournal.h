/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Resolve only partial native SDK weather mutations before retirement
 *****************************************************************************/
#pragma once
#include "NativeWeatherTransitionMemory.h"
#include <game/CGameClockService.h>

namespace mta::game_de
{
    class NativeWeatherTransitionJournal final
    {
    public:
        using Memory = NativeWeatherTransitionMemory;
        using Field = Memory::Field;
        NativeWeatherTransitionJournal() = default;
        NativeWeatherTransitionJournal(const NativeWeatherTransitionJournal&) = delete;
        NativeWeatherTransitionJournal& operator=(const NativeWeatherTransitionJournal&) = delete;

        ClockResult ReadForced(const Memory& memory, std::uint16_t& output) const noexcept
        {
            if (HasPending())
                return ClockResult::Busy;
            std::uint32_t value = 0;
            if (!memory.Read(Field::ForcedType, value))
                return ClockResult::ReadFailure;
            if (!ValidType(Field::ForcedType, value))
                return ClockResult::BindingChanged;
            output = static_cast<std::uint16_t>(value);
            return ClockResult::Ok;
        }
        ClockResult SetTypes(const Memory& memory, std::uint8_t primary, std::uint8_t secondary) noexcept
        {
            if (HasPending())
                return ClockResult::Busy;
            if (primary >= NativeWeatherTransitionProfile::WeatherTypeCount || secondary >= NativeWeatherTransitionProfile::WeatherTypeCount)
                return ClockResult::InvalidArgument;
            entries = {{{Field::OldType, 0, primary, false}, {Field::NewType, 0, secondary, false}}};
            count = 2;
            return CaptureAndApply(memory);
        }
        ClockResult ReleaseForced(const Memory& memory) noexcept
        {
            if (HasPending())
                return ClockResult::Busy;
            // This is the full native signed16 -1 sentinel, not a copied
            // classic byte0xff write and not the force-now native function.
            entries = {{{Field::ForcedType, 0, 0xffff, false}, {}}};
            count = 1;
            return CaptureAndApply(memory);
        }
        ClockResult ResyncWithClock(const Memory& memory) noexcept
        {
            if (HasPending())
                return ClockResult::Busy;
            std::uint16_t seconds = 0;
            if (!memory.ReadClockSeconds(seconds))
                return ClockResult::ReadFailure;
            // Native weather wrap uses integer previous seconds, unlike the
            // classic float-only comparison. Both changes belong to this one
            // synchronous callback; a failed second write cannot be forgotten.
            entries = {{{Field::PreviousClockSeconds, 0, seconds, false}, {Field::InterpolationBits, 0, 0, false}}};
            count = 2;
            return CaptureAndApply(memory);
        }
        bool HasPending() const noexcept
        {
            for (const auto& entry : entries)
                if (entry.pending)
                    return true;
            return false;
        }
        std::size_t PendingCount() const noexcept
        {
            std::size_t result = 0;
            for (const auto& entry : entries)
                result += entry.pending ? 1 : 0;
            return result;
        }
        std::size_t SupersededCount() const noexcept { return superseded; }
        ClockResult Recover(const Memory& memory) noexcept
        {
            superseded = 0;
            if (!HasPending())
            {
                Clear();
                return ClockResult::Ok;
            }
            if (!image || image != memory.ImageIdentity())
                return ClockResult::BindingChanged;
            // Reverse only writes from an incomplete operation. Successful SDK
            // changes are persistent and never form generic End-time rollback.
            for (std::size_t index = count; index > 0; --index)
            {
                auto& entry = entries[index - 1];
                if (!entry.pending)
                    continue;
                std::uint32_t observed = 0;
                if (!memory.Read(entry.field, observed))
                    continue;
                if (observed == entry.original)
                {
                    entry.pending = false;
                    continue;
                }
                if (observed != entry.replacement)
                {
                    // A later native/foreign value relinquishes this scalar.
                    // Preserve it and report supersession rather than keeping
                    // an owner alive forever or claiming we restored its byte.
                    entry.pending = false;
                    ++superseded;
                    continue;
                }
                const auto result = memory.WriteIf(entry.field, entry.replacement, entry.original);
                if (result == Memory::WriteResult::Written || result == Memory::WriteResult::Unchanged)
                    entry.pending = false;
            }
            if (HasPending())
                return ClockResult::RestoreFailure;
            Clear();
            return superseded ? ClockResult::OwnerConflict : ClockResult::Ok;
        }

    private:
        struct Entry
        {
            Field         field = Field::OldType;
            std::uint32_t original = 0, replacement = 0;
            bool          pending = false;
        };
        ClockResult CaptureAndApply(const Memory& memory) noexcept
        {
            superseded = 0;
            image = memory.ImageIdentity();
            for (std::size_t index = 0; index < count; ++index)
                if (!memory.Read(entries[index].field, entries[index].original))
                {
                    Clear();
                    return ClockResult::ReadFailure;
                }
            for (std::size_t index = 0; index < count; ++index)
                if (!ValidType(entries[index].field, entries[index].original))
                {
                    Clear();
                    return ClockResult::BindingChanged;
                }
            for (std::size_t index = 0; index < count; ++index)
            {
                std::uint32_t bookend = 0;
                if (!memory.Read(entries[index].field, bookend))
                {
                    Clear();
                    return ClockResult::ReadFailure;
                }
                if (bookend != entries[index].original)
                {
                    Clear();
                    return ClockResult::Busy;
                }
            }
            for (std::size_t index = 0; index < count; ++index)
            {
                auto&      entry = entries[index];
                const auto result = memory.WriteIf(entry.field, entry.original, entry.replacement);
                if (result == Memory::WriteResult::Written || result == Memory::WriteResult::Unconfirmed)
                    entry.pending = true;
                if (result == Memory::WriteResult::Written || result == Memory::WriteResult::Unchanged)
                    continue;
                const auto failure = result == Memory::WriteResult::Conflict ? ClockResult::OwnerConflict : ClockResult::WriteFailure;
                const auto recovery = Recover(memory);
                if (HasPending())
                    return ClockResult::RestoreFailure;
                return recovery == ClockResult::OwnerConflict ? recovery : failure;
            }
            // All readbacks confirmed. Clearing the journal relinquishes only
            // partial-operation recovery debt; it never undoes an SDK setter.
            Clear();
            return ClockResult::Ok;
        }
        static bool ValidType(Field field, std::uint32_t value) noexcept
        {
            if (field == Field::OldType || field == Field::NewType)
                return value < NativeWeatherTransitionProfile::WeatherTypeCount;
            return field != Field::ForcedType || value < NativeWeatherTransitionProfile::WeatherTypeCount || value == 0xffff;
        }
        void Clear() noexcept
        {
            entries = {};
            count = 0;
            image = 0;
        }
        std::array<Entry, 2> entries{};
        std::size_t          count = 0;
        std::size_t          superseded = 0;
        std::uintptr_t       image = 0;
    };
}
