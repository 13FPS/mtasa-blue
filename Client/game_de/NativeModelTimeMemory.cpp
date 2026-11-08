/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh static timed-model identity and one aligned hour-word CAS
 ****************************************************************************/
#include "NativeModelTimeMemory.h"
#include "NativeModelTimeProfile.h"
#include "NativeModelTypeLeaf.h"
#include "NativeImageAccess.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <windows.h>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeModelTimeMemory
{
    namespace TimeMemoryDetail
    {
        namespace P = NativeModelTimeProfile;
        namespace I = NativeImageAccess;
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
            const auto                           bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (!nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > sections.size() || at > NativePedPoolProfile::ImageSize ||
                bytes > NativePedPoolProfile::ImageSize - at || !I::ReadImage(image, image + at, sections.data(), bytes))
                return false;
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden)
            {
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (I::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            if (!contains(P::PoolRva, P::PoolCapacity * P::RowBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::PoolCountRva, sizeof(std::int32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
                !contains(P::VtableRva, sizeof(P::VtableMethods), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (const auto& pin : P::Pins)
            {
                std::array<unsigned char, P::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) || std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            // The existing named Time producer owns only these first eight
            // slots. Do not widen this into a guessed second type3 layout or
            // call either the native accessor or a captured virtual pointer.
            for (std::size_t index = 0; index != sizeof(P::VtableMethods) / sizeof(P::VtableMethods[0]); ++index)
            {
                std::uintptr_t pointer = 0;
                if (!I::ReadImage(image, image + P::VtableRva + index * sizeof(pointer), &pointer, sizeof(pointer)) ||
                    pointer != image + P::VtableMethods[index])
                    return false;
            }
            return NativeStreamingModelContext::QualifyProfile(image);
        }
        bool Tuple(std::uintptr_t image, const Value& value, std::int32_t& count, std::array<unsigned char, 4>& tuple) noexcept
        {
            const auto base = image + P::PoolRva;
            const auto model = value.model.nativeModelInfo;
            if (model < base || model - base >= P::PoolCapacity * P::RowBytes || (model - base) % P::RowBytes ||
                !I::ReadImage(image, image + P::PoolCountRva, &count, sizeof(count), true) || count <= 0 ||
                count > static_cast<std::int32_t>(P::PoolCapacity) || (model - base) / P::RowBytes >= static_cast<std::size_t>(count) ||
                value.context.modelVtable != image + P::VtableRva || value.context.typeMethod != image + P::TypeMethodRva)
                return false;
            std::uintptr_t vtable = 0, method = 0;
            if (!I::ReadImage(image, model, &vtable, sizeof(vtable), true) || vtable != image + P::VtableRva ||
                !I::ReadImage(image, vtable + P::TimeSlot, &method, sizeof(method)) || method != image + P::TimeMethodRva)
                return false;
            // Preserve the pairing word together with both hour bytes. Its
            // day/night association belongs to the parser and is never part
            // of MTA's hour setter, even when two bank keys alias this row.
            return I::ReadImage(image, model + P::TimeOffset, tuple.data(), tuple.size(), true);
        }
        std::uint16_t Word(const std::array<unsigned char, 4>& tuple, std::size_t offset) noexcept
        {
            std::uint16_t value = 0;
            std::memcpy(&value, tuple.data() + offset, sizeof(value));
            return value;
        }
        Member Membership(const NativeUObjectMembershipAccess::Snapshot& value) noexcept
        {
            return {value.objectAddress, value.vtableAddress, value.classAddress, value.outerAddress, value.itemAddress, value.index, value.serial};
        }
        bool Same(const Member& a, const Member& b) noexcept
        {
            return a.object == b.object && a.vtable == b.vtable && a.klass == b.klass && a.outer == b.outer && a.item == b.item && a.index == b.index &&
                   a.serial == b.serial;
        }
        bool Anchors(std::uintptr_t image, const Value& value, const std::array<unsigned char, 4>& expected) noexcept
        {
            // All caller callbacks finish before this raw equality fence and
            // the immediate CAS. Current model/provider/member observations
            // are never reused as a retained lease by the default journal.
            std::uintptr_t current = 0;
            if (!NativeModelBankReader::Revalidate({image}) || !Profile(image) ||
                !I::ReadImage(image, value.model.slotAddress, &current, sizeof(current), true) || current != value.model.nativeModelInfo)
                return false;
            if (current)
            {
                if (NativeStreamingModelContext::Recheck(image, value.model, value.context) != ClockResult::Ok)
                    return false;
                std::array<unsigned char, 6> bytes{};
                std::uint8_t                 type = 0;
                if (!NativeModelTypeLeaf::Read(image, value.context.typeMethod, bytes, type) || bytes != value.typeBytes || (type == 3) != value.timed)
                    return false;
                if (value.timed)
                {
                    std::array<unsigned char, 4> observed{};
                    std::int32_t                 count = 0;
                    if (!Tuple(image, value, count, observed) || count != value.poolCount || observed != expected)
                        return false;
                }
                if (!NativeStreamingModelContext::MembershipUnchanged(image, value.context))
                    return false;
            }
            std::uint32_t state = 0;
            return NativeGameStateMemory::Read(image, state) && state == 9 && NativeStartGameCalls::CheckThread(image) == ClockResult::Ok;
        }
        bool ExchangeHours(std::uintptr_t at, std::uint16_t expected, std::uint16_t desired, std::uint16_t& observed, std::uint32_t& exception) noexcept
        {
            __try
            {
                SHORT from = 0, to = 0;
                std::memcpy(&from, &expected, sizeof(from));
                std::memcpy(&to, &desired, sizeof(to));
                const SHORT result = InterlockedCompareExchange16(reinterpret_cast<volatile SHORT*>(at), to, from);
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
    bool SameIdentity(const Value& a, const Value& b) noexcept
    {
        return a.generation == b.generation && a.model.modelIndex == b.model.modelIndex && a.model.slotAddress == b.model.slotAddress &&
               a.model.nativeModelInfo == b.model.nativeModelInfo && a.timed == b.timed && a.poolCount == b.poolCount && a.typeBytes == b.typeBytes &&
               NativeStreamingModelContext::Same(a.context, b.context) &&
               TimeMemoryDetail::Same(TimeMemoryDetail::Membership(a.context.rootMembership), TimeMemoryDetail::Membership(b.context.rootMembership)) &&
               TimeMemoryDetail::Same(TimeMemoryDetail::Membership(a.context.providerMembership), TimeMemoryDetail::Membership(b.context.providerMembership)) &&
               TimeMemoryDetail::Word(a.tuple, 2) == TimeMemoryDetail::Word(b.tuple, 2);
    }
    Origin Identity(const Value& value) noexcept
    {
        Origin origin;
        origin.generation = value.generation;
        origin.model = value.model.modelIndex;
        origin.slot = value.model.slotAddress;
        origin.metadata = value.model.nativeModelInfo;
        origin.vtable = value.context.modelVtable;
        origin.typeMethod = value.context.typeMethod;
        origin.animationMethod = value.context.animationMethod;
        origin.root = value.context.root;
        origin.provider = value.context.provider;
        origin.rows = value.context.rows;
        origin.count = value.context.count;
        origin.poolCount = value.poolCount;
        origin.rootMembership = TimeMemoryDetail::Membership(value.context.rootMembership);
        origin.providerMembership = TimeMemoryDetail::Membership(value.context.providerMembership);
        origin.typeBytes = value.typeBytes;
        origin.otherModel = TimeMemoryDetail::Word(value.tuple, 2);
        return origin;
    }
    bool SameStorageIdentity(const Origin& origin, const Value& current) noexcept
    {
        const auto next = Identity(current);
        return current.timed && origin.generation == next.generation && origin.metadata == next.metadata && origin.vtable == next.vtable &&
               origin.typeMethod == next.typeMethod && origin.animationMethod == next.animationMethod && origin.root == next.root &&
               origin.provider == next.provider && origin.rows == next.rows && origin.count == next.count && origin.poolCount == next.poolCount &&
               TimeMemoryDetail::Same(origin.rootMembership, next.rootMembership) &&
               TimeMemoryDetail::Same(origin.providerMembership, next.providerMembership) && origin.typeBytes == next.typeBytes &&
               origin.otherModel == next.otherModel;
    }
    bool SameIdentity(const Origin& origin, const Value& current) noexcept
    {
        return origin.model == current.model.modelIndex && origin.slot == current.model.slotAddress && SameStorageIdentity(origin, current);
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner afterApplication,
                        Value& output) noexcept
    {
        if (!image || !generation || !owner || !afterApplication)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        Value next;
        next.generation = generation;
        NativeModelBankReader bank({image}, owner);
        if ((result = bank.Read(model, next.model)) != ClockResult::Ok)
            return result;
        if (!TimeMemoryDetail::Profile(image))
            return ClockResult::UnsupportedBuild;
        if (next.model.nativeModelInfo)
        {
            if ((result = NativeStreamingModelContext::Capture(image, next.model, next.context)) != ClockResult::Ok)
                return result;
            std::uint8_t type = 0;
            if (!NativeModelTypeLeaf::Read(image, next.context.typeMethod, next.typeBytes, type))
                return ClockResult::UnsupportedBuild;
            if (type == 3)
            {
                // The independently observed second type3 vtable has another
                // accessor. A matching type byte alone cannot authorize this
                // first producer's allocated static row or its hour fields.
                if (next.context.modelVtable != image + NativeModelTimeProfile::VtableRva ||
                    next.context.typeMethod != image + NativeModelTimeProfile::TypeMethodRva)
                    return ClockResult::UnsupportedBuild;
                next.timed = true;
                if (!TimeMemoryDetail::Tuple(image, next, next.poolCount, next.tuple))
                    return ClockResult::BindingChanged;
            }
        }
        if ((result = bank.Recheck(next.model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        if (!TimeMemoryDetail::Anchors(image, next, next.tuple))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult Write(std::uintptr_t image, const Value& supplied, std::uint16_t desiredHours, CheckOwner owner, CheckOwner afterApplication, Receipt& output,
                      Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.before = supplied;
        receipt.desired = desiredHours;
        const auto finish = [&](ClockResult result) noexcept
        {
            receipt.result = result;
            output = receipt;
            return result;
        };
        if (!supplied.timed)
            return finish(ClockResult::InvalidArgument);
        Value current;
        auto  result = Capture(image, supplied.generation, supplied.model.modelIndex, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!SameIdentity(supplied, current) || supplied.tuple != current.tuple || !TimeMemoryDetail::Anchors(image, current, current.tuple))
            return finish(ClockResult::BindingChanged);
        const std::uint16_t before = TimeMemoryDetail::Word(current.tuple, 0);
        const auto          at = current.model.nativeModelInfo + NativeModelTimeProfile::TimeOffset;
        if (at % alignof(std::uint16_t))
            return finish(ClockResult::BindingChanged);
        std::uint16_t observed = 0;
        // One aligned CAS changes On/Off together and cannot overwrite the
        // adjacent parser-owned OtherModel word. No native getter or setter
        // is called, and no new normalization is imposed on MTA's bytes.
        receipt.entered = true;
        const bool returned = (exchange ? exchange : TimeMemoryDetail::ExchangeHours)(at, before, desiredHours, observed, receipt.exceptionCode);
        receipt.observed = observed;
        if (!returned)
        {
            receipt.uncertain = true;
            return finish(ClockResult::OriginalException);
        }
        receipt.accepted = observed == before;
        if (!receipt.accepted)
            return finish(ClockResult::OwnerConflict);
        Value after;
        result = Capture(image, supplied.generation, supplied.model.modelIndex, owner, afterApplication, after);
        if (result == ClockResult::Ok && (!SameIdentity(current, after) || TimeMemoryDetail::Word(after.tuple, 0) != desiredHours))
            result = ClockResult::BindingChanged;
        if (result != ClockResult::Ok)
        {
            receipt.uncertain = true;
            return finish(result);
        }
        return finish(ClockResult::Ok);
    }
}
