/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute the proven native stat transaction on its admitted owner
 *****************************************************************************/
#include "NativeStatsWriteCalls.h"
#include "NativeStatsWriteProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeStatsWriteCalls
{
    namespace
    {
        namespace Profile = NativeStatsWriteProfile;
        namespace Image = NativeImageAccess;
        struct Identity
        {
            std::uintptr_t               ped = 0, data = 0, script = 0;
            unsigned char                weaponSlot = 0;
            std::array<unsigned char, 7> scriptName{};
            bool                         Same(const Identity& other) const noexcept
            {
                return ped == other.ped && data == other.data && script == other.script && weaponSlot == other.weaponSlot && scriptName == other.scriptName;
            }
        };
        ClockResult ReadStatus(Status value) noexcept
        {
            switch (value)
            {
                case Status::Ok:
                    return ClockResult::Ok;
                case Status::InvalidArgument:
                    return ClockResult::InvalidArgument;
                case Status::WrongThread:
                    return ClockResult::WrongThread;
                case Status::NotInitialized:
                    return ClockResult::NotInitialized;
                default:
                    return ClockResult::BindingChanged;
            }
        }
        bool ObjectPages(std::uintptr_t address, std::size_t bytes, bool writable) noexcept
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
                const bool canRead = protection == PAGE_READONLY || protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
                const bool canWrite = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY;
                if (!canRead || (writable && !canWrite))
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <typename T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            if (!ObjectPages(address, sizeof(output), false))
                return false;
            SIZE_T copied = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &output, sizeof(output), &copied) && copied == sizeof(output);
        }
        template <typename T>
        bool Global(std::uintptr_t image, std::uintptr_t rva, T& output) noexcept
        {
            return Image::ReadImage(image, image + rva, &output, sizeof(output), true);
        }
        bool Pins(std::uintptr_t image) noexcept
        {
            for (const auto& pin : Profile::Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            std::array<unsigned char, Profile::MissionGuard.size()> name{};
            return Image::ReadImage(image, image + Profile::MissionGuardRva, name.data(), name.size()) && name == Profile::MissionGuard;
        }
        ClockResult Context(std::uintptr_t image, bool scriptRequired, Identity& output) noexcept
        {
            Identity      next;
            unsigned char playerIndex = 1;
            if (!Global(image, Profile::PlayerIndexRva, playerIndex) || !Global(image, Profile::PlayerRootRva, next.ped))
                return ClockResult::ReadFailure;
            // This is MTA's established local-player-zero route, not a guessed
            // number of native player-info rows. Unknown alternate routes refuse.
            if (playerIndex != 0)
                return ClockResult::BindingChanged;
            if (!next.ped)
                return ClockResult::NotInitialized;
            std::uintptr_t vtable = 0, control = 0, info = 0;
            std::int32_t   selector = -1;
            float          health = 0;
            if (next.ped % alignof(std::uintptr_t) || !ObjectPages(next.ped, 0xa11, true) || !Object(next.ped, vtable) ||
                vtable != image + Profile::PlayerVtableRva || !Image::ReadImage(image, vtable + 0x50, &control, sizeof(control)) ||
                control != image + Profile::PlayerControlRva || !Object(next.ped + 0x5f0, info) || info != image + Profile::PlayerInfoRva ||
                !Object(next.ped + 0x7e0, selector) || selector != 0 || !Object(next.ped + 0xa10, next.weaponSlot) || next.weaponSlot >= 13 ||
                !Object(next.ped + 0x76c, health) || !std::isfinite(health) ||
                static_cast<double>(health) < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
                static_cast<double>(health) > static_cast<double>((std::numeric_limits<std::int32_t>::max)()) ||
                !Global(image, Profile::PlayerInfoRva, next.data) || !next.data || next.data % alignof(std::uintptr_t) ||
                !ObjectPages(next.data, 0x3c8, true) || !Image::ImagePages(image, image + Profile::PlayerRootRva, 0x1c0, true))
                // The native calorie path also converts existing health to
                // signed32 before its later arithmetic. A real object alone
                // cannot authorize a foreign nonrepresentable health value.
                return ClockResult::BindingChanged;
            if (scriptRequired)
            {
                if (!Global(image, Profile::ScriptRootRva, next.script) || !next.script)
                    return ClockResult::NotInitialized;
                if (next.script > (std::numeric_limits<std::uintptr_t>::max)() - 0x10 || !Object(next.script + 0x10, next.scriptName))
                    return ClockResult::ReadFailure;
            }
            output = next;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_STATS_NATIVE_INVOKE)
        ClockResult NativeCall(NativeFunction function, std::uint32_t index, float amount, std::uint32_t& exception) noexcept
        {
            __try
            {
                function(index, amount);
                return ClockResult::Ok;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                // Native reactions may already have committed multiple effects.
                // Never repeat or snapshot-restore this partially entered call.
                return ClockResult::OriginalException;
            }
        }
#endif
    }

    ClockResult Apply(const NativeStatsReader& reader, std::uintptr_t image, Operation operation, unsigned short index, float amount, CheckOwner checkOwner,
                      Receipt* receiptOutput, Invoke invoke) noexcept
    {
        Receipt    receipt;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        const bool floating = index < NativeStatsProfile::FloatCount;
        if ((operation != Operation::Set && operation != Operation::Modify) ||
            (!floating && (index < NativeStatsProfile::IntegerFirst || index >= NativeStatsProfile::IntegerSdkEnd)) || !std::isfinite(amount))
            return finish(ClockResult::InvalidArgument);
        receipt.promotedIndex = index;
        receipt.passedAmount = operation == Operation::Modify && amount < 0 ? -amount : amount;
        receipt.entryRva = operation == Operation::Set ? Profile::SetRva : amount < 0 ? Profile::DecrementRva : Profile::IncrementRva;
        if (!floating)
        {
            // Native integer paths first CVTTSS2SI to signed32, then sign-extend
            // AX. Admit the real first conversion domain, retaining the native
            // later signed16 narrowing/wrap instead of inventing int16 limits.
            const double widened = receipt.passedAmount;
            if (widened < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
                widened > static_cast<double>((std::numeric_limits<std::int32_t>::max)()))
                return finish(ClockResult::InvalidArgument);
        }
        if (!checkOwner)
            return finish(ClockResult::NotInitialized);
        if (!image || image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
            return finish(ClockResult::BindingChanged);
        float before = 0;
        auto  result = ReadStatus(reader.Read(index, before));
        if (result != ClockResult::Ok)
            return finish(result);
        if (!Pins(image))
            return finish(ClockResult::BindingChanged);
        const bool scriptRequired = operation == Operation::Set && index == 64;
        Identity   first, confirmed;
        result = Context(image, scriptRequired, first);
        if (result != ClockResult::Ok)
            return finish(result);
        result = checkOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        result = Context(image, scriptRequired, confirmed);
        if (result != ClockResult::Ok || !first.Same(confirmed))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        result = checkOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)))
            return finish(ClockResult::BindingChanged);
        const auto function = reinterpret_cast<NativeFunction>(image + receipt.entryRva);
        receipt.entered = true;
        // The ordinary-host owner fixture may replace only the final call
        // boundary at compile time. All actual image, player and owner guards
        // above still execute; production defines no such substitution.
#if defined(MTA_DE_STATS_NATIVE_INVOKE)
        const auto entry = invoke ? invoke : &MTA_DE_STATS_NATIVE_INVOKE;
#else
        const auto entry = invoke ? invoke : &NativeCall;
#endif
        result = entry(function, receipt.promotedIndex, receipt.passedAmount, receipt.exceptionCode);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.returned = true;
        float after = 0;
        result = ReadStatus(reader.Read(index, after));
        if (result != ClockResult::Ok || !Pins(image))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        result = Context(image, scriptRequired, confirmed);
        if (result != ClockResult::Ok || !first.Same(confirmed))
            return finish(result == ClockResult::Ok ? ClockResult::BindingChanged : result);
        // Native caps, mission guards and reactions may deliberately make the
        // final value differ from the requested amount. Acceptance means this
        // exact native transaction returned in the still-admitted owner scope.
        result = checkOwner();
        if (result != ClockResult::Ok)
            return finish(result);
        return finish(image == reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) ? ClockResult::Ok : ClockResult::BindingChanged);
    }
}
