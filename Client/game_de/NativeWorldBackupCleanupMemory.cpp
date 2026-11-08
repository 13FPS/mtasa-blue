/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Close every native empty DeleteRW/reference forward branch before entry
 ****************************************************************************/
#include "NativeWorldBackupCleanupMemory.h"
#include "NativeImageAccess.h"
#include <cstring>
namespace mta::game_de::NativeWorldBackupCleanupMemory
{
    namespace CleanupP = NativeWorldBackupCleanupProfile;
    namespace CleanupB = NativeWorldBoundRectMemory;
    namespace CleanupI = NativeImageAccess;
    template <class T, class Bytes>
    T CleanupField(const Bytes& bytes, std::size_t offset) noexcept
    {
        T value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!CleanupB::Profile(image))
            return false;
        std::array<unsigned char, CleanupP::MaximumPinBytes> bytes{};
        for (const auto& pin : CleanupP::Pins)
            if (!pin.size || pin.size > bytes.size() || !CleanupI::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, true) ||
                std::memcmp(bytes.data(), pin.bytes, pin.size))
                return false;
        for (const auto rva : CleanupP::DeleteRwSlots)
        {
            std::uintptr_t target = 0;
            if (!NativeModelReferenceHeap::ReadOnlyImage(image, image + rva, sizeof(target)) ||
                !CleanupI::ReadImage(image, image + rva, &target, sizeof(target)) || target != image + CleanupP::DeleteRwRva)
                return false;
        }
        return true;
    }
    bool CleanupEmpty(const Value& value) noexcept
    {
        const auto& row = value.inputs.entity.bytes;
        if (CleanupField<std::uintptr_t>(row, 0x20) || CleanupField<std::uintptr_t>(row, CleanupP::EntityReferencesOffset) ||
            (CleanupField<std::uint32_t>(row, 0x34) & CleanupP::ProviderCleanupFlag) || value.inputs.modelBytes[CleanupP::ModelEffectCountOffset] ||
            (value.inputs.entity.kind == Kind::Dummy && CleanupField<std::uintptr_t>(row, 0x70)))
            return false;
        if (value.manager.size() != CleanupP::ManagerCapacity * CleanupP::ManagerStride)
            return false;
        for (std::size_t i = 0; i < CleanupP::ManagerCapacity; ++i)
        {
            const auto at = i * CleanupP::ManagerStride;
            if (value.manager[at + CleanupP::ManagerActiveOffset] &&
                CleanupField<std::uintptr_t>(value.manager, at + CleanupP::ManagerOwnerOffset) == value.inputs.entity.address)
                return false;
        }
        return true;
    }
    ClockResult CaptureEmpty(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner after, Value& output)
    {
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Value value;
        auto  result = CleanupB::Capture(image, generation, kind, index, owner, after, value.inputs);
        if (result != ClockResult::Ok)
            return result;
        value.manager.resize(CleanupP::ManagerCapacity * CleanupP::ManagerStride);
        if (!CleanupI::ReadImage(image, image + CleanupP::ManagerRva, value.manager.data(), value.manager.size(), true))
            return ClockResult::ReadFailure;
        if (!CleanupEmpty(value))
            return ClockResult::UnsupportedBuild;
        if ((result = owner()) != ClockResult::Ok || (result = after()) != ClockResult::Ok)
            return result;
        if (!SealEmpty(image, value))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool SealEmpty(std::uintptr_t image, const Value& value) noexcept
    {
        if (!Profile(image) || !CleanupEmpty(value) || !CleanupB::Seal(image, value.inputs))
            return false;
        try
        {
            std::vector<unsigned char> current(value.manager.size());
            return CleanupI::ReadImage(image, image + CleanupP::ManagerRva, current.data(), current.size(), true) && current == value.manager;
        }
        catch (...)
        {
            return false;
        }
    }
}
