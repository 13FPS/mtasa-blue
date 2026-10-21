/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve raw native physical properties and persistent partial effects
 ****************************************************************************/
#include "NativePedPhysicalScalarsMemory.h"
#include "NativePedPhysicalScalarsProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <cstring>
#include <limits>
#include <intrin.h>
#include <type_traits>

namespace mta::game_de
{
    namespace PedPhysicalScalarsDetail
    {
        namespace P = NativePedPhysicalScalarsProfile;
        namespace Image = NativeImageAccess;
        using Memory = NativePedPhysicalScalarsMemory;
        using Value = NativePedPoolReader::Value;
        using Bits = std::array<std::uint32_t, 3>;
        thread_local bool writing = false;
        struct WriteScope
        {
            WriteScope() noexcept { writing = true; }
            ~WriteScope() { writing = false; }
        };
        static_assert(sizeof(float) == sizeof(std::uint32_t) && sizeof(LONG) == sizeof(std::uint32_t));

        static_assert(std::is_standard_layout_v<CVector> && std::is_trivially_copyable_v<CVector> && sizeof(CVector) == 12 && alignof(CVector) == 4);
        static_assert(offsetof(CVector, fX) == 0 && offsetof(CVector, fY) == 4 && offsetof(CVector, fZ) == 8);
        static_assert(P::CenterOfMassOffset % alignof(std::uint32_t) == 0 && P::VectorBytes == sizeof(CVector));

