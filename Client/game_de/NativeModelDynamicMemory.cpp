/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read only the genuine object-property WORD without a mutation lease.
 *****************************************************************************/
#include "NativeModelDynamicMemory.h"
#include "NativeModelObjectGroupProfile.h"
#include "NativeModelTimeProfile.h"
#include "NativeModelWheelProfile.h"
#include "NativeStreamingModelContext.h"
#include "NativeModelTypeLeaf.h"
#include "NativeImageAccess.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <array>
#include <cstring>
#include <iterator>
#include <limits>
#include <windows.h>

namespace mta::game_de::NativeModelDynamicMemory
{
    namespace DynamicMemoryDetail
    {
        namespace A = NativeModelObjectGroupProfile;
        namespace T = NativeModelTimeProfile;
        namespace V = NativeModelWheelProfile;
        namespace I = NativeImageAccess;
        static_assert(A::FieldOffset == 0x28 && A::FieldBytes == sizeof(std::uint16_t) && A::StaticGroup == 0xffff);
        static_assert(A::MaximumPinBytes >= T::MaximumPinBytes && A::MaximumPinBytes >= V::MaximumPinBytes);
        enum class Family : unsigned char
        {
            Absent,
            Atomic,
            Time,
            Vehicle
        };
        struct Observation
        {
            NativeModelBankReader::Value       model;
            NativeStreamingModelContext::Value context;
            std::array<unsigned char, 6>       typeBytes{};
            std::int32_t                       poolCount = 0;
            std::uint16_t                      word = A::StaticGroup;
            Family                             family = Family::Absent;
        };
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
            // The saved Time publisher calls slot40 before bank publication.
            // Its older getter profile pins only slots00..38, so independently
            // require the saved relocated slot40 -> shared Atomic initializer.
            // That initializer and Vehicle's own initializer both write
            // DWORD+26=ffffffff, establishing this WORD's static sentinel.
            std::uintptr_t timeInitialize = 0;
            return pool(A::PoolRva, A::PoolBytes, A::PoolCountRva) && pool(T::PoolRva, T::PoolCapacity * T::RowBytes, T::PoolCountRva) &&
                   pool(V::PoolRva, V::PoolBytes, V::PoolCountRva) && Pins(image, A::Pins, contains) && Pins(image, T::Pins, contains) &&
                   Pins(image, V::Pins, contains) && Vtable(image, A::VtableRva, A::VtableMethods, contains) &&
                   Vtable(image, T::VtableRva, T::VtableMethods, contains) && Vtable(image, V::VtableRva, V::VtableMethods, contains) &&
                   contains(T::VtableRva + 0x40, sizeof(timeInitialize), IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE) &&
                   I::ReadImage(image, image + T::VtableRva + 0x40, &timeInitialize, sizeof(timeInitialize)) && timeInitialize == image + A::InitializeRva &&
                   NativeStreamingModelContext::QualifyProfile(image);
        }
        Family Select(std::uintptr_t image, const Observation& value, std::uint8_t type) noexcept
        {
            if (type == 1 && value.context.modelVtable == image + A::VtableRva && value.context.typeMethod == image + A::TypeGetterRva &&
                value.context.animationMethod == image + A::VtableMethods[15])
                return Family::Atomic;
            if (type == 3 && value.context.modelVtable == image + T::VtableRva && value.context.typeMethod == image + T::TypeMethodRva)
                return Family::Time;
            if (type == 6 && value.context.modelVtable == image + V::VtableRva && value.context.typeMethod == image + V::TypeGetterRva &&
                value.context.animationMethod == image + V::VtableMethods[15])
                return Family::Vehicle;
            return Family::Absent;
        }
        bool Field(std::uintptr_t image, const Observation& value, std::int32_t& count, std::uint16_t& word) noexcept
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
                (metadata - base) / stride >= static_cast<std::size_t>(count) || (metadata + A::FieldOffset) % alignof(std::uint16_t))
                return false;
            return I::ReadImage(image, metadata + A::FieldOffset, &word, sizeof(word), true);
        }
        bool Anchors(std::uintptr_t image, const Observation& value) noexcept
        {
            // All owner callbacks finish before this immediate raw fence. No
            // old observation escapes as an admission token or a saved lease.
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
                std::int32_t                 count = 0;
                std::uint16_t                word = 0;
                if (!NativeModelTypeLeaf::Read(image, value.context.typeMethod, bytes, type) || bytes != value.typeBytes ||
                    Select(image, value, type) != value.family || !Field(image, value, count, word) || count != value.poolCount || word != value.word ||
                    !NativeStreamingModelContext::MembershipUnchanged(image, value.context))
                    return false;
            }
            else if (value.family != Family::Absent)
                return false;
            std::uint32_t state = 0;
            return NativeGameStateMemory::Read(image, state) && state == 9 && NativeStartGameCalls::CheckThread(image) == ClockResult::Ok;
        }
    }
    ClockResult Read(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner afterApplication, bool& output) noexcept
    {
        if (!image || !generation || !owner || !afterApplication)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        DynamicMemoryDetail::Observation next;
        NativeModelBankReader            bank({image}, owner);
        if ((result = bank.Read(model, next.model)) != ClockResult::Ok)
            return result;
        if (!DynamicMemoryDetail::Profile(image))
            return ClockResult::UnsupportedBuild;
        if (next.model.nativeModelInfo)
        {
            if ((result = NativeStreamingModelContext::Capture(image, next.model, next.context)) != ClockResult::Ok)
                return result;
            std::uint8_t type = 0;
            if (!NativeModelTypeLeaf::Read(image, next.context.typeMethod, next.typeBytes, type) ||
                (next.family = DynamicMemoryDetail::Select(image, next, type)) == DynamicMemoryDetail::Family::Absent)
                return ClockResult::UnsupportedBuild;
            if (!DynamicMemoryDetail::Field(image, next, next.poolCount, next.word))
                return ClockResult::BindingChanged;
        }
        if ((result = bank.Recheck(next.model)) != ClockResult::Ok || (result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        if (!DynamicMemoryDetail::Anchors(image, next))
            return ClockResult::BindingChanged;
        // The SDK is a raw unsigned WORD != ffff test. High-bit and arbitrary
        // non-sentinel values remain true; no signed table index or clamp runs.
        output = next.model.nativeModelInfo && next.word != NativeModelObjectGroupProfile::StaticGroup;
        return ClockResult::Ok;
    }
}
