/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve native physical vector units and persistent partial writes
 ****************************************************************************/
#include "NativePedVelocityMemory.h"
#include "NativePedVelocityProfile.h"
#include "NativePedTransformProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <cstring>
#include <limits>
#include <type_traits>

namespace mta::game_de
{
    namespace PedVelocityDetail
    {
        namespace P = NativePedVelocityProfile;
        namespace T = NativePedTransformProfile;
        namespace Image = NativeImageAccess;
        using Bits = std::array<std::uint32_t, 3>;
        thread_local bool writing = false;
        struct WriteScope
        {
            WriteScope() noexcept { writing = true; }
            ~WriteScope() { writing = false; }
        };
        static_assert(std::is_standard_layout_v<CVector> && std::is_trivially_copyable_v<CVector> && sizeof(CVector) == 12 && alignof(CVector) == 4);
        static_assert(offsetof(CVector, fX) == 0 && offsetof(CVector, fY) == 4 && offsetof(CVector, fZ) == 8);
        static_assert(P::LinearOffset == T::VelocityOffset && P::LinearOffset % alignof(std::uint32_t) == 0 && P::AngularOffset % alignof(std::uint32_t) == 0);

        bool Kind(NativePedVelocityKind kind, std::size_t& offset) noexcept
        {
            switch (kind)
            {
                case NativePedVelocityKind::Linear:
                    offset = P::LinearOffset;
                    return true;
                case NativePedVelocityKind::Angular:
                    offset = P::AngularOffset;
                    return true;
                default:
                    return false;
            }
        }
        bool Pages(std::uintptr_t address, std::size_t bytes) noexcept
        {
            if (!address || !bytes || bytes > (std::numeric_limits<std::uintptr_t>::max)() - address)
                return false;
            const auto     end = address + bytes;
            std::uintptr_t allocation = 0;
            while (address < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) != sizeof(info) || info.State != MEM_COMMIT ||
                    (info.Type != MEM_PRIVATE && info.Type != MEM_MAPPED) || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                    (info.Protect & 0xff) != PAGE_READWRITE)
                    return false;
                const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
                const auto current = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
                if (!current || (allocation && allocation != current) || base > address ||
                    info.RegionSize > (std::numeric_limits<std::uintptr_t>::max)() - base || base + info.RegionSize <= address)
                    return false;
                allocation = current;
                address = base + info.RegionSize < end ? base + info.RegionSize : end;
            }
            return true;
        }
        template <class Value>
        bool Read(std::uintptr_t address, Value& output) noexcept
        {
            Value  next{};
            SIZE_T copied = 0;
            if (!Pages(address, sizeof(next)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        bool Source(const CVector& source, Bits& output) noexcept
        {
            Bits   first{}, second{};
            SIZE_T copied = 0;
            // Capture the complete source before the first destination write.
            // This preserves self/overlap aliases without reading a partly
            // changed source after one accepted component.
            if (!ReadProcessMemory(GetCurrentProcess(), &source, first.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) ||
                !ReadProcessMemory(GetCurrentProcess(), &source, second.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) || first != second)
                return false;
            output = second;
            return true;
        }
        bool SamePed(const NativePedPoolReader::Value& a, const NativePedPoolReader::Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
        }
        ClockResult Owner(NativePedVelocityMemory::Binding binding, NativePedVelocityMemory::CheckOwner check) noexcept
        {
            if (!check)
                return ClockResult::NotInitialized;
            auto result = check();
            if (result != ClockResult::Ok)
                return result;
            result = NativeStartGameCalls::CheckThread(binding.image);
            if (result != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            return check();
        }
        struct Snapshot
        {
            NativePedPoolReader::Value ped;
            Bits                       bits{};
            unsigned char              type = 0;
        };
        ClockResult Capture(NativePedVelocityMemory::Binding binding, NativePedVelocityMemory::CheckOwner check, std::uint32_t handle, std::size_t offset,
                            Snapshot& output) noexcept
        {
            auto result = Owner(binding, check);
            if (result != ClockResult::Ok)
                return result;
            if (!NativePedVelocityMemory::Revalidate(binding))
                return ClockResult::BindingChanged;
            Snapshot            next;
            NativePedPoolReader pool(binding, check);
            if ((result = pool.Resolve(handle, next.ped)) != ClockResult::Ok)
                return result;
            if (next.ped.vtable != binding.image + T::PedVtableRva && next.ped.vtable != binding.image + T::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            unsigned char typeAfter = 0;
            if (!Read(next.ped.nativePed + P::TypeOffset, next.type) || !Read(next.ped.nativePed + offset, next.bits) ||
                !Read(next.ped.nativePed + P::TypeOffset, typeAfter))
                return ClockResult::ReadFailure;
            // Object setters have additional moving/static behavior in MTA.
            // This native prerequisite admits real occupied peds only, and
            // does not silently claim those unrelated object side effects.
            if ((next.type & 7) != 3 || next.type != typeAfter)
                return ClockResult::BindingChanged;
            if ((result = pool.Recheck(next.ped)) != ClockResult::Ok || (result = Owner(binding, check)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& previous, std::uint32_t& exception) noexcept
        {
            LONG before = 0, after = 0, result = 0;
            static_assert(sizeof(LONG) == sizeof(expected));
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &target, sizeof(after));
            __try
            {
                result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), after, before);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
            std::memcpy(&previous, &result, sizeof(previous));
            return true;
        }
    }
    bool NativePedVelocityMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedTransformReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedVelocityMemory::Revalidate(const Binding& binding) noexcept
    {
        if (!NativePedTransformReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!NativeImageAccess::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !NativeImageAccess::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !NativeImageAccess::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : NativePedVelocityProfile::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (NativeImageAccess::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, NativePedVelocityProfile::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !NativeImageAccess::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativePedVelocityMemory::Read(std::uint32_t handle, NativePedVelocityKind kind, CVector& output) const noexcept
    {
        namespace D = PedVelocityDetail;
        std::size_t offset = 0;
        if (!D::Kind(kind, offset))
            return ClockResult::InvalidArgument;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, handle, offset, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, handle, offset, second)) != ClockResult::Ok)
            return result;
        if (!D::SamePed(first.ped, second.ped) || first.type != second.type || first.bits != second.bits)
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // Raw bits and native units survive, including signed zero/nonfinite
        // input needed by a future real SDK physical-state repair policy.
        std::memcpy(&output, second.bits.data(), sizeof(output));
        return ClockResult::Ok;
    }
    ClockResult NativePedVelocityMemory::Set(std::uint32_t handle, NativePedVelocityKind kind, const CVector& source, Receipt* receiptOutput) const noexcept
    {
        namespace D = PedVelocityDetail;
        Receipt receipt;
        receipt.handle = handle;
        receipt.kind = kind;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        std::size_t offset = 0;
        if (!D::Kind(kind, offset))
            return finish(ClockResult::InvalidArgument);
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        D::WriteScope scope;
        if (!D::Source(source, receipt.target))
            return finish(ClockResult::ReadFailure);
        D::Snapshot original, current;
        if ((result = D::Capture(binding, owner, handle, offset, original)) != ClockResult::Ok)
            return finish(result);
        receipt.nativePed = original.ped.nativePed;
        receipt.address = original.ped.nativePed + offset;
        receipt.original = original.bits;
        if (receipt.address % alignof(std::uint32_t))
            return finish(ClockResult::BindingChanged);
        for (unsigned index = 0; index != 3; ++index)
        {
            if ((result = D::Capture(binding, owner, handle, offset, current)) != ClockResult::Ok)
                return finish(result);
            if (!D::SamePed(original.ped, current.ped) || original.type != current.type)
                return finish(ClockResult::BindingChanged);
            receipt.observed = current.bits;
            receipt.observedMask = 7;
            for (unsigned prior = 0; prior != index; ++prior)
                if (current.bits[prior] != receipt.target[prior])
                    receipt.foreignMask |= static_cast<std::uint8_t>(1u << prior);
            const auto bit = static_cast<std::uint8_t>(1u << index);
            if (current.bits[index] != receipt.original[index])
                receipt.foreignMask |= bit;
            if (receipt.foreignMask)
                return finish(ClockResult::OwnerConflict);
            if (receipt.original[index] == receipt.target[index])
            {
                receipt.unchangedMask |= bit;
                continue;
            }
            if (!D::Pages(receipt.address + index * sizeof(std::uint32_t), sizeof(std::uint32_t)))
                return finish(ClockResult::WriteFailure);
            if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
                return finish(result);
            std::uint32_t previous = 0;
            receipt.attemptedMask |= bit;
            receipt.entered = true;
            if (!D::Exchange(receipt.address + index * sizeof(std::uint32_t), receipt.original[index], receipt.target[index], previous, receipt.exceptionCode))
                return finish(ClockResult::WriteFailure);
            if (previous != receipt.original[index])
            {
                receipt.foreignMask |= bit;
                return finish(ClockResult::OwnerConflict);
            }
            receipt.appliedMask |= bit;
        }
        if ((result = D::Capture(binding, owner, handle, offset, current)) != ClockResult::Ok)
            return finish(result);
        if (!D::SamePed(original.ped, current.ped) || original.type != current.type)
            return finish(ClockResult::BindingChanged);
        receipt.observed = current.bits;
        receipt.observedMask = 7;
        for (unsigned i = 0; i != 3; ++i)
            if (current.bits[i] != receipt.target[i])
                receipt.foreignMask |= static_cast<std::uint8_t>(1u << i);
        if (receipt.foreignMask)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
