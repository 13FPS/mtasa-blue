/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Recheck actual loaded-player identity around every admitted intent
 *****************************************************************************/
#include "NativePlayerInfoMemory.h"
#include "NativePlayerInfoProfile.h"
#include "NativePlayerInfoExtraProfile.h"
#include "NativeImageAccess.h"
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>

namespace mta::game_de
{
    namespace
    {
        namespace P = NativePlayerInfoProfile;
        namespace E = NativePlayerInfoExtraProfile;
        namespace Image = NativeImageAccess;
        struct PlayerIdentity
        {
            std::uintptr_t ped = 0;
            bool           Same(const PlayerIdentity& other) const noexcept { return ped == other.ped; }
        };
        bool ObjectPages(std::uintptr_t address, std::size_t bytes) noexcept
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
                if (protection != PAGE_READONLY && protection != PAGE_READWRITE && protection != PAGE_WRITECOPY)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                if (info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            SIZE_T copied = 0;
            return ObjectPages(address, sizeof(output)) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &output, sizeof(output), &copied) && copied == sizeof(output);
        }
        template <class T>
        bool Global(std::uintptr_t image, std::uintptr_t rva, T& output) noexcept
        {
            return Image::ReadImage(image, image + rva, &output, sizeof(output), true);
        }
        bool Binding(std::uintptr_t image) noexcept
        {
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - P::ImageSize)
                return false;
            IMAGE_DOS_HEADER dos{};
            if (!Image::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > P::ImageSize - sizeof(IMAGE_NT_HEADERS64))
                return false;
            IMAGE_NT_HEADERS64 nt{};
            const auto         ntRva = static_cast<std::uintptr_t>(dos.e_lfanew);
            if (!Image::ReadImage(image, image + ntRva, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != P::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
                !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
                return false;
            const auto sectionRva = ntRva + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            const auto sectionBytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (sectionRva > P::ImageSize || sectionBytes > P::ImageSize - sectionRva)
                return false;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!Image::ReadImage(image, image + sectionRva, sections.data(), sectionBytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden)
            {
                for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                    if (Image::SectionContains(sections[i], rva, bytes, required, forbidden))
                        return true;
                return false;
            };
            for (const auto& pin : P::Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            for (const auto& pin : E::Code)
            {
                std::array<unsigned char, 160> actual{};
                if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                    !Image::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes.data(), pin.size))
                    return false;
            }
            return contains(P::BankRva, P::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
                   contains(P::PlayerIndexRva, 1, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
                   Image::ImagePages(image, image + P::BankRva, P::RowBytes, true) && Image::ImagePages(image, image + P::PlayerIndexRva, 1, true);
        }
        ClockResult Context(std::uintptr_t image, PlayerIdentity& output) noexcept
        {
            unsigned char  index = 1;
            PlayerIdentity next;
            if (!Global(image, P::PlayerIndexRva, index) || !Global(image, P::BankRva, next.ped))
                return ClockResult::ReadFailure;
            if (index != 0)
                return ClockResult::BindingChanged;
            if (!next.ped)
                return ClockResult::NotInitialized;
            std::uintptr_t vtable = 0, control = 0, info = 0;
            std::int32_t   selector = -1;
            if (next.ped % alignof(std::uintptr_t) || next.ped > (std::numeric_limits<std::uintptr_t>::max)() - 0x7e4 || !Object(next.ped, vtable) ||
                vtable != image + P::PlayerVtableRva || !Image::ReadImage(image, vtable + 0x50, &control, sizeof(control)) ||
                control != image + P::PlayerControlRva || !Object(next.ped + 0x5f0, info) || info != image + P::BankRva + 8 ||
                !Object(next.ped + 0x7e0, selector) || selector != 0)
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        ClockResult Owner(NativePlayerInfoMemory::CheckOwner owner) noexcept
        {
            return owner ? owner() : ClockResult::NotInitialized;
        }
        bool Exchange(std::uintptr_t address, std::size_t width, std::uint64_t value, std::uint64_t expected, std::uint64_t& observed) noexcept
        {
            __try
            {
                if (width == 8)
                    observed = static_cast<std::uint64_t>(
                        InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), static_cast<LONG64>(value), static_cast<LONG64>(expected)));
                else if (width == 2)
                    observed = static_cast<std::uint16_t>(
                        InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(address), static_cast<SHORT>(value), static_cast<SHORT>(expected)));
                else if (width == 1)
                    observed = static_cast<unsigned char>(
                        _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address), static_cast<char>(value), static_cast<char>(expected)));
                else
                    return false;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }
    }
    ClockResult NativePlayerInfoMemory::Bind() noexcept
    {
        const auto thread = GetCurrentThreadId();
        if (ownerThread.load() && ownerThread.load() != thread)
            return ClockResult::WrongThread;
        Reset();
        if (sizeof(void*) != 8 || !Image::CheckExecutableBuild())
            return ClockResult::UnsupportedBuild;
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (!Binding(image))
            return ClockResult::UnsupportedBuild;
        imageAddress.store(image);
        ownerThread.store(thread);
        return ClockResult::Ok;
    }
    void NativePlayerInfoMemory::Reset() noexcept
    {
        imageAddress.store(0);
        ownerThread.store(0);
    }
    bool NativePlayerInfoMemory::IsBound() const noexcept
    {
        return imageAddress.load() && ownerThread.load();
    }
    ClockResult NativePlayerInfoMemory::Current(std::uintptr_t& output) const noexcept
    {
        const auto image = imageAddress.load();
        const auto thread = ownerThread.load();
        if (!image || !thread)
            return ClockResult::NotInitialized;
        if (thread != GetCurrentThreadId())
            return ClockResult::WrongThread;
        if (image != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) || !Binding(image))
            return ClockResult::BindingChanged;
        output = image;
        return ClockResult::Ok;
    }
    ClockResult NativePlayerInfoMemory::ReadMoney(std::int32_t& output, CheckOwner owner) const noexcept
    {
        auto result = Owner(owner);
        if (result != ClockResult::Ok)
            return result;
        std::uintptr_t image = 0;
        if ((result = Current(image)) != ClockResult::Ok)
            return result;
        PlayerIdentity before, after;
        if ((result = Context(image, before)) != ClockResult::Ok)
            return result;
        std::int32_t first = 0, second = 0;
        if (!Global(image, P::BankRva + P::MoneyOffset, first) || !Global(image, P::BankRva + P::MoneyOffset, second))
            return ClockResult::ReadFailure;
        if ((result = Current(image)) != ClockResult::Ok || (result = Context(image, after)) != ClockResult::Ok)
            return result;
        if (!before.Same(after) || first != second)
            return ClockResult::OwnerConflict;
        if ((result = Owner(owner)) != ClockResult::Ok)
            return result;
        output = first;
        return ClockResult::Ok;
    }
    ClockResult NativePlayerInfoMemory::ReadIdentity(NativePlayerInfoMemory::Identity& output, CheckOwner owner) const noexcept
    {
        auto result = Owner(owner);
        if (result != ClockResult::Ok)
            return result;
        std::uintptr_t image = 0;
        if ((result = Current(image)) != ClockResult::Ok)
            return result;
        PlayerIdentity before, after;
        if ((result = Context(image, before)) != ClockResult::Ok || (result = Current(image)) != ClockResult::Ok ||
            (result = Context(image, after)) != ClockResult::Ok)
            return result;
        if (!before.Same(after))
            return ClockResult::OwnerConflict;
        if ((result = Owner(owner)) != ClockResult::Ok)
            return result;
        output = {image, before.ped};
        return ClockResult::Ok;
    }
    ClockResult NativePlayerInfoMemory::Write(std::size_t offset, std::size_t width, std::uint64_t value, bool instant, CheckOwner owner) const noexcept
    {
        auto result = Owner(owner);
        if (result != ClockResult::Ok)
            return result;
        std::uintptr_t image = 0;
        if ((result = Current(image)) != ClockResult::Ok)
            return result;
        PlayerIdentity before, after;
        if ((result = Context(image, before)) != ClockResult::Ok)
            return result;
        const auto    address = image + P::BankRva + offset;
        std::uint64_t expected = 0;
        if (address % width || !Image::ReadImage(image, address, &expected, width, true))
            return ClockResult::ReadFailure;
        if (width == 8)
        {
            // Classic SetPlayerMoney writes Score always and DisplayScore only
            // for instant updates. One aligned pair CAS preserves that behavior
            // while refusing an intervening producer instead of overwriting it.
            value = (value & UINT64_C(0xffffffff)) | (instant ? (value << 32) : (expected & UINT64_C(0xffffffff00000000)));
        }
        if ((result = Current(image)) != ClockResult::Ok || (result = Context(image, after)) != ClockResult::Ok)
            return result;
        if (!before.Same(after))
            return ClockResult::OwnerConflict;
        // Every exchange is immediately guarded by the real retained callback
        // owner. Binding an image or observing a ped is never write authority.
        if ((result = Owner(owner)) != ClockResult::Ok)
            return result;
        std::uint64_t observed = 0;
        if (!Exchange(address, width, value, expected, observed))
            return ClockResult::WriteFailure;
        if (observed != expected)
            return ClockResult::OwnerConflict;
        if ((result = Owner(owner)) != ClockResult::Ok)
            return result;
        std::uint64_t actual = 0;
        if (!Image::ReadImage(image, address, &actual, width, true))
            return ClockResult::ReadFailure;
        if (actual != value)
            return ClockResult::OwnerConflict;
        if ((result = Current(image)) != ClockResult::Ok || (result = Context(image, after)) != ClockResult::Ok)
            return result;
        if (!before.Same(after))
            return ClockResult::OwnerConflict;
        if ((result = Owner(owner)) != ClockResult::Ok)
            return result;
        // Successful persistent effects and unverifiable post-exchange effects
        // remain native intent. End must not restore an old player-info snapshot.
        return ClockResult::Ok;
    }
    ClockResult NativePlayerInfoMemory::SetMoney(std::int32_t value, bool instant, CheckOwner owner) const noexcept
    {
        return Write(P::MoneyOffset, 8, static_cast<std::uint32_t>(value), instant, owner);
    }
    ClockResult NativePlayerInfoMemory::SetLastTimeEaten(std::int16_t value, CheckOwner owner) const noexcept
    {
        return Write(P::LastTimeEatenOffset, 2, static_cast<std::uint16_t>(value), false, owner);
    }
    ClockResult NativePlayerInfoMemory::SetDoesNotGetTired(bool value, CheckOwner owner) const noexcept
    {
        return Write(P::DoesNotGetTiredOffset, 1, value ? 1 : 0, false, owner);
    }
    ClockResult NativePlayerInfoMemory::ReadScalar(std::size_t offset, std::size_t width, std::uint64_t& output, CheckOwner owner) const noexcept
    {
        auto result = Owner(owner);
        if (result != ClockResult::Ok)
            return result;
        std::uintptr_t image = 0;
        if ((result = Current(image)) != ClockResult::Ok)
            return result;
        PlayerIdentity before, after;
        if ((result = Context(image, before)) != ClockResult::Ok)
            return result;
        std::uint64_t first = 0, second = 0;
        if (!Image::ReadImage(image, image + P::BankRva + offset, &first, width, true) ||
            !Image::ReadImage(image, image + P::BankRva + offset, &second, width, true))
            return ClockResult::ReadFailure;
        if ((result = Current(image)) != ClockResult::Ok || (result = Context(image, after)) != ClockResult::Ok)
            return result;
        if (!before.Same(after) || first != second)
            return ClockResult::OwnerConflict;
        if ((result = Owner(owner)) != ClockResult::Ok)
            return result;
        output = first;
        return ClockResult::Ok;
    }
    ClockResult NativePlayerInfoMemory::ReadFloat(NativePlayerInfoFloatField field, float& output, CheckOwner owner) const noexcept
    {
        constexpr std::array<std::size_t, 4> offsets{E::FpsMoveHeadingOffset, E::CarTwoWheelDistanceOffset, E::BikeRearWheelDistanceOffset,
                                                     E::BikeFrontWheelDistanceOffset};
        const auto                           index = static_cast<std::size_t>(field);
        if (index >= offsets.size())
            return ClockResult::InvalidArgument;
        std::uint64_t raw = 0;
        const auto    result = ReadScalar(offsets[index], sizeof(float), raw, owner);
        if (result == ClockResult::Ok)
        {
            const auto bits = static_cast<std::uint32_t>(raw);
            // Classic getters expose the stored engine value. Preserve even
            // signed zero/NaN payloads rather than inventing a finite snapshot.
            std::memcpy(&output, &bits, sizeof(output));
        }
        return result;
    }
    ClockResult NativePlayerInfoMemory::ReadDword(NativePlayerInfoDwordField field, std::uint32_t& output, CheckOwner owner) const noexcept
    {
        constexpr std::array<std::size_t, 4> offsets{E::CarTwoWheelCounterOffset, E::CarLess3WheelCounterOffset, E::BikeRearWheelCounterOffset,
                                                     E::BikeFrontWheelCounterOffset};
        const auto                           index = static_cast<std::size_t>(field);
        if (index >= offsets.size())
            return ClockResult::InvalidArgument;
        std::uint64_t raw = 0;
        const auto    result = ReadScalar(offsets[index], sizeof(output), raw, owner);
        if (result == ClockResult::Ok)
            output = static_cast<std::uint32_t>(raw);
        return result;
    }
    ClockResult NativePlayerInfoMemory::ReadByte(NativePlayerInfoByteField field, std::uint8_t& output, CheckOwner owner) const noexcept
    {
        constexpr std::array<std::size_t, 4> offsets{E::CamDrunkLevelOffset, E::MaxHealthOffset, E::MaxArmorOffset, E::CanDoDriveByOffset};
        const auto                           index = static_cast<std::size_t>(field);
        if (index >= offsets.size())
            return ClockResult::InvalidArgument;
        std::uint64_t raw = 0;
        const auto    result = ReadScalar(offsets[index], sizeof(output), raw, owner);
        if (result == ClockResult::Ok)
            output = static_cast<std::uint8_t>(raw);
        return result;
    }
    ClockResult NativePlayerInfoMemory::SetCamDrunkLevel(std::uint8_t level, CheckOwner owner) const noexcept
    {
        // Existing MTA sets the fading bit only for level zero. These adjacent
        // native bytes form one aligned short, so one checked exchange keeps
        // the pair coherent while preserving independent native producers.
        const auto pair = static_cast<std::uint16_t>(level) | (level == 0 ? std::uint16_t{0x100} : std::uint16_t{0});
        return Write(E::CamDrunkLevelOffset, 2, pair, false, owner);
    }
}
