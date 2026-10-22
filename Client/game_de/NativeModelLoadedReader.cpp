/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bookend native model streaming facts inside the existing owner
 ****************************************************************************/
#include "NativeModelLoadedReader.h"
#include "NativeModelLoadedProfile.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <array>
#include <cstring>

namespace mta::game_de
{
    namespace ModelLoadedDetail
    {
        namespace P = NativeModelLoadedProfile;
        namespace Image = NativeImageAccess;
        using Reader = NativeModelLoadedReader;
        ClockResult Owner(Reader::Binding binding, Reader::CheckOwner owner) noexcept
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
        bool Same(const Reader::Value& a, const Reader::Value& b) noexcept
        {
            return a.modelIndex == b.modelIndex && a.rowAddress == b.rowAddress && a.rawFlags == b.rawFlags && a.rawLoadState == b.rawLoadState &&
                   a.streamedIn == b.streamedIn;
        }
    }
    bool NativeModelLoadedReader::Qualify(Binding& output) noexcept
    {
        Binding next;
        if (!NativeModelBankReader::Qualify(next) || !Revalidate(next))
            return false;
        output = next;
        return true;
    }
    bool NativeModelLoadedReader::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativeModelLoadedProfile;
        namespace Image = NativeImageAccess;
        if (!NativeModelBankReader::Revalidate(binding))
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
        bool arrayContained = false;
        for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
            if (Image::SectionContains(sections[i], P::ArrayRva, P::ArrayBytes, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE))
                arrayContained = true;
        if (!arrayContained)
            return false;
        for (const auto& pin : P::Code)
        {
            bool contained = false;
            for (std::size_t i = 0; i != nt.FileHeader.NumberOfSections; ++i)
                if (Image::SectionContains(sections[i], pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE))
                    contained = true;
            std::array<unsigned char, P::MaximumPinBytes> actual{};
            if (!contained || !pin.size || pin.size > actual.size() ||
                !Image::ReadImage(binding.image, binding.image + pin.rva, actual.data(), pin.size, false, true) ||
                std::memcmp(actual.data(), pin.bytes, pin.size))
                return false;
        }
        return true;
    }
    ClockResult NativeModelLoadedReader::Read(std::uint32_t modelIndex, Value& output) const noexcept
    {
        namespace D = ModelLoadedDetail;
        namespace P = NativeModelLoadedProfile;
        // The full SDK key is bounded to the initializer's complete resource
        // array. The narrower DFF metadata bank remains a request-only limit;
        // WORD ffff and high-DWORD aliases never select outside this array.
        if (modelIndex >= P::ModelCount)
            return ClockResult::InvalidArgument;
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        Value first{modelIndex, binding.image + P::ArrayRva + static_cast<std::uintptr_t>(modelIndex) * P::RowStride, 0, 0, false}, second = first;
        std::array<unsigned char, 2> firstBytes{}, secondBytes{};
        static_assert(P::StateOffset == P::FlagsOffset + 1);
        if (!NativeImageAccess::ReadImage(binding.image, first.rowAddress + P::FlagsOffset, firstBytes.data(), firstBytes.size(), true))
            return ClockResult::ReadFailure;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        if (!NativeImageAccess::ReadImage(binding.image, second.rowAddress + P::FlagsOffset, secondBytes.data(), secondBytes.size(), true))
            return ClockResult::ReadFailure;
        first.rawFlags = firstBytes[0];
        first.rawLoadState = firstBytes[1];
        first.streamedIn = first.rawLoadState == P::LoadedState;
        second.rawFlags = secondBytes[0];
        second.rawLoadState = secondBytes[1];
        second.streamedIn = second.rawLoadState == P::LoadedState;
        if (!D::Same(first, second) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        output = second;
        return ClockResult::Ok;
    }
    ClockResult NativeModelLoadedReader::Recheck(const Value& previous) const noexcept
    {
        Value      next;
        const auto result = Read(previous.modelIndex, next);
        return result != ClockResult::Ok ? result : ModelLoadedDetail::Same(previous, next) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
