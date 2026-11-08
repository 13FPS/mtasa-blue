/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Share genuine current BoundRect receiver/input lifetime observations
 ****************************************************************************/
#include "NativeWorldBoundRectMemory.h"
#include "NativeWorldBoundRectProfile.h"
#include "NativeImageAccess.h"
#include "NativeModelTypeLeaf.h"
#include "NativeGameStateMemory.h"
#include "NativeStartGameCalls.h"
#include <cmath>
#include <cstring>
#include <utility>

namespace mta::game_de::NativeWorldBoundRectMemory
{
    namespace BoundP = NativeWorldBoundRectProfile;
    namespace BoundG = NativeModelObjectGroupMemory;
    namespace BoundGP = NativeModelObjectGroupProfile;
    namespace BoundM = NativeWorldPoolMatrixMemory;
    namespace BoundC = NativeStreamingCompletionContext;
    namespace BoundH = NativeModelReferenceHeap;
    namespace
    {
        template <class T, std::size_t N>
        T Field(const std::array<unsigned char, N>& bytes, std::size_t offset) noexcept
        {
            T value{};
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        template <class Pins>
        bool Match(std::uintptr_t image, const Pins& pins) noexcept
        {
            std::array<unsigned char, BoundP::MaximumPinBytes> bytes{};
            for (const auto& pin : pins)
                if (!pin.size || pin.size > bytes.size() || !NativeImageAccess::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            return true;
        }
        bool ModelSeal(std::uintptr_t image, const Value& value) noexcept
        {
            const auto&                                    model = value.model;
            std::uintptr_t                                 current = 0;
            std::int32_t                                   count = 0;
            std::array<unsigned char, BoundGP::PoolStride> bytes{};
            std::array<unsigned char, 6>                   typeBytes{};
            std::uint8_t                                   type = 0;
            return NativeModelBankReader::Revalidate({image}) &&
                   NativeImageAccess::ReadImage(image, model.model.slotAddress, &current, sizeof(current), true) && current == model.model.nativeModelInfo &&
                   NativeStreamingModelContext::Recheck(image, model.model, model.context) == ClockResult::Ok &&
                   NativeImageAccess::ReadImage(image, image + BoundGP::PoolCountRva, &count, sizeof(count), true) && count == model.poolCount &&
                   NativeImageAccess::ReadImage(image, current, bytes.data(), bytes.size(), true) && bytes == value.modelBytes &&
                   NativeModelTypeLeaf::Read(image, model.context.typeMethod, typeBytes, type) && type == 1 && typeBytes == model.typeBytes;
        }
    }
    bool Profile(std::uintptr_t image) noexcept
    {
        if (!BoundM::Profile(image) || !Match(image, BoundP::Pins) || !Match(image, BoundGP::Pins))
            return false;
        for (const auto& pin : BoundP::Pins)
            if (!pin.code && pin.rva != BoundP::DefaultBoxRva && !BoundH::ReadOnlyImage(image, image + pin.rva, pin.size))
                return false;
        for (const auto& slot : NativeWorldAddProfile::Slots)
        {
            std::uintptr_t target = 0;
            if (!BoundH::ReadOnlyImage(image, image + slot.rva, sizeof(target)) ||
                !NativeImageAccess::ReadImage(image, image + slot.rva, &target, sizeof(target)) || target != image + slot.target)
                return false;
        }
        return true;
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, Kind kind, std::uint32_t index, CheckOwner owner, CheckOwner afterApplication,
                        Value& output)
    {
        if (!generation || !owner || !afterApplication)
            return ClockResult::InvalidArgument;
        auto result = owner();
        if (result != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        if (!Profile(image))
            return ClockResult::UnsupportedBuild;
        Value value;
        if ((result = BoundC::Capture(image, value.world)) != ClockResult::Ok || (result = BoundH::Capture(image, value.heap)) != ClockResult::Ok ||
            (result = BoundM::CaptureEntity(image, kind, index, value.entity)) != ClockResult::Ok)
            return result;
        if (Field<std::uintptr_t>(value.entity.bytes, 0x20))
            return ClockResult::UnsupportedBuild;
        const auto model = Field<std::int16_t>(value.entity.bytes, 0x3a);
        if (model < 0)
            return ClockResult::UnsupportedBuild;
        if ((result = BoundG::Capture(image, generation, static_cast<std::uint32_t>(model), owner, afterApplication, value.model)) != ClockResult::Ok)
            return result;
        if (!value.model.present ||
            !NativeImageAccess::ReadImage(image, value.model.model.nativeModelInfo, value.modelBytes.data(), value.modelBytes.size(), true))
            return ClockResult::ReadFailure;
        // The genuine Atomic initializer clears collision30; this is an exact
        // supported null association, not a readable arbitrary heap CColModel.
        // Native then borrows its static image-lifetime default box unchanged.
        if (Field<std::uintptr_t>(value.modelBytes, BoundP::CollisionOffset))
            return ClockResult::UnsupportedBuild;
        if (!NativeImageAccess::ReadImage(image, image + BoundP::DefaultBoxRva, value.defaultBox.data(), value.defaultBox.size()) ||
            value.defaultBox != BoundP::DefaultBox)
            return ClockResult::BindingChanged;
        if (value.entity.matrix)
        {
            if ((result = BoundM::Capture(image, value.matrices)) != ClockResult::Ok ||
                (result = BoundM::FindOwner(value.matrices, value.entity, value.matrix)) != ClockResult::Ok)
                return result;
            value.hasMatrix = true;
        }
        else
        {
            if (Field<std::uint32_t>(value.entity.bytes, 0x14) != BoundP::InlineHeadingBits)
                return ClockResult::UnsupportedBuild;
            for (std::size_t offset = 8; offset != 0x14; offset += 4)
                if (!std::isfinite(Field<float>(value.entity.bytes, offset)))
                    return ClockResult::UnsupportedBuild;
        }
        if ((result = owner()) != ClockResult::Ok || (result = afterApplication()) != ClockResult::Ok)
            return result;
        if (!Seal(image, value))
            return ClockResult::BindingChanged;
        output = std::move(value);
        return ClockResult::Ok;
    }
    bool Seal(std::uintptr_t image, const Value& value) noexcept
    {
        BoundC::Value              world;
        decltype(value.defaultBox) box{};
        std::uint32_t              state = 0;
        if (!Profile(image) || BoundC::Capture(image, world) != ClockResult::Ok || !BoundC::Same(world, value.world, true) ||
            BoundH::Recheck(image, value.heap) != ClockResult::Ok || !BoundM::SealEntity(image, value.entity) || !ModelSeal(image, value) ||
            !NativeImageAccess::ReadImage(image, image + BoundP::DefaultBoxRva, box.data(), box.size()) || box != value.defaultBox ||
            !NativeGameStateMemory::Read(image, state) || state != 9 || NativeStartGameCalls::CheckThread(image) != ClockResult::Ok)
            return false;
        return !value.hasMatrix || BoundM::Seal(image, value.matrices);
    }
}
