/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Read genuine current-bank hierarchy and native sequence borrows
 ****************************************************************************/
#include "NativeAnimationHierarchyMemory.h"
#include "NativeAnimationHierarchyOrigin.h"
#include "NativeAnimationHierarchyReadProfile.h"
#include "NativeModelReferenceMap.h"
#include <array>
#include <cstring>
#include <limits>
namespace mta::game_de
{
    namespace AnimationHierarchyReadDetail
    {
        namespace Origin = NativeAnimationHierarchyOrigin;
        namespace Profile = NativeAnimationHierarchyReadProfile;
        namespace Image = NativeImageAccess;
        using Binding = NativeAnimationHierarchyMemory::Binding;
        using CheckOwner = NativeAnimationHierarchyMemory::CheckOwner;
        struct Payload
        {
            std::uintptr_t sequences = 0;
            std::uint32_t  hash = 0, totalTimeBits = 0;
            std::int32_t   block = 0;
            std::uint16_t  count = 0;
            std::uint8_t   compressed = 0;
        };
        using Observation = Origin::Observation<Payload>;
        // Keep the real guarded private-page copy callable even when a test
        // injects a failure at the cookie read. No child payload is copied.
        bool Copy(std::uintptr_t source, void* target, std::size_t bytes) noexcept
        {
            return NativeModelReferenceMap::Copy(source, target, bytes);
        }
#ifndef MTA_DE_ANIMATION_HIERARCHY_COPY
    #define MTA_DE_ANIMATION_HIERARCHY_COPY ::mta::game_de::AnimationHierarchyReadDetail::Copy
#endif
        bool ReadProfile(Binding binding, std::uintptr_t rowRva) noexcept
        {
            if (!Origin::Sections(binding, rowRva, 0, Profile::Code.data(), Profile::Code.size()))
                return false;
            for (const auto& pin : Profile::Code)
            {
                std::array<unsigned char, Profile::MaximumPinBytes> bytes{};
                if (!pin.size || pin.size > bytes.size() || !Image::ReadImage(binding.image, binding.image + pin.rva, bytes.data(), pin.size, false, true) ||
                    std::memcmp(bytes.data(), pin.bytes, pin.size))
                    return false;
            }
            return true;
        }
        template <class T>
        T Field(const std::array<unsigned char, Profile::PrefixBytes>& bytes, std::size_t offset) noexcept
        {
            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
        ClockResult Observe(Binding binding, std::uintptr_t row, Payload& output) noexcept
        {
            if (!ReadProfile(binding, row - binding.image))
                return ClockResult::BindingChanged;
            std::array<unsigned char, Profile::PrefixBytes> bytes{};
            if (!Image::ReadImage(binding.image, row, bytes.data(), bytes.size(), true))
                return ClockResult::ReadFailure;
            Payload next;
            next.hash = Field<std::uint32_t>(bytes, 0);
            next.sequences = Field<std::uintptr_t>(bytes, 8);
            next.count = Field<std::uint16_t>(bytes, 0x10);
            next.compressed = Field<std::uint8_t>(bytes, 0x12);
            next.block = Field<std::int32_t>(bytes, 0x14);
            // Preserve the actual float representation. Equality and output
            // never compare/normalize floating values, including NaN payloads.
            next.totalTimeBits = Field<std::uint32_t>(bytes, 0x18);
            output = next;
            return ClockResult::Ok;
        }
        ClockResult Capture(Binding binding, CheckOwner owner, const Origin::Location& location, Observation& output) noexcept
        {
            Observation next;
            auto        result = Origin::Capture(binding, owner, location.ordinal, location.rva, &Observe, next);
            if (result != ClockResult::Ok)
                return result;
            if (static_cast<std::uint32_t>(next.payload.block) != next.origin.block || !ReadProfile(binding, location.rva))
                return ClockResult::BindingChanged;
            if ((result = Origin::Owner(binding, owner)) != ClockResult::Ok)
                return result;
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Observation& first, const Observation& second) noexcept
        {
            const auto& a = first.payload;
            const auto& b = second.payload;
            return Origin::Same(first.origin, second.origin) && a.sequences == b.sequences && a.hash == b.hash && a.totalTimeBits == b.totalTimeBits &&
                   a.block == b.block && a.count == b.count && a.compressed == b.compressed;
        }
        struct Span
        {
            std::uintptr_t first = 0, base = 0;
            std::size_t    bytes = 0;
            std::uint64_t  capacity = 0;
        };
        ClockResult SequenceSpan(std::uintptr_t first, Span& output) noexcept
        {
            if (!first || first < Profile::CookieBytes || first % alignof(std::uint64_t))
                return ClockResult::BindingChanged;
            Span next;
            next.first = first;
            next.base = first - Profile::CookieBytes;
            if (!MTA_DE_ANIMATION_HIERARCHY_COPY(next.base, &next.capacity, sizeof(next.capacity)))
                return ClockResult::ReadFailure;
            constexpr auto limit = (std::numeric_limits<std::size_t>::max)();
            if (next.capacity > (limit - Profile::CookieBytes) / Profile::SequenceStride)
                return ClockResult::BindingChanged;
            next.bytes = Profile::CookieBytes + static_cast<std::size_t>(next.capacity) * Profile::SequenceStride;
            if (next.bytes > (std::numeric_limits<std::uintptr_t>::max)() - next.base || !NativeModelReferenceMap::PrivateWritable(next.base, next.bytes))
                return ClockResult::BindingChanged;
            // The full native cookie is allocation capacity. The independent
            // logical WORD count is deliberately neither compared nor clamped.
            output = next;
            return ClockResult::Ok;
        }
        bool Same(const Span& first, const Span& second) noexcept
        {
            return first.first == second.first && first.base == second.base && first.bytes == second.bytes && first.capacity == second.capacity;
        }
        ClockResult ReadPointer(Binding binding, CheckOwner owner, std::uintptr_t requested, bool selected, std::uint32_t index,
                                std::uintptr_t& output) noexcept
        {
            Origin::Location location;
            auto             result = Origin::Locate(binding, requested, location);
            if (result != ClockResult::Ok)
                return result;
            Observation first, second, final;
            if ((result = Capture(binding, owner, location, first)) != ClockResult::Ok)
                return result;
            if (!first.payload.sequences)
            {
                if ((result = Capture(binding, owner, location, second)) != ClockResult::Ok)
                    return result;
                if (!Same(first, second))
                    return ClockResult::BindingChanged;
                // Source zero arithmetic remains null. A nonzero index has
                // no actual child row to borrow from a null native pointer.
                if (selected && index)
                    return ClockResult::InvalidArgument;
                output = 0;
                return ClockResult::Ok;
            }
            Span firstSpan, secondSpan;
            if ((result = SequenceSpan(first.payload.sequences, firstSpan)) != ClockResult::Ok ||
                (result = Capture(binding, owner, location, second)) != ClockResult::Ok)
                return result;
            if (!Same(first, second))
                return ClockResult::BindingChanged;
            if ((result = SequenceSpan(second.payload.sequences, secondSpan)) != ClockResult::Ok ||
                (result = Capture(binding, owner, location, final)) != ClockResult::Ok)
                return result;
            if (!Same(first, final) || !Same(firstSpan, secondSpan))
                return ClockResult::BindingChanged;
            std::uintptr_t next = secondSpan.first;
            if (selected)
            {
                if (static_cast<std::uint64_t>(index) >= secondSpan.capacity)
                    return ClockResult::InvalidArgument;
                // The native full cookie already bounds the row80 product;
                // preserve the SDK DWORD index without a WORD conversion.
                const auto displacement = static_cast<std::size_t>(index) * Profile::SequenceStride;
                if (displacement > (std::numeric_limits<std::uintptr_t>::max)() - next)
                    return ClockResult::InvalidArgument;
                next += displacement;
                if (!NativeModelReferenceMap::PrivateWritable(next, Profile::SequenceStride))
                    return ClockResult::BindingChanged;
            }
            output = next;
            return ClockResult::Ok;
        }
    }
    ClockResult NativeAnimationHierarchyMemory::Read(std::uintptr_t locator, NativeAnimationHierarchyValues& output) const noexcept
    {
        namespace Detail = AnimationHierarchyReadDetail;
        namespace Origin = NativeAnimationHierarchyOrigin;
        const auto       requested = locator;
        Origin::Location location;
        auto             result = Origin::Locate(binding, requested, location);
        if (result != ClockResult::Ok)
            return result;
        Detail::Observation first, second;
        if ((result = Detail::Capture(binding, owner, location, first)) != ClockResult::Ok ||
            (result = Detail::Capture(binding, owner, location, second)) != ClockResult::Ok)
            return result;
        if (!Detail::Same(first, second))
            return ClockResult::BindingChanged;
        NativeAnimationHierarchyValues next;
        next.interfaceAddress = binding.image + location.rva;
        next.hash = second.payload.hash;
        next.totalTimeBits = second.payload.totalTimeBits;
        next.sequenceCount = second.payload.count;
        next.runningCompressed = second.payload.compressed;
        next.blockId = second.payload.block;
        output = next;
        return ClockResult::Ok;
    }
    ClockResult NativeAnimationHierarchyMemory::ReadSequences(std::uintptr_t locator, std::uintptr_t& output) const noexcept
    {
        const auto requested = locator;
        return AnimationHierarchyReadDetail::ReadPointer(binding, owner, requested, false, 0, output);
    }
    ClockResult NativeAnimationHierarchyMemory::ReadSequence(std::uintptr_t locator, std::uint32_t index, std::uintptr_t& output) const noexcept
    {
        const auto requested = locator;
        const auto ordinal = index;
        return AnimationHierarchyReadDetail::ReadPointer(binding, owner, requested, true, ordinal, output);
    }
}
