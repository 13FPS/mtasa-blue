/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Follow the real vehicle visual pool and journal every vector word
 *****************************************************************************/
#include "NativeModelDummiesMemory.h"
#include "NativeModelDummiesProfile.h"
#include "NativeModelLoadedProfile.h"
#include "NativeModelReferenceMap.h"
#include "NativeImageAccess.h"
#include <cstring>
#include <limits>

namespace mta::game_de::NativeModelDummiesMemory
{
    namespace Detail
    {
        namespace P = NativeModelDummiesProfile;
        namespace I = NativeImageAccess;
        template <class T>
        T Field(const Value& value, std::size_t offset) noexcept
        {
            T result{};
            std::memcpy(&result, value.header.data() + offset, sizeof(result));
            return result;
        }
        bool Profile(std::uintptr_t image) noexcept
        {
            std::array<unsigned char, P::MaximumPinBytes> bytes{};
            for (const auto& pin : P::Pins)
                if (!pin.size || pin.size > bytes.size() || !I::ReadImage(image, image + pin.rva, bytes.data(), pin.size, false, pin.code) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            std::array<std::uintptr_t, P::VtableMethods.size()> methods{};
            if (!I::ReadImage(image, image + NativeModelWheelProfile::VtableRva, methods.data(), sizeof(methods)))
                return false;
            for (std::size_t n = 0; n < methods.size(); ++n)
                if (methods[n] != image + P::VtableMethods[n])
                    return false;
            return true;
        }
        bool Loaded(std::uintptr_t image, const NativeModelLoadedReader::Value& value) noexcept
        {
            std::array<unsigned char, 2> current{};
            return value.rowAddress == image + NativeModelLoadedProfile::ArrayRva + value.modelIndex * NativeModelLoadedProfile::RowStride &&
                   I::ReadImage(image, value.rowAddress + NativeModelLoadedProfile::FlagsOffset, current.data(), current.size(), true) &&
                   current[0] == value.rawFlags && current[1] == value.rawLoadState;
        }
        ClockResult Visual(std::uintptr_t image, Value& value) noexcept
        {
            if (!Profile(image))
                return ClockResult::UnsupportedBuild;
            if (!I::ReadImage(image, value.model.model.nativeModelInfo + P::VisualOffset, &value.visual, sizeof(value.visual), true) ||
                !I::ReadImage(image, image + P::PoolRootRva, &value.pool, sizeof(value.pool), true))
                return ClockResult::ReadFailure;
            if (!value.pool || !NativeModelReferenceMap::Copy(value.pool, value.header.data(), value.header.size()))
                return ClockResult::ReadFailure;
            value.data = Field<std::uintptr_t>(value, 0);
            value.flags = Field<std::uintptr_t>(value, 8);
            const auto count = Field<std::int32_t>(value, 0x10), cursor = Field<std::int32_t>(value, 0x14);
            if (count != static_cast<std::int32_t>(P::PoolCapacity) || cursor < -1 || cursor >= count || value.header[0x18] != 1 || !value.data ||
                !value.flags || !NativeModelReferenceMap::PrivateWritable(value.data, P::PoolCapacity * P::PoolStride) ||
                !NativeModelReferenceMap::PrivateWritable(value.flags, P::PoolCapacity))
                return ClockResult::UnsupportedBuild;
            // A genuine current null visual pointer is a distinct observation.
            // Reset can qualify the classic null-skip without inventing zeros.
            if (!value.visual)
                return ClockResult::Ok;
            if (value.visual < value.data)
                return ClockResult::BindingChanged;
            const auto delta = value.visual - value.data;
            if (delta % P::PoolStride || delta / P::PoolStride >= P::PoolCapacity || value.visual % alignof(std::uint32_t))
                return ClockResult::BindingChanged;
            value.index = static_cast<std::uint32_t>(delta / P::PoolStride);
            if (!NativeModelReferenceMap::Copy(value.flags + value.index, &value.flag, sizeof(value.flag)))
                return ClockResult::ReadFailure;
            if (value.flag & 0x80)
                return ClockResult::BindingChanged;
            static_assert(sizeof(value.bits) == P::SdkVectorCount * P::VectorBytes);
            if (!NativeModelReferenceMap::Copy(value.visual, value.bits.data(), sizeof(value.bits)))
                return ClockResult::ReadFailure;
            value.present = true;
            return ClockResult::Ok;
        }
        bool Same(const Value& first, const Value& next) noexcept
        {
            return NativeModelWheelMemory::SameIdentity(first.model, next.model) && first.loaded.modelIndex == next.loaded.modelIndex &&
                   first.loaded.rowAddress == next.loaded.rowAddress && first.loaded.rawFlags == next.loaded.rawFlags &&
                   first.loaded.rawLoadState == next.loaded.rawLoadState && first.loaded.streamedIn == next.loaded.streamedIn && first.pool == next.pool &&
                   first.header == next.header && first.visual == next.visual && first.data == next.data && first.flags == next.flags &&
                   first.index == next.index && first.flag == next.flag && first.present == next.present && first.bits == next.bits;
        }
        ClockResult Held(std::uintptr_t image, const Value& value, NativeModelBackingReferenceController& controller,
                         const NativeModelBackingReferenceToken& token, CheckOwner owner) noexcept
        {
            if (token.ownerGeneration != value.model.generation || token.modelIndex != value.model.model.modelIndex)
                return ClockResult::BindingChanged;
            NativeModelBackingReferenceState state;
            auto                             result = controller.Read(token.ownerGeneration, token, owner, state);
            if (result != ClockResult::Ok)
                return result;
            if (state.phase != NativeModelBackingReferencePhase::Held || state.acquisition.origin.root != value.model.context.root ||
                !NativeUObjectMembershipAccess::MatchesWeak(value.model.context.rootMembership, state.acquisition.origin.rootIndex,
                                                            state.acquisition.origin.rootSerial))
                return ClockResult::BindingChanged;
            std::uint32_t count = 0;
            result = NativeModelReferenceMemory::ReadCount(image, token.modelIndex, owner, count, &state.acquisition.origin);
            return result != ClockResult::Ok ? result : count ? ClockResult::Ok : ClockResult::BindingChanged;
        }
        bool Exchange(std::uintptr_t address, std::uint32_t before, std::uint32_t desired, std::uint32_t& observed, std::uint32_t& exception) noexcept
        {
            __try
            {
                LONG from = 0, to = 0;
                std::memcpy(&from, &before, sizeof(from));
                std::memcpy(&to, &desired, sizeof(to));
                const LONG result = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), to, from);
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
    Origin Identity(const Value& value) noexcept
    {
        return {NativeModelWheelMemory::Identity(value.model), value.pool, value.data, value.flags, value.visual, value.index, value.flag};
    }
    bool SameIdentity(const Origin& first, const Value& next) noexcept
    {
        return NativeModelWheelMemory::SameIdentity(first.model, next.model) && first.pool == next.pool && first.data == next.data &&
               first.flags == next.flags && first.visual == next.visual && first.index == next.index && first.flag == next.flag;
    }
    ClockResult Capture(std::uintptr_t image, std::uint64_t generation, std::uint32_t model, CheckOwner owner, CheckOwner afterApplication,
                        Value& output) noexcept
    {
        Value first, next;
        auto  result = NativeModelWheelMemory::Capture(image, generation, model, owner, afterApplication, first.model);
        if (result != ClockResult::Ok)
            return result;
        if (!first.model.vehicle)
        {
            output = first;
            return ClockResult::Ok;
        }
        NativeModelLoadedReader loaded({image}, owner);
        if ((result = loaded.Read(model, first.loaded)) != ClockResult::Ok || (result = Detail::Visual(image, first)) != ClockResult::Ok)
            return result;
        // Finish owner/GC callbacks before the last raw visual/flag snapshots.
        // The retained model hold is additional protection, never permission to
        // reuse a previous row pointer or a seven-bit pool serial as a lease.
        if ((result = NativeModelWheelMemory::Capture(image, generation, model, owner, afterApplication, next.model)) != ClockResult::Ok)
            return result;
        next.loaded = first.loaded;
        if (!NativeModelWheelMemory::SameIdentity(first.model, next.model) || !Detail::Loaded(image, first.loaded) ||
            (result = Detail::Visual(image, next)) != ClockResult::Ok)
            return result != ClockResult::Ok ? result : ClockResult::BindingChanged;
        if (!Detail::Same(first, next) || !Detail::Loaded(image, next.loaded))
            return ClockResult::BindingChanged;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult Write(std::uintptr_t image, const Value& input, std::uint32_t dummy, const VectorBits& requested,
                      NativeModelBackingReferenceController& controller, const NativeModelBackingReferenceToken& inputToken, CheckOwner owner,
                      CheckOwner afterApplication, Receipt& output) noexcept
    {
        const auto supplied = input;
        const auto desired = requested;
        const auto token = inputToken;
        Receipt    receipt;
        receipt.origin = Identity(supplied);
        receipt.dummy = dummy;
        receipt.desired = desired;
        const auto finish = [&](ClockResult status) noexcept
        {
            receipt.result = status;
            output = receipt;
            return status;
        };
        if (dummy >= supplied.bits.size() || !supplied.model.vehicle || !supplied.present || !supplied.loaded.streamedIn)
            return finish(ClockResult::InvalidArgument);
        receipt.before = supplied.bits[dummy];
        auto expected = supplied.bits;
        for (unsigned component = 0; component < desired.size(); ++component)
        {
            auto  result = Detail::Held(image, supplied, controller, token, owner);
            Value current;
            if (result == ClockResult::Ok)
                result = Capture(image, supplied.model.generation, supplied.model.model.modelIndex, owner, afterApplication, current);
            if (result == ClockResult::Ok && (!SameIdentity(receipt.origin, current) || !current.present || current.bits != expected))
                result = ClockResult::BindingChanged;
            if (result != ClockResult::Ok)
            {
                receipt.uncertain = receipt.accepted != 0;
                return finish(result);
            }
            const auto mask = static_cast<unsigned char>(1u << component);
            receipt.entered |= mask;
            if (!Detail::Exchange(current.visual + dummy * NativeModelDummiesProfile::VectorBytes + component * sizeof(std::uint32_t),
                                  expected[dummy][component], desired[component], receipt.observed[component], receipt.exceptionCode))
            {
                receipt.uncertain = true;
                return finish(ClockResult::OriginalException);
            }
            if (receipt.observed[component] != expected[dummy][component])
            {
                receipt.uncertain = receipt.accepted != 0;
                return finish(ClockResult::OwnerConflict);
            }
            receipt.accepted |= mask;
            expected[dummy][component] = desired[component];
            // Twelve-byte vectors are not uniformly eight-byte aligned. Three
            // aligned DWORD CAS operations preserve raw float bits and expose
            // every accepted prefix instead of using an unaligned wide store.
            result = Detail::Held(image, supplied, controller, token, owner);
            if (result == ClockResult::Ok)
                result = Capture(image, supplied.model.generation, supplied.model.model.modelIndex, owner, afterApplication, current);
            if (result == ClockResult::Ok && (!SameIdentity(receipt.origin, current) || !current.present || current.bits != expected))
                result = ClockResult::BindingChanged;
            if (result != ClockResult::Ok)
            {
                receipt.uncertain = true;
                return finish(result);
            }
        }
        return finish(ClockResult::Ok);
    }
    NativeModelDummyVector Decode(const VectorBits& bits) noexcept
    {
        NativeModelDummyVector value;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, bits.data(), sizeof(value));
        return value;
    }
    VectorBits Encode(const NativeModelDummyVector& value) noexcept
    {
        VectorBits bits{};
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(bits.data(), &value, sizeof(value));
        return bits;
    }
}