        bool Field(NativePedPhysicalScalarField field, std::size_t& offset) noexcept
        {
            switch (field)
            {
                case NativePedPhysicalScalarField::Mass:
                    offset = P::MassOffset;
                    return true;
                case NativePedPhysicalScalarField::TurnMass:
                    offset = P::TurnMassOffset;
                    return true;
                case NativePedPhysicalScalarField::AirResistance:
                    offset = P::AirResistanceOffset;
                    return true;
                case NativePedPhysicalScalarField::Elasticity:
                    offset = P::ElasticityOffset;
                    return true;
                default:
                    return false;
            }
        }
        bool SamePed(const Value& a, const Value& b) noexcept
        {
            return a.handle == b.handle && a.slot == b.slot && a.pool == b.pool && a.objects == b.objects && a.flags == b.flags && a.nativePed == b.nativePed &&
                   a.vtable == b.vtable && a.firstMethod == b.firstMethod && a.capacity == b.capacity && a.cursor == b.cursor && a.generation == b.generation &&
                   a.ownsAllocations == b.ownsAllocations;
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
        template <class T>
        bool Object(std::uintptr_t address, T& output) noexcept
        {
            T      next{};
            SIZE_T copied = 0;
            if (!Pages(address, sizeof(next)) || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &next, sizeof(next), &copied) ||
                copied != sizeof(next))
                return false;
            output = next;
            return true;
        }
        ClockResult Owner(Memory::Binding binding, Memory::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
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
            return owner();
        }
        ClockResult Identity(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected) noexcept
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            // Independent force, damping and contact-impulse effects identify
            // this layout; the exact common constructor binds both known
            // ped classes. No CURRENT-player substitution supplies authority.
            if (expected.vtable != binding.image + P::PedVtableRva && expected.vtable != binding.image + P::PlayerVtableRva)
                return ClockResult::UnsupportedBuild;
            NativePedPoolReader pool(binding, owner);
            Value               current;
            if ((result = pool.Resolve(expected.handle, current)) != ClockResult::Ok)
                return result;
            if (!SamePed(expected, current))
                return ClockResult::BindingChanged;
            if ((result = pool.Recheck(current)) != ClockResult::Ok)
                return result;
            return Owner(binding, owner);
        }
        struct Snapshot
        {
            std::uint32_t bits = 0;
            unsigned char type = 0;
        };
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected, std::size_t offset, Snapshot& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            Snapshot      next;
            unsigned char after = 0;
            if (!Object(expected.nativePed + P::TypeOffset, next.type) || !Object(expected.nativePed + offset, next.bits) ||
                !Object(expected.nativePed + P::TypeOffset, after))
                return ClockResult::ReadFailure;
            if ((next.type & 7) != 3 || next.type != after)
                return ClockResult::BindingChanged;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        struct VectorSnapshot
        {
            Bits          bits{};
            unsigned char type = 0;
        };
        ClockResult CaptureVector(Memory::Binding binding, Memory::CheckOwner owner, const Value& expected, VectorSnapshot& output) noexcept
        {
            auto result = Identity(binding, owner, expected);
            if (result != ClockResult::Ok)
                return result;
            VectorSnapshot next;
            unsigned char  after = 0;
            if (!Object(expected.nativePed + P::TypeOffset, next.type) || !Object(expected.nativePed + P::CenterOfMassOffset, next.bits) ||
                !Object(expected.nativePed + P::TypeOffset, after))
                return ClockResult::ReadFailure;
            if ((next.type & 7) != 3 || next.type != after)
                return ClockResult::BindingChanged;
            if ((result = Identity(binding, owner, expected)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Source(const CVector& source, Bits& output) noexcept
        {
            Bits   first{}, second{};
            SIZE_T copied = 0;
            // Capture all caller bits twice before any destination mutation.
            // In particular, a native self/overlap alias must not become a
            // partly updated source after the first accepted component.
            if (!ReadProcessMemory(GetCurrentProcess(), &source, first.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) ||
                !ReadProcessMemory(GetCurrentProcess(), &source, second.data(), sizeof(CVector), &copied) || copied != sizeof(CVector) || first != second)
                return false;
            output = second;
            return true;
        }
#if !defined(MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE)
        bool Exchange(std::uintptr_t address, std::uint32_t expected, std::uint32_t target, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            LONG before = 0, after = 0, result = 0;
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
            std::memcpy(&observed, &result, sizeof(observed));
            return true;
        }
#endif
    }

    bool NativePedPhysicalScalarsMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativePedPhysicalScalarsMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace Image = NativeImageAccess;
        if (!NativePedPoolReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!Image::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !Image::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) ||
            nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !Image::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : NativePedPhysicalScalarsProfile::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, NativePedPhysicalScalarsProfile::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        bool constantContained = false;
        for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
            if (Image::SectionContains(sections[i], NativePedPhysicalScalarsProfile::InverseMassOneRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ,
                                       IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
                constantContained = true;
        std::uint32_t one = 0;
        if (!constantContained || !Image::ReadImage(binding.image, binding.image + NativePedPhysicalScalarsProfile::InverseMassOneRva, &one, sizeof(one)) ||
            one != NativePedPhysicalScalarsProfile::InverseMassOneBits)
            return false;
        return true;
    }
    ClockResult NativePedPhysicalScalarsMemory::Read(const Value& expected, NativePedPhysicalScalarField field, float& output) const noexcept
    {
        namespace D = PedPhysicalScalarsDetail;
        std::size_t offset = 0;
        if (!D::Field(field, offset))
            return ClockResult::InvalidArgument;
        D::Snapshot first, second;
        auto        result = D::Capture(binding, owner, expected, offset, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, expected, offset, second)) != ClockResult::Ok)
            return result;
        if (first.type != second.type || first.bits != second.bits)
            return ClockResult::OwnerConflict;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // The actual CPhysical accessors copy raw float state, including NaN
        // payloads and signed zero. Arithmetic comparisons would change that
        // contract or manufacture a plausible clamped physical property.
        std::memcpy(&output, &second.bits, sizeof(output));
        return ClockResult::Ok;
    }
    ClockResult NativePedPhysicalScalarsMemory::Set(const Value& expected, NativePedPhysicalScalarField field, float value,
                                                    Receipt* receiptOutput) const noexcept
    {
        namespace D = PedPhysicalScalarsDetail;
        Receipt receipt;
        receipt.field = field;
        receipt.handle = expected.handle;
        std::memcpy(&receipt.target, &value, sizeof(value));
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        std::size_t offset = 0;
        if (!D::Field(field, offset))
            return finish(ClockResult::InvalidArgument);
        D::WriteScope scope;
        D::Snapshot   first, second;
        auto          result = D::Capture(binding, owner, expected, offset, first);
        if (result != ClockResult::Ok)
            return finish(result);
        receipt.original = first.bits;
        receipt.nativePed = expected.nativePed;
        receipt.address = expected.nativePed + offset;
        if ((result = D::Capture(binding, owner, expected, offset, second)) != ClockResult::Ok)
            return finish(result);
        receipt.observed = second.bits;
        receipt.observedValid = true;
        if (first.type != second.type || first.bits != second.bits)
            return finish(ClockResult::OwnerConflict);
        if (receipt.address % alignof(std::uint32_t) || !D::Pages(receipt.address, sizeof(std::uint32_t)))
            return finish(ClockResult::WriteFailure);
        if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
            return finish(result);
#if defined(MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE)
        const auto exchange = &MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        // One exact scalar CAS implements persistent caller intent. Even a
        // failed acknowledgement can follow a committed store: mark entry
        // first and never retry, resurrect, or restore an old native snapshot.
        receipt.entered = receipt.attempted = true;
        std::uint32_t previous = 0;
        if (!exchange(receipt.address, receipt.original, receipt.target, previous, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observed = previous;
        receipt.observedValid = true;
        if (previous != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.applied = true;
        D::Snapshot final;
        if ((result = D::Capture(binding, owner, expected, offset, final)) != ClockResult::Ok)
            return finish(result);
        receipt.observed = final.bits;
        if (final.type != first.type || final.bits != receipt.target)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
    ClockResult NativePedPhysicalScalarsMemory::ReadCenterOfMass(const Value& expected, CVector& output) const noexcept
    {
        namespace D = PedPhysicalScalarsDetail;
        D::VectorSnapshot first, second;
        auto              result = D::CaptureVector(binding, owner, expected, first);
        if (result != ClockResult::Ok || (result = D::CaptureVector(binding, owner, expected, second)) != ClockResult::Ok)
            return result;
        if (first.type != second.type || first.bits != second.bits)
            return ClockResult::OwnerConflict;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        std::memcpy(&output, second.bits.data(), sizeof(output));
        return ClockResult::Ok;
    }
    ClockResult NativePedPhysicalScalarsMemory::SetCenterOfMass(const Value& expected, const CVector& source, VectorReceipt* receiptOutput) const noexcept
    {
        namespace D = PedPhysicalScalarsDetail;
        VectorReceipt receipt;
        receipt.handle = expected.handle;
        const auto finish = [&](ClockResult result)
        {
            if (receiptOutput)
                *receiptOutput = receipt;
            return result;
        };
        if (D::writing)
            return finish(ClockResult::Busy);
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        D::WriteScope scope;
        if (!D::Source(source, receipt.target))
            return finish(ClockResult::ReadFailure);
        D::VectorSnapshot original, current;
        if ((result = D::CaptureVector(binding, owner, expected, original)) != ClockResult::Ok)
            return finish(result);
        receipt.nativePed = expected.nativePed;
        receipt.address = expected.nativePed + D::P::CenterOfMassOffset;
        receipt.original = original.bits;
        if (receipt.address % alignof(std::uint32_t))
            return finish(ClockResult::BindingChanged);
#if defined(MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE)
        const auto exchange = &MTA_DE_PED_PHYSICAL_SCALARS_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        for (unsigned index = 0; index != 3; ++index)
        {
            if ((result = D::CaptureVector(binding, owner, expected, current)) != ClockResult::Ok)
                return finish(result);
            if (original.type != current.type)
                return finish(ClockResult::BindingChanged);
            receipt.observed = current.bits;
            receipt.observedMask = 7;
            // Preserve any later engine/foreign component. Accepted earlier
            // components stay accepted even when this vector cannot finish.
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
            const auto address = receipt.address + index * sizeof(std::uint32_t);
            if (!D::Pages(address, sizeof(std::uint32_t)))
                return finish(ClockResult::WriteFailure);
            if ((result = D::Identity(binding, owner, expected)) != ClockResult::Ok)
                return finish(result);
            std::uint32_t previous = 0;
            receipt.attemptedMask |= bit;
            receipt.entered = true;
            // Three width-checked scalar CASes copy the native vector. This
            // is deliberately no atomic-vector or cleanup-repair promise:
            // an unknown/failed acknowledgement can follow a real store.
            if (!exchange(address, receipt.original[index], receipt.target[index], previous, receipt.exceptionCode))
                return finish(ClockResult::WriteFailure);
            if (previous != receipt.original[index])
            {
                receipt.foreignMask |= bit;
                return finish(ClockResult::OwnerConflict);
            }
            receipt.appliedMask |= bit;
        }
        if ((result = D::CaptureVector(binding, owner, expected, current)) != ClockResult::Ok)
            return finish(result);
        if (current.type != original.type)
            return finish(ClockResult::BindingChanged);
        receipt.observed = current.bits;
        receipt.observedMask = 7;
        for (unsigned index = 0; index != 3; ++index)
            if (current.bits[index] != receipt.target[index])
                receipt.foreignMask |= static_cast<std::uint8_t>(1u << index);
        if (receipt.foreignMask)
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }

}
