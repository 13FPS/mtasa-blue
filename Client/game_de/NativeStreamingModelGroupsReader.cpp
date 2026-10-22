/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current native tracker facts with exact image/GT/GC/playing bookends
 ****************************************************************************/
#include "NativeStreamingModelGroupsReader.h"
#include "NativeImageAccess.h"
#include "NativeStartGameCalls.h"
#include "NativeGameStateMemory.h"
#include <cstring>
namespace mta::game_de
{
    namespace ModelGroupsDetail
    {
        using Reader = NativeStreamingModelGroupsReader;
        namespace P = NativeStreamingModelGroupsProfile;
        namespace Image = NativeImageAccess;
        ClockResult Owner(Reader::Binding binding, Reader::CheckOwner owner) noexcept
        {
            if (!owner)
                return ClockResult::NotInitialized;
            auto result = owner();
            if (result != ClockResult::Ok)
                return result;
            if ((result = NativeStartGameCalls::CheckThread(binding.image)) != ClockResult::Ok)
                return result;
            std::uint32_t state = 0;
            if (!NativeGameStateMemory::Read(binding.image, state))
                return ClockResult::BindingChanged;
            if (state != 9)
                return ClockResult::NotInitialized;
            return owner();
        }
        bool Copy(std::uintptr_t image, Reader::Value& v) noexcept
        {
            return Image::ReadImage(image, image + P::PedCountRva, &v.pedCount, sizeof(v.pedCount), true) &&
                   Image::ReadImage(image, image + P::PedSlotsRva, v.peds.data(), sizeof(v.peds), true) &&
                   Image::ReadImage(image, image + P::VehicleSlotsRva, v.vehicles.data(), sizeof(v.vehicles), true);
        }
        bool Same(const Reader::Value& a, const Reader::Value& b) noexcept
        {
            return a.pedCount == b.pedCount && a.peds == b.peds && a.vehicles == b.vehicles;
        }
    }
    bool NativeStreamingModelGroupsReader::Qualify(Binding& output) noexcept
    {
        Binding value;
        if (!NativeModelBankReader::Qualify(value) || !Revalidate(value))
            return false;
        output = value;
        return true;
    }
    bool NativeStreamingModelGroupsReader::Revalidate(const Binding& binding) noexcept
    {
        namespace P = NativeStreamingModelGroupsProfile;
        namespace I = NativeImageAccess;
        // Reuse established supported PE/image admission, not a model-bank
        // read or fabricated object association for this scalar-only service.
        if (!NativeModelBankReader::Revalidate(binding))
            return false;
        IMAGE_DOS_HEADER   dos{};
        IMAGE_NT_HEADERS64 nt{};
        if (!I::ReadImage(binding.image, binding.image, &dos, sizeof(dos)) || dos.e_lfanew <= 0 ||
            static_cast<std::size_t>(dos.e_lfanew) > NativePedPoolProfile::ImageSize - sizeof(nt) ||
            !I::ReadImage(binding.image, binding.image + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt)) || !nt.FileHeader.NumberOfSections ||
            nt.FileHeader.NumberOfSections > 96)
            return false;
        const auto at = static_cast<std::uintptr_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
        const auto size = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        std::array<IMAGE_SECTION_HEADER, 96> sections{};
        if (at > NativePedPoolProfile::ImageSize || size > NativePedPoolProfile::ImageSize - at ||
            !I::ReadImage(binding.image, binding.image + at, sections.data(), size))
            return false;
        const auto contains = [&](std::uintptr_t rva, std::size_t bytes, DWORD required, DWORD forbidden)
        {
            for (std::size_t index = 0; index < nt.FileHeader.NumberOfSections; ++index)
                if (I::SectionContains(sections[index], rva, bytes, required, forbidden))
                    return true;
            return false;
        };
        if (!contains(P::PedCountRva, sizeof(Value::pedCount), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
            !contains(P::PedSlotsRva, sizeof(Value::peds), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE) ||
            !contains(P::VehicleSlotsRva, sizeof(Value::vehicles), IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE, IMAGE_SCN_MEM_EXECUTE))
            return false;
        std::array<unsigned char, P::MaximumPinBytes> bytes{};
        for (const auto& pin : P::Code)
            if (!pin.size || pin.size > bytes.size() || !contains(pin.rva, pin.size, IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_WRITE) ||
                !I::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) || std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        return I::ImagePages(binding.image, binding.image + P::PedCountRva, sizeof(Value::pedCount), true) &&
               I::ImagePages(binding.image, binding.image + P::PedSlotsRva, sizeof(Value::peds), true) &&
               I::ImagePages(binding.image, binding.image + P::VehicleSlotsRva, sizeof(Value::vehicles), true);
    }
    ClockResult NativeStreamingModelGroupsReader::Read(Value& output) const noexcept
    {
        namespace D = ModelGroupsDetail;
        auto result = D::Owner(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        if (!Revalidate(binding))
            return ClockResult::BindingChanged;
        Value before{}, after{};
        if (!D::Copy(binding.image, before))
            return ClockResult::ReadFailure;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        if (!D::Copy(binding.image, after))
            return ClockResult::ReadFailure;
        if (!D::Same(before, after) || !Revalidate(binding))
            return ClockResult::BindingChanged;
        if ((result = D::Owner(binding, owner)) != ClockResult::Ok)
            return result;
        output = after;
        return ClockResult::Ok;
    }
    ClockResult NativeStreamingModelGroupsReader::Recheck(const Value& previous) const noexcept
    {
        Value      current;
        const auto result = Read(current);
        return result != ClockResult::Ok ? result : ModelGroupsDetail::Same(previous, current) ? ClockResult::Ok : ClockResult::BindingChanged;
    }
}
