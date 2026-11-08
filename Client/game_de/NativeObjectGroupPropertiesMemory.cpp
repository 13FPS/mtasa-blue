/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Guard sparse builtin row fields and one masked native32-bit property CAS
 ****************************************************************************/
#include "NativeObjectGroupPropertiesMemory.h"
#include "NativeObjectGroupPropertiesProfile.h"
#include "NativeImageAccess.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <windows.h>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeObjectGroupPropertiesMemory
{
    namespace Detail
    {
        namespace P = NativeObjectGroupPropertiesProfile;
        namespace D = NativeStreamingCompletionContext;
        namespace I = NativeImageAccess;
        static_assert(P::PropertyCount == static_cast<std::size_t>(NativeObjectGroupProperty::Count));
        static_assert(sizeof(Raw) == P::RowBytes && P::RowBytes % sizeof(std::uint32_t) == 0);
        bool Profile(std::uintptr_t image) noexcept
        {
            IMAGE_DOS_HEADER   dos{};
            IMAGE_NT_HEADERS64 nt{};
            if (!image || image > (std::numeric_limits<std::uintptr_t>::max)() - NativePedPoolProfile::ImageSize ||
                !I::ReadImage(image, image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
                static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
                !I::ReadImage(image, image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64))
                return false;
            const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
            std::array<IMAGE_SECTION_HEADER, 96> sections{};
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                sizeof(IMAGE_SECTION_HEADER) * nt.FileHeader.NumberOfSections > NativePedPoolProfile::ImageSize - at ||
                !I::ReadImage(image, image + at, sections.data(), sizeof(IMAGE_SECTION_HEADER) * nt.FileHeader.NumberOfSections))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden) noexcept
            {
                for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                    if (I::SectionContains(sections[index], rva, bytes, required, forbidden))
                        return true;
                return false;
            };
            // Explicit native initializer writes prove at least these five
            // contiguous rows. Neither section extent nor classic160 is used
            // to infer a larger allocated/live table or an FX pointer lifetime.
            if (!contains(P::TableRva, P::TableBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !I::ImagePages(image, image + P::TableRva, P::TableBytes, true) || !D::Profile(image))
                return false;
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !pin.size || pin.size > actual.size() || !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) ||
                    std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        bool Fields(std::uintptr_t image, std::uint8_t group, Bits& output, Raw& rawOutput) noexcept
        {
            if (group >= P::BuiltinRows)
                return false;
            const auto               address = image + P::TableRva + static_cast<std::uintptr_t>(group) * P::RowBytes;
            MEMORY_BASIC_INFORMATION page{};
            if (address % alignof(std::uint32_t) || VirtualQuery(reinterpret_cast<void*>(address), &page, sizeof(page)) != sizeof(page) ||
                page.State != MEM_COMMIT || page.Type != MEM_IMAGE || page.Protect != PAGE_READWRITE ||
                reinterpret_cast<std::uintptr_t>(page.AllocationBase) != image || !I::ImagePages(image, address, P::RowBytes, true))
                return false;
            Raw    raw{};
            SIZE_T copied = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), raw.data(), raw.size(), &copied) || copied != raw.size())
                return false;
            Bits next{};
            for (std::size_t index = 0; index != P::PropertyCount; ++index)
            {
                const auto& field = P::Descriptors[index];
                if (field.bytes == 1)
                    next[index] = raw[field.offset];
                else
                    std::memcpy(&next[index], raw.data() + field.offset, sizeof(next[index]));
            }
            // The full row is only a raw equality fence. The FX pointer bytes
            // are never interpreted, dereferenced, transferred, or restored;
            // neither their width nor the unowned padding becomes an API field.
            output = next;
            rawOutput = raw;
            return true;
        }
        bool Seal(const Value& value, const Raw& expected) noexcept
        {
            D::Value      current;
            Bits          bits{};
            Raw           raw{};
            std::uint32_t state = 0;
            // All supplied callbacks already finished. These are fresh raw
            // world/member/full-row bookends under this borrowed engine scope.
            // Any sibling/padding/FX representation drift refuses the CAS or
            // makes an already accepted write's acknowledgement uncertain.
            return Profile(value.image) && D::Capture(value.image, current) == ClockResult::Ok && D::Same(value.world, current, true) &&
                   D::Fence(value.image, value.world) && Fields(value.image, value.group, bits, raw) && raw == expected &&
                   NativeGameStateMemory::Read(value.image, state) && state == 9 && NativeStartGameCalls::CheckThread(value.image) == ClockResult::Ok;
        }
        ClockResult Sample(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, CheckOwner owner, CheckOwner afterApplication,
                           Value& output) noexcept
        {
            auto result = afterApplication();
            if (result != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            Value next;
            next.image = image;
            next.generation = generation;
            next.group = group;
            if ((result = D::Capture(image, next.world)) != ClockResult::Ok)
                return result;
            if (!Fields(image, group, next.bits, next.raw))
                return ClockResult::ReadFailure;
            if ((result = afterApplication()) != ClockResult::Ok || (result = D::Owner(image, owner)) != ClockResult::Ok)
                return result;
            if (!Seal(next, next.raw))
                return ClockResult::BindingChanged;
            output = next;
            return ClockResult::Ok;
        }
        bool ExchangeField(std::uintptr_t address, std::uint32_t before, std::uint32_t desired, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            __try
            {
                LONG from = 0, to = 0;
                std::memcpy(&from, &before, sizeof(from));
                std::memcpy(&to, &desired, sizeof(to));
                const LONG result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), to, from);
                std::memcpy(&observed, &result, sizeof(observed));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
        }
    }
    bool Patch(Raw& raw, NativeObjectGroupProperty field, std::uint32_t desired) noexcept
    {
        const auto index = static_cast<std::size_t>(field);
        if (index >= NativeObjectGroupPropertiesProfile::PropertyCount)
            return false;
        const auto& descriptor = NativeObjectGroupPropertiesProfile::Descriptors[index];
        if (descriptor.bytes == 1)
        {
            if (desired > 0xffu)
                return false;
            raw[descriptor.offset] = static_cast<unsigned char>(desired);
        }
        else
            std::memcpy(raw.data() + descriptor.offset, &desired, sizeof(desired));
        return true;
    }
    Origin Identity(const Value& value) noexcept
    {
        Origin result;
        result.image = value.image;
        result.generation = value.generation;
        result.group = value.group;
        result.root = value.world.root;
        result.gameMode = value.world.gameMode;
        result.level = value.world.level;
        result.world = value.world.world;
        result.gameClass = value.world.gameClass;
        result.levelClass = value.world.levelClass;
        result.mode = value.world.mode;
        result.count = value.world.count;
        for (std::size_t index = 0; index < result.count && index < result.members.size(); ++index)
        {
            const auto& source = value.world.members[index];
            result.members[index] = {source.objectAddress, source.vtableAddress, source.classAddress, source.outerAddress,
                                     source.itemAddress,   source.index,         source.serial};
        }
        return result;
    }
    bool SameIdentity(const Origin& before, const Value& after) noexcept
    {
        const auto current = Identity(after);
        if (before.image != current.image || before.generation != current.generation || before.group != current.group || before.root != current.root ||
            before.gameMode != current.gameMode || before.level != current.level || before.world != current.world || before.gameClass != current.gameClass ||
            before.levelClass != current.levelClass || before.mode != current.mode || before.count != current.count || before.count > before.members.size())
            return false;
        for (std::size_t index = 0; index < before.count; ++index)
        {
            const auto& a = before.members[index];
            const auto& b = current.members[index];
            if (a.object != b.object || a.vtable != b.vtable || a.klass != b.klass || a.outer != b.outer || a.item != b.item || a.index != b.index ||
                a.serial != b.serial)
                return false;
        }
        return true;
    }
    bool SameIdentity(const Value& a, const Value& b) noexcept
    {
        return SameIdentity(Identity(a), b);
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint8_t group, CheckOwner owner, CheckOwner afterApplication,
                        Value& output) noexcept
    {
        if (!image || !generation || !owner || !afterApplication || group >= NativeObjectGroupPropertiesProfile::BuiltinRows)
            return ClockResult::InvalidArgument;
        if (!NativeStreamingCompletionContext::TryEnter())
            return ClockResult::Busy;
        NativeStreamingCompletionContext::Scope scope;
        Value                                   first, current;
        auto                                    result = Detail::Sample(image, generation, group, owner, afterApplication, first);
        if (result != ClockResult::Ok || (result = Detail::Sample(image, generation, group, owner, afterApplication, current)) != ClockResult::Ok)
            return result;
        if (!SameIdentity(first, current) || first.bits != current.bits || first.raw != current.raw)
            return ClockResult::BindingChanged;
        output = current;
        return ClockResult::Ok;
    }
    ClockResult Write(std::uintptr_t image, const Value& before, NativeObjectGroupProperty field, std::uint32_t desired, CheckOwner owner,
                      CheckOwner afterApplication, Receipt& output, Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.before = Identity(before);
        receipt.field = field;
        receipt.desired = desired;
        const auto finish = [&](ClockResult result) noexcept
        {
            receipt.result = result;
            output = receipt;
            return result;
        };
        const auto index = static_cast<std::size_t>(field);
        if (!image || image != before.image || !before.generation || !owner || !afterApplication ||
            before.group >= NativeObjectGroupPropertiesProfile::BuiltinRows || index >= NativeObjectGroupPropertiesProfile::PropertyCount ||
            (NativeObjectGroupPropertiesProfile::Descriptors[index].bytes == 1 && desired > 0xffu))
            return finish(ClockResult::InvalidArgument);
        if (!NativeStreamingCompletionContext::TryEnter())
            return finish(ClockResult::Busy);
        NativeStreamingCompletionContext::Scope scope;
        Value                                   current;
        auto                                    result = Detail::Sample(image, before.generation, before.group, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!SameIdentity(before, current) || before.bits != current.bits || before.raw != current.raw)
            return finish(ClockResult::BindingChanged);
        auto expectedAfter = current.bits;
        auto expectedRaw = current.raw;
        if (!Patch(expectedRaw, field, desired))
            return finish(ClockResult::InvalidArgument);
        expectedAfter[index] = desired;
        const auto&         descriptor = NativeObjectGroupPropertiesProfile::Descriptors[index];
        const auto          wordOffset = descriptor.offset & ~std::size_t{3};
        const auto          shift = static_cast<unsigned>((descriptor.offset - wordOffset) * 8);
        const std::uint32_t mask = descriptor.bytes == 1 ? 0xffu << shift : (std::numeric_limits<std::uint32_t>::max)();
        std::memcpy(&receipt.wordBefore, current.raw.data() + wordOffset, sizeof(receipt.wordBefore));
        std::memcpy(&receipt.wordDesired, expectedRaw.data() + wordOffset, sizeof(receipt.wordDesired));
        const auto address = image + NativeObjectGroupPropertiesProfile::TableRva +
                             static_cast<std::uintptr_t>(before.group) * NativeObjectGroupPropertiesProfile::RowBytes + wordOffset;
        // The native parser narrows six fields to BYTEs, including sparks+50.
        // A masked aligned DWORD CAS preserves all neighbouring fields and
        // padding and also refuses a competing sibling-byte change. No full
        // row memcpy or store across the FX pointer can enter this path.
        receipt.entered = true;
        const bool returned =
            (exchange ? exchange : Detail::ExchangeField)(address, receipt.wordBefore, receipt.wordDesired, receipt.wordObserved, receipt.exceptionCode);
        receipt.observed = (receipt.wordObserved & mask) >> shift;
        if (!returned)
        {
            receipt.uncertain = true;
            return finish(ClockResult::OriginalException);
        }
        receipt.accepted = receipt.wordObserved == receipt.wordBefore;
        if (!receipt.accepted)
            return finish(ClockResult::OwnerConflict);
        Value after;
        result = Detail::Sample(image, before.generation, before.group, owner, afterApplication, after);
        if (result == ClockResult::Ok && (!SameIdentity(current, after) || after.bits != expectedAfter || after.raw != expectedRaw))
            result = ClockResult::BindingChanged;
        if (result != ClockResult::Ok)
            receipt.uncertain = true;
        return finish(result);
    }
}
