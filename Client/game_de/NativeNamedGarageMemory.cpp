/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve selected named garage bytes without inventing native capacity or SDK ID mapping
 ****************************************************************************/
#include "NativeNamedGarageMemory.h"
#include "NativeNamedGarageProfile.h"
#include "NativeNamedGarageGeometryProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <game/CGarageDoorStatePolicy.h>
#include <array>
#include <cstring>
#include <limits>
#include <intrin.h>
namespace mta::game_de
{
    namespace NamedGarageDetail
    {
        namespace P = NativeNamedGarageProfile;
        namespace I = NativeImageAccess;
        using Memory = NativeNamedGarageMemory;
        struct Value
        {
            std::uint32_t    count = 0, index = 0;
            std::uintptr_t   row = 0;
            NativeGarageName name{};
            std::uint8_t     type = 0, state = 0;
        };
        thread_local bool writing = false;
        struct Scope
        {
            Scope() noexcept { writing = true; }
            ~Scope() { writing = false; }
        };
        bool ValidName(const NativeGarageName& name) noexcept
        {
            for (const auto byte : name)
                if (!byte)
                    return true;
            return false;
        }
        bool EqualName(const NativeGarageName& left, const NativeGarageName& right) noexcept
        {
            // This reproduces the bounded domain of the genuine named lookup:
            // exact non-ASCII bytes, and its pinned ASCII casefold table only.
            for (std::size_t i = 0; i != left.size(); ++i)
            {
                const auto a = left[i], b = right[i];
                if (a == b)
                {
                    if (!a)
                        return true;
                }
                else if (a >= 128 || b >= 128 || P::Fold[a] != P::Fold[b])
                    return false;
            }
            return false;
        }
        ClockResult Owner(Memory::Binding binding, Memory::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok || (result = NativeStartGameCalls::CheckThread(binding.image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            return owner();
        }
        bool SameIdentity(const Value& a, const Value& b) noexcept
        {
            return a.count == b.count && a.index == b.index && a.row == b.row && a.name == b.name;
        }
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const NativeGarageName& name, Value& output) noexcept
        {
            auto result = Owner(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            std::uint32_t count = 0;
            if (!I::ReadImage(binding.image, binding.image + P::CountRva, &count, sizeof(count), true))
                return ClockResult::ReadFailure;
            // The producer publishes DWORD count, but lookup compares a signed
            // WORD. Refuse truncation/negative lookup domains; do not silently
            // alias a large native count into a smaller invented array.
            if (count > P::MaximumLookupCount)
                return ClockResult::BindingChanged;
            Value next;
            next.count = count;
            bool found = false;
            for (std::uint32_t index = 0; index != count; ++index)
            {
                const auto rva = P::BankRva + static_cast<std::uintptr_t>(index) * P::RowBytes;
                if (rva > NativePedPoolProfile::ImageSize || P::DoorStateOffset + 1 > NativePedPoolProfile::ImageSize - rva ||
                    binding.image > (std::numeric_limits<std::uintptr_t>::max)() - rva - P::DoorStateOffset)
                    return ClockResult::ReadFailure;
                NativeGarageName candidate{};
                const auto       row = binding.image + rva;
                if (!I::ReadImage(binding.image, row + P::NameOffset, candidate.data(), candidate.size(), true))
                    return ClockResult::ReadFailure;
                // No unbounded native strcmp is called on an invalid field.
                // Unterminated rows remain unsupported rather than reading
                // through adjacent type/state bytes looking for a terminator.
                if (!ValidName(candidate))
                    return ClockResult::BindingChanged;
                if (!EqualName(name, candidate))
                    continue;
                next.index = index;
                next.row = row;
                next.name = candidate;
                std::array<std::uint8_t, 2> bytes{};
                if (!I::ReadImage(binding.image, row + P::TypeOffset, bytes.data(), bytes.size(), true))
                    return ClockResult::ReadFailure;
                next.type = bytes[0];
                next.state = bytes[1];
                found = true;
                break;
            }
            std::uint32_t after = 0;
            if (!I::ReadImage(binding.image, binding.image + P::CountRva, &after, sizeof(after), true))
                return ClockResult::ReadFailure;
            if (after != count || !Memory::Revalidate(binding))
                return ClockResult::BindingChanged;
            if ((result = Owner(binding, owner)) != ClockResult::Ok)
                return result;
            if (!found)
                return ClockResult::InvalidArgument;
            output = next;
            return ClockResult::Ok;
        }
#if !defined(MTA_DE_NAMED_GARAGE_EXCHANGE)
        bool Exchange(std::uintptr_t address, std::uint8_t expected, std::uint8_t target, std::uint8_t& observed, std::uint32_t& exception) noexcept
        {
            char before = 0, after = 0;
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &target, sizeof(after));
            __try
            {
                const auto previous = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address), after, before);
                std::memcpy(&observed, &previous, sizeof(observed));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                exception = GetExceptionCode();
                return false;
            }
        }
#endif
    }
    bool NativeNamedGarageMemory::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativePedPoolReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeNamedGarageMemory::Revalidate(const Binding& binding) noexcept
    {
        namespace D = NamedGarageDetail;
        namespace P = NativeNamedGarageProfile;
        if (!NativePedPoolReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!D::I::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !D::I::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
            !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !D::I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t count, DWORD required, DWORD forbidden)
        {
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (D::I::SectionContains(sections[i], rva, count, required, forbidden))
                    return true;
            return false;
        };
        // Only the producer's count and the statically named first row are
        // qualified here. Dynamic per-row reads follow that published count;
        // section extent is a memory check, never an asserted bank capacity.
        if (!contains(P::CountRva, sizeof(std::uint32_t), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
            !contains(P::BankRva, P::DoorStateOffset + 1, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
            !contains(P::FoldRva, sizeof(P::Fold), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE))
            return false;
        for (const auto& pin : P::Code)
        {
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!pin.size || pin.size > actual.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !D::I::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        std::array<unsigned char, sizeof(P::Fold)> fold{};
        return D::I::ReadImage(binding.image, binding.image + P::FoldRva, fold.data(), fold.size()) && std::memcmp(fold.data(), P::Fold, fold.size()) == 0;
    }
    ClockResult NativeNamedGarageMemory::Read(const NativeGarageName& name, bool door, std::uint8_t& output) const noexcept
    {
        namespace D = NamedGarageDetail;
        if (!D::ValidName(name))
            return ClockResult::InvalidArgument;
        D::Value first, second;
        auto     result = D::Capture(binding, owner, name, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, name, second)) != ClockResult::Ok)
            return result;
        if (!D::SameIdentity(first, second) || first.type != second.type || (door && first.state != second.state))
            return ClockResult::OwnerConflict;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        output = door ? second.state : second.type;
        return ClockResult::Ok;
    }
    ClockResult NativeNamedGarageMemory::ReadType(const NativeGarageName& name, std::uint8_t& output) const noexcept
    {
        return Read(name, false, output);
    }
    ClockResult NativeNamedGarageMemory::ReadOpen(const NativeGarageName& name, bool& output) const noexcept
    {
        std::uint8_t state = 0;
        const auto   result = Read(name, true, state);
        if (result != ClockResult::Ok)
            return result;
        output = MtaGame::CGarageDoorStatePolicy::IsOpen(state);
        return ClockResult::Ok;
    }
    ClockResult NativeNamedGarageMemory::SetType(const NativeGarageName& name, std::uint8_t type, Receipt* receipt) const noexcept
    {
        return Set(name, false, type, receipt);
    }
    ClockResult NativeNamedGarageMemory::SetOpen(const NativeGarageName& name, bool open, Receipt* receipt) const noexcept
    {
        return Set(name, true, MtaGame::CGarageDoorStatePolicy::StateForOpen(open), receipt);
    }
    ClockResult NativeNamedGarageMemory::Set(const NativeGarageName& name, bool door, std::uint8_t target, Receipt* output) const noexcept
    {
        namespace D = NamedGarageDetail;
        Receipt receipt;
        receipt.target = target;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        if (!D::ValidName(name))
            return finish(ClockResult::InvalidArgument);
        if (D::writing)
            return finish(ClockResult::Busy);
        D::Scope scope;
        D::Value first, current;
        auto     result = D::Capture(binding, owner, name, first);
        if (result != ClockResult::Ok || (result = D::Capture(binding, owner, name, current)) != ClockResult::Ok)
            return finish(result);
        if (!D::SameIdentity(first, current) || first.type != current.type || (door && first.state != current.state))
            return finish(ClockResult::OwnerConflict);
        receipt.count = current.count;
        receipt.index = current.index;
        receipt.row = current.row;
        receipt.address = current.row + (door ? D::P::DoorStateOffset : D::P::TypeOffset);
        receipt.original = door ? current.state : current.type;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return finish(result);
        if (!Revalidate(binding) || !NativeImageAccess::ImagePages(binding.image, receipt.address, sizeof(std::uint8_t), true))
            return finish(ClockResult::BindingChanged);
        D::Value entry;
        if ((result = D::Capture(binding, owner, name, entry)) != ClockResult::Ok)
            return finish(result);
        if (!D::SameIdentity(current, entry) || current.type != entry.type || (door && current.state != entry.state))
            return finish(ClockResult::OwnerConflict);
#if defined(MTA_DE_NAMED_GARAGE_EXCHANGE)
        const auto exchange = &MTA_DE_NAMED_GARAGE_EXCHANGE;
#else
        const auto exchange = &D::Exchange;
#endif
        // One exact BYTE CAS avoids touching the adjacent type/state/flags or
        // cloning the script command's extra transitions. A failed or accepted
        // attempt never retries, including a post-entry identity revocation.
        receipt.entered = true;
        if (!exchange(receipt.address, receipt.original, receipt.target, receipt.observed, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observedValid = true;
        if (receipt.observed != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        D::Value after;
        if ((result = D::Capture(binding, owner, name, after)) != ClockResult::Ok)
            return finish(result);
        if (!D::SameIdentity(current, after) || after.type != (door ? current.type : target) || (door && after.state != target))
            return finish(ClockResult::OwnerConflict);
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
    bool NativeNamedGarageMemory::QualifyGeometry(Binding& output) noexcept
    {
        Binding next;
        if (!Qualify(next) || !RevalidateGeometry(next))
            return false;
        output = next;
        return true;
    }
    bool NativeNamedGarageMemory::RevalidateGeometry(const Binding& binding) noexcept
    {
        namespace P = NativeNamedGarageGeometryProfile;
        namespace I = NativeImageAccess;
        if (!Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!I::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !I::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt.OptionalHeader.SizeOfImage != NativePedPoolProfile::ImageSize || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
            !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || bytes > NativePedPoolProfile::ImageSize - at ||
            !I::ReadImage(binding.image, binding.image + at, sections.data(), bytes))
            return false;
        for (const auto& pin : P::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (I::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !I::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) || std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativeNamedGarageMemory::ReadGeometry(const NativeGarageName& name, NativeGarageGeometry& output) const noexcept
    {
        namespace D = NamedGarageDetail;
        namespace P = NativeNamedGarageGeometryProfile;
        if (!D::ValidName(name))
            return ClockResult::InvalidArgument;
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        if (!RevalidateGeometry(binding))
            return ClockResult::BindingChanged;
        D::Value             before, middle, after;
        NativeGarageGeometry first, second;
        const auto           copy = [&](std::uintptr_t row, NativeGarageGeometry& value)
        {
            SIZE_T copied = 0;
            return NativeImageAccess::ImagePages(binding.image, row, P::GeometryBytes, true) &&
                   ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(row), value.words.data(), P::GeometryBytes, &copied) &&
                   copied == P::GeometryBytes;
        };
        if ((result = D::Capture(binding, owner, name, before)) != ClockResult::Ok)
            return result;
        if (!copy(before.row, first))
            return ClockResult::ReadFailure;
        if ((result = D::Capture(binding, owner, name, middle)) != ClockResult::Ok)
            return result;
        if (!D::SameIdentity(before, middle) || before.type != middle.type)
            return ClockResult::OwnerConflict;
        if (!copy(middle.row, second))
            return ClockResult::ReadFailure;
        if ((result = D::Capture(binding, owner, name, after)) != ClockResult::Ok)
            return result;
        if (!D::SameIdentity(middle, after) || middle.type != after.type || first.words != second.words)
            return ClockResult::OwnerConflict;
        if (!RevalidateGeometry(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        // These are14 raw binary32 storage words, not recomputed shape data.
        // In particular upperZ remains the classic SDK's raw Height value;
        // there is no subtraction, normalization, finite test or writable view.
        output = second;
        return ClockResult::Ok;
    }
}
