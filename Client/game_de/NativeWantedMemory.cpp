/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute the actual wanted transaction and narrow classic NoFlash intent
 *****************************************************************************/
#include "NativeWantedMemory.h"
#include "NativeWantedProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>

namespace mta::game_de
{
    namespace WantedDetail
    {
        namespace Profile = NativeWantedProfile;
        namespace Image = NativeImageAccess;
        struct Snapshot
        {
            NativePlayerInfoMemory::Identity player;
            std::uintptr_t                   wanted = 0;
            std::uint32_t                    level = 0, timestamp = 0, timer = 0, maximumLevel = 0;
            std::int32_t                     maximumChaos = 0;
            bool                             SameObject(const Snapshot& other) const noexcept
            {
                return player.image == other.player.image && player.ped == other.player.ped && wanted == other.wanted;
            }
            bool SameValues(const Snapshot& other) const noexcept
            {
                return SameObject(other) && level == other.level && timestamp == other.timestamp && timer == other.timer &&
                       maximumLevel == other.maximumLevel && maximumChaos == other.maximumChaos;
            }
        };
        bool Pages(std::uintptr_t address, std::size_t bytes, bool writable) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto end = address + bytes;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                    return false;
                const auto protection = info.Protect & 0xff;
                const bool canWrite = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
                if ((!canWrite && protection != PAGE_READONLY) || (writable && !canWrite))
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool ReadObject(std::uintptr_t address, T& output) noexcept
        {
            SIZE_T copied = 0;
            return Pages(address, sizeof(output), false) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &output, sizeof(output), &copied) && copied == sizeof(output);
        }
        template <class T>
        bool ReadGlobal(std::uintptr_t image, std::uintptr_t rva, T& output) noexcept
        {
            return Image::ReadImage(image, image + rva, &output, sizeof(output), true);
        }
        bool Pins(std::uintptr_t image) noexcept
        {
            for (const auto& pin : Profile::Code)
            {
                std::array<unsigned char, 512> actual{};
                if (!pin.size || pin.size > actual.size() || !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) ||
                    std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            return true;
        }
        ClockResult Capture(const NativePlayerInfoMemory& player, NativeWantedMemory::CheckOwner owner, bool writable, Snapshot& output) noexcept
        {
            Snapshot next;
            auto     result = player.ReadIdentity(next.player, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Pins(next.player.image))
                return ClockResult::BindingChanged;
            if (!ReadGlobal(next.player.image, Profile::WantedRootRva, next.wanted))
                return ClockResult::ReadFailure;
            if (!next.wanted)
                return ClockResult::NotInitialized;
            if (next.wanted % alignof(std::uintptr_t) || !Pages(next.wanted, Profile::ObjectBytes, writable))
                return ClockResult::BindingChanged;
            if (!ReadObject(next.wanted + Profile::LevelOffset, next.level) || !ReadObject(next.wanted + Profile::TimestampOffset, next.timestamp) ||
                !ReadGlobal(next.player.image, Profile::TimerRva, next.timer) || !ReadGlobal(next.player.image, Profile::MaximumLevelRva, next.maximumLevel) ||
                !ReadGlobal(next.player.image, Profile::MaximumChaosRva, next.maximumChaos))
                return ClockResult::ReadFailure;
            NativePlayerInfoMemory::Identity confirmed;
            result = player.ReadIdentity(confirmed, owner);
            std::uintptr_t root = 0;
            if (result != ClockResult::Ok)
                return result;
            if (next.player.image != confirmed.image || next.player.ped != confirmed.ped || !ReadGlobal(next.player.image, Profile::WantedRootRva, root) ||
                root != next.wanted || !Pins(next.player.image))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        bool Domain(const Snapshot& value) noexcept
        {
            // Current native state must be representable by its own bounded
            // producers. Caller DWORD arguments retain the actual native signed
            // clamp and unsigned dispatch, including deliberate out-of-range noops.
            return value.level <= 6 && value.maximumLevel < Profile::MaximumChaos.size() && value.maximumChaos == Profile::MaximumChaos[value.maximumLevel];
        }
        bool ExchangeTimestamp(std::uintptr_t address, std::uint32_t original, std::uint32_t expected, std::uint32_t& observed) noexcept
        {
            __try
            {
                observed = static_cast<std::uint32_t>(
                    InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(original), static_cast<LONG>(expected)));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
#if !defined(MTA_DE_WANTED_NATIVE_INVOKE)
        ClockResult NativeCall(NativeWantedMemory::NativeFunction function, void* wanted, std::uint32_t level, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(wanted, level);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return ClockResult::OriginalException;
            }
        }
#endif
    }

    ClockResult NativeWantedMemory::Qualify(CheckOwner owner) const noexcept
    {
        WantedDetail::Snapshot first, second;
        auto                   result = WantedDetail::Capture(playerInfo, owner, false, first);
        if (result != ClockResult::Ok)
            return result;
        result = WantedDetail::Capture(playerInfo, owner, false, second);
        if (result != ClockResult::Ok)
            return result;
        return first.SameObject(second) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
    ClockResult NativeWantedMemory::ReadLevel(std::uint32_t& output, CheckOwner owner) const noexcept
    {
        WantedDetail::Snapshot first, second;
        auto                   result = WantedDetail::Capture(playerInfo, owner, false, first);
        if (result != ClockResult::Ok)
            return result;
        result = WantedDetail::Capture(playerInfo, owner, false, second);
        if (result != ClockResult::Ok)
            return result;
        if (!first.SameObject(second) || first.level != second.level)
            return ClockResult::BindingChanged;
        // Reads preserve raw native bits. Only the actual SDK wrapper applies
        // the established clamp, so callers cannot mistake corruption for0.
        output = second.level;
        return ClockResult::Ok;
    }
    ClockResult NativeWantedMemory::SetLevel(std::uint32_t level, bool noFlash, CheckOwner owner, Receipt* receiptOutput, Invoke invoke) const noexcept
    {
        Receipt receipt;
        receipt.requestedLevel = level;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        WantedDetail::Snapshot before, confirmed;
        auto                   result = WantedDetail::Capture(playerInfo, owner, true, before);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!WantedDetail::Domain(before))
            return finish(ClockResult::BindingChanged);
        result = WantedDetail::Capture(playerInfo, owner, true, confirmed);
        if (result != ClockResult::Ok || !before.SameValues(confirmed))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        // These are synchronous borrowed pointers. The retained engine owner
        // must grant the same generation/real callback immediately before entry.
        result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_WANTED_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_WANTED_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &WantedDetail::NativeCall;
#endif
        receipt.entered = true;
        result = entry(reinterpret_cast<NativeFunction>(before.player.image + NativeWantedProfile::SetRva), reinterpret_cast<void*>(before.wanted), level,
                       receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        result = WantedDetail::Capture(playerInfo, owner, true, confirmed);
        if (result != ClockResult::Ok || !before.SameObject(confirmed))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        if (!noFlash)
            return finish(owner());
        if (confirmed.timestamp == before.timestamp)
        {
            receipt.timestampRestored = true;
            return finish(owner());
        }
        // Classic NoFlash restores just this timestamp after the real setter.
        // Attribute the value to its proved timer store before claiming it;
        // preserve a later timer/foreign edit instead of overwriting it.
        if (confirmed.level == before.level || confirmed.timestamp != before.timer || confirmed.timer != before.timer)
            return finish(ClockResult::OwnerConflict);
        WantedDetail::Snapshot final;
        result = WantedDetail::Capture(playerInfo, owner, true, final);
        if (result != ClockResult::Ok || !confirmed.SameValues(final))
            return finish(result == ClockResult::Ok ? ClockResult::OwnerConflict : result);
        result = owner();
        if (result != ClockResult::Ok)
            return finish(result);
        std::uint32_t observed = 0;
        if (!WantedDetail::ExchangeTimestamp(before.wanted + NativeWantedProfile::TimestampOffset, before.timestamp, confirmed.timestamp, observed))
            return finish(ClockResult::WriteFailure);
        if (observed != confirmed.timestamp)
            return finish(ClockResult::OwnerConflict);
        result = WantedDetail::Capture(playerInfo, owner, true, final);
        if (result != ClockResult::Ok || !before.SameObject(final))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        if (final.timestamp != before.timestamp)
            return finish(ClockResult::OwnerConflict);
        receipt.timestampRestored = true;
        return finish(owner());
    }
}
