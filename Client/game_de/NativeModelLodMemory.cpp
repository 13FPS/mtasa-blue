/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Fresh typed model distance bits and one aligned DWORD CAS
 ****************************************************************************/
#include "NativeModelLodMemory.h"
#include "NativeModelLodProfile.h"
#include "NativeModelObjectGroupProfile.h"
#include "NativeModelTimeProfile.h"
#include "NativeModelWheelProfile.h"
#include "NativeModelTypeLeaf.h"
#include "NativeImageAccess.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <windows.h>
#include <cstring>
#include <iterator>
#include <limits>

namespace mta::game_de::NativeModelLodMemory
{
    namespace LodMemoryDetail
    {
        namespace P = NativeModelLodProfile;
        namespace A = NativeModelObjectGroupProfile;
        namespace T = NativeModelTimeProfile;
        namespace V = NativeModelWheelProfile;
        namespace I = NativeImageAccess;
        static_assert(FieldOffset == P::FieldOffset && P::FieldBytes == sizeof(std::uint32_t));
        static_assert(A::MaximumPinBytes >= T::MaximumPinBytes && A::MaximumPinBytes >= V::MaximumPinBytes && A::MaximumPinBytes >= P::MaximumPinBytes);

        template <class Range, class Contains>
        bool Pins(std::uintptr_t image, const Range& pins, const Contains& contains) noexcept
        {
            for (const auto& pin : pins)
            {
                std::array<unsigned char, A::MaximumPinBytes> actual{};
                if (!pin.size || pin.size > actual.size() ||
                    !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | (pin.code ? IMAGE_SCN_MEM_EXECUTE : 0),
                              IMAGE_SCN_MEM_WRITE | (pin.code ? 0 : IMAGE_SCN_MEM_EXECUTE)) ||
                    !I::ReadImage(image, image + pin.rva, actual.data(), pin.size, false, pin.code) || std::memcmp(actual.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        template <class Range, class Contains>
        bool Vtable(std::uintptr_t image, std::uintptr_t rva, const Range& methods, const Contains& contains) noexcept
        {
            const auto count = static_cast<std::size_t>(std::end(methods) - std::begin(methods));
            if (!contains(rva, count * sizeof(std::uintptr_t), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                return false;
            for (std::size_t index = 0; index != count; ++index)
            {
                std::uintptr_t pointer = 0;
                if (!I::ReadImage(image, image + rva + index * sizeof(pointer), &pointer, sizeof(pointer)) || pointer != image + methods[index])
                    return false;
            }
            return true;
        }
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
            const auto contains = [&](std::uintptr_t rva, std::size_t size, DWORD required, DWORD forbidden) noexcept
            {
                for (std::size_t index = 0; index != nt.FileHeader.NumberOfSections; ++index)
                    if (I::SectionContains(sections[index], rva, size, required, forbidden))
                        return true;
                return false;
            };
            const auto pool = [&](std::uintptr_t rva, std::size_t size, std::uintptr_t count) noexcept
            {
                return contains(rva, size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) &&
                       contains(count, sizeof(std::int32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE);
            };
            // Keep the existing complete producer/constructor profiles. The
            // independent IPL consumer proves the shared field's distance
            // role; an AL type getter by itself grants no storage authority.
            return pool(A::PoolRva, A::PoolBytes, A::PoolCountRva) && pool(T::PoolRva, T::PoolCapacity * T::RowBytes, T::PoolCountRva) &&
                   pool(V::PoolRva, V::PoolBytes, V::PoolCountRva) && Pins(image, A::Pins, contains) && Pins(image, T::Pins, contains) &&
                   Pins(image, V::Pins, contains) && Pins(image, P::Pins, contains) && Vtable(image, A::VtableRva, A::VtableMethods, contains) &&
                   Vtable(image, T::VtableRva, T::VtableMethods, contains) && Vtable(image, V::VtableRva, V::VtableMethods, contains) &&
                   NativeStreamingModelContext::QualifyProfile(image);
        }
        Family Select(std::uintptr_t image, const Value& value, std::uint8_t type) noexcept
        {
            if (type == 1 && value.context.modelVtable == image + A::VtableRva && value.context.typeMethod == image + A::TypeGetterRva &&
                value.context.animationMethod == image + A::VtableMethods[15])
                return Family::Atomic;
            // Only the existing first eight Time vtable slots are pinned.
            // The other observed type3 receiver is not this static producer.
            if (type == 3 && value.context.modelVtable == image + T::VtableRva && value.context.typeMethod == image + T::TypeMethodRva)
                return Family::Time;
            if (type == 6 && value.context.modelVtable == image + V::VtableRva && value.context.typeMethod == image + V::TypeGetterRva &&
                value.context.animationMethod == image + V::VtableMethods[15])
                return Family::Vehicle;
            return Family::Absent;
        }
        bool Field(std::uintptr_t image, const Value& value, std::int32_t& count, std::uint32_t& bits) noexcept
        {
            std::uintptr_t rva = 0, countRva = 0;
            std::size_t    capacity = 0, stride = 0;
            switch (value.family)
            {
                case Family::Atomic:
                    rva = A::PoolRva;
                    countRva = A::PoolCountRva;
                    capacity = A::PoolCapacity;
                    stride = A::PoolStride;
                    break;
                case Family::Time:
                    rva = T::PoolRva;
                    countRva = T::PoolCountRva;
                    capacity = T::PoolCapacity;
                    stride = T::RowBytes;
                    break;
                case Family::Vehicle:
                    rva = V::PoolRva;
                    countRva = V::PoolCountRva;
                    capacity = V::PoolCapacity;
                    stride = V::PoolStride;
                    break;
                default:
                    return false;
            }
            const auto base = image + rva;
            const auto metadata = value.model.nativeModelInfo;
            if (metadata < base || metadata - base >= capacity * stride || (metadata - base) % stride || !I::ImagePages(image, base, capacity * stride, true) ||
                !I::ReadImage(image, image + countRva, &count, sizeof(count), true) || count <= 0 || count > static_cast<std::int32_t>(capacity) ||
                (metadata - base) / stride >= static_cast<std::size_t>(count) || (metadata + P::FieldOffset) % alignof(std::uint32_t))
                return false;
            // Admit only an allocated constructor-backed image row. Read the
            // float representation as bits so signed zero, NaNs and MTA's
            // existing policy reach the native field without normalization.
            return I::ReadImage(image, metadata + P::FieldOffset, &bits, sizeof(bits), true);
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
        bool Anchors(std::uintptr_t image, const Value& value, std::uint32_t expected) noexcept
        {
            // Finish every owner callback before this fresh raw equality
            // fence and the immediate CAS. Saved plain identities never act
            // as membership tokens or permission to reuse old pointers.
            std::uintptr_t current = 0;
            if (!NativeModelBankReader::Revalidate({image}) || !Profile(image) ||
                !I::ReadImage(image, value.model.slotAddress, &current, sizeof(current), true) || current != value.model.nativeModelInfo)
                return false;
            if (current)
            {
                if (!value.present || NativeStreamingModelContext::Recheck(image, value.model, value.context) != ClockResult::Ok)
                    return false;
                std::array<unsigned char, 6> bytes{};
                std::uint8_t                 type = 0;
                std::uint32_t                bits = 0;
                std::int32_t                 count = 0;
                if (!NativeModelTypeLeaf::Read(image, value.context.typeMethod, bytes, type) || bytes != value.typeBytes ||
                    Select(image, value, type) != value.family || !Field(image, value, count, bits) || count != value.poolCount || bits != expected ||
                    !NativeStreamingModelContext::MembershipUnchanged(image, value.context))
                    return false;
            }
            else if (value.present || value.family != Family::Absent || expected)
                return false;
            std::uint32_t state = 0;
            return NativeGameStateMemory::Read(image, state) && state == 9 && NativeStartGameCalls::CheckThread(image) == ClockResult::Ok;
        }
        bool ExchangeBits(std::uintptr_t at, std::uint32_t expected, std::uint32_t desired, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            __try
            {
                LONG from = 0, to = 0;
                static_assert(sizeof(LONG) == sizeof(expected));
                std::memcpy(&from, &expected, sizeof(from));
                std::memcpy(&to, &desired, sizeof(to));
                const LONG result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), to, from);
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
               a.model.nativeModelInfo == b.model.nativeModelInfo && a.present == b.present && a.family == b.family && a.poolCount == b.poolCount &&
               a.typeBytes == b.typeBytes && NativeStreamingModelContext::Same(a.context, b.context) &&
               LodMemoryDetail::Same(LodMemoryDetail::Membership(a.context.rootMembership), LodMemoryDetail::Membership(b.context.rootMembership)) &&
               LodMemoryDetail::Same(LodMemoryDetail::Membership(a.context.providerMembership), LodMemoryDetail::Membership(b.context.providerMembership));
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
        origin.rootMembership = LodMemoryDetail::Membership(value.context.rootMembership);
        origin.providerMembership = LodMemoryDetail::Membership(value.context.providerMembership);
        origin.typeBytes = value.typeBytes;
        origin.family = value.family;
        return origin;
    }
    bool SameStorageIdentity(const Origin& origin, const Value& current) noexcept
    {
        const auto next = Identity(current);
        return current.present && current.family != Family::Absent && origin.generation == next.generation && origin.metadata == next.metadata &&
               origin.vtable == next.vtable && origin.typeMethod == next.typeMethod && origin.animationMethod == next.animationMethod &&
               origin.root == next.root && origin.provider == next.provider && origin.rows == next.rows && origin.count == next.count &&
               origin.poolCount == next.poolCount && LodMemoryDetail::Same(origin.rootMembership, next.rootMembership) &&
               LodMemoryDetail::Same(origin.providerMembership, next.providerMembership) && origin.typeBytes == next.typeBytes && origin.family == next.family;
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
        if (!LodMemoryDetail::Profile(image))
            return ClockResult::UnsupportedBuild;
        if (next.model.nativeModelInfo)
        {
            if ((result = NativeStreamingModelContext::Capture(image, next.model, next.context)) != ClockResult::Ok)
                return result;
            std::uint8_t type = 0;
            if (!NativeModelTypeLeaf::Read(image, next.context.typeMethod, next.typeBytes, type))
                return ClockResult::UnsupportedBuild;
            next.family = LodMemoryDetail::Select(image, next, type);
            // A real present receiver outside these three proven static
            // families is unsupported; it must not look like a missing key
            // with a successful zero-distance result.
            if (next.family == Family::Absent)
                return ClockResult::UnsupportedBuild;
            next.present = true;
            if (!LodMemoryDetail::Field(image, next, next.poolCount, next.bits))
                return ClockResult::BindingChanged;
        }
        if ((result = bank.Recheck(next.model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        if (!LodMemoryDetail::Anchors(image, next, next.bits))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult Write(std::uintptr_t image, const Value& supplied, std::uint32_t desiredBits, CheckOwner owner, CheckOwner afterApplication, Receipt& output,
                      Exchange exchange) noexcept
    {
        Receipt receipt;
        receipt.before = supplied;
        receipt.desired = desiredBits;
        const auto finish = [&](ClockResult result) noexcept
        {
            receipt.result = result;
            output = receipt;
            return result;
        };
        if (!supplied.present || supplied.family == Family::Absent)
            return finish(ClockResult::InvalidArgument);
        Value current;
        auto  result = Capture(image, supplied.generation, supplied.model.modelIndex, owner, afterApplication, current);
        if (result != ClockResult::Ok)
            return finish(result);
        if (!SameIdentity(supplied, current) || supplied.bits != current.bits || !LodMemoryDetail::Anchors(image, current, current.bits))
            return finish(ClockResult::BindingChanged);
        const auto    at = current.model.nativeModelInfo + NativeModelLodProfile::FieldOffset;
        std::uint32_t observed = 0;
        // One aligned DWORD CAS changes only the genuine distance field.
        // Float policy belongs to the existing SDK; no clamping or conversion
        // may erase the original representation required for later reset.
        receipt.entered = true;
        const bool returned = (exchange ? exchange : LodMemoryDetail::ExchangeBits)(at, current.bits, desiredBits, observed, receipt.exceptionCode);
        receipt.observed = observed;
        if (!returned)
        {
            receipt.uncertain = true;
            return finish(ClockResult::OriginalException);
        }
        receipt.accepted = observed == current.bits;
        if (!receipt.accepted)
            return finish(ClockResult::OwnerConflict);
        Value after;
        result = Capture(image, supplied.generation, supplied.model.modelIndex, owner, afterApplication, after);
        if (result == ClockResult::Ok && (!SameIdentity(current, after) || after.bits != desiredBits))
            result = ClockResult::BindingChanged;
        if (result != ClockResult::Ok)
        {
            receipt.uncertain = true;
            return finish(result);
        }
        return finish(ClockResult::Ok);
    }
}
