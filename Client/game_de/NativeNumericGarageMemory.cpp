/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Current-world unique numeric descriptors using the existing garage byte protocol
 ****************************************************************************/
#include "NativeNumericGarageMemory.h"
#include "NativeNumericGarageProfile.h"
#include "NativeGarageMemoryShared.h"
#include "NativeStreamingCompletionContext.h"
#include <game/CGarageDoorStatePolicy.h>
#include <array>
#include <limits>

namespace mta::game_de
{
    namespace NumericGarageDetail
    {
        namespace D = NamedGarageDetail;
        namespace P = NativeNumericGarageProfile;
        namespace C = NativeStreamingCompletionContext;
        using Memory = NativeNumericGarageMemory;
        struct Value
        {
            D::Value             row;
            NativeGarageGeometry geometry{};
        };
        using Values = std::array<Value, P::Descriptors.size()>;

        bool Same(const Value& left, const Value& right, bool properties, bool door = false) noexcept
        {
            return D::SameIdentity(left.row, right.row) && left.geometry.words == right.geometry.words &&
                   (!properties || (left.row.type == right.row.type && (!door || left.row.state == right.row.state)));
        }
        ClockResult Begin(Memory::Binding binding, Memory::CheckOwner owner) noexcept
        {
            auto result = D::Owner(binding, owner);
            return result == ClockResult::Ok ? C::Owner(binding.image, owner) : result;
        }
        // One current published scan finds every requested descriptor. Name
        // equality alone never selects a numeric ID: both sprLAe rows must
        // retain their independent fourteen-word geometry identities.
        ClockResult Capture(Memory::Binding binding, Memory::CheckOwner owner, const NativeNumericGarageDescriptor* requested, bool properties,
                            const C::Value* previous, C::Value& world, Values& output) noexcept
        {
            auto result = Begin(binding, owner);
            if (result != ClockResult::Ok)
                return result;
            if (!D::Memory::RevalidateGeometry(binding) || !C::Profile(binding.image))
                return ClockResult::BindingChanged;
            C::Value currentWorld;
            if ((result = C::Capture(binding.image, currentWorld)) != ClockResult::Ok)
                return result;
            if (previous && (!C::Fence(binding.image, *previous) || !C::Same(*previous, currentWorld, true)))
                return ClockResult::OwnerConflict;
            std::uint32_t count = 0;
            if (!D::I::ReadImage(binding.image, binding.image + D::P::CountRva, &count, sizeof(count), true))
                return ClockResult::ReadFailure;
            if (count != P::CertifiedPublishedCount || count > D::P::MaximumLookupCount)
                return ClockResult::BindingChanged;
            Values                                  next{};
            std::array<bool, P::Descriptors.size()> found{};
            for (std::uint32_t index = 0; index != count; ++index)
            {
                const auto rva = D::P::BankRva + static_cast<std::uintptr_t>(index) * D::P::RowBytes;
                if (rva > NativePedPoolProfile::ImageSize || D::P::DoorStateOffset + 1 > NativePedPoolProfile::ImageSize - rva ||
                    binding.image > (std::numeric_limits<std::uintptr_t>::max)() - rva - D::P::DoorStateOffset)
                    return ClockResult::ReadFailure;
                const auto       address = binding.image + rva;
                NativeGarageName name{};
                if (!D::I::ReadImage(binding.image, address + D::P::NameOffset, name.data(), name.size(), true))
                    return ClockResult::ReadFailure;
                if (!D::ValidName(name))
                    return ClockResult::BindingChanged;
                bool wanted = false;
                for (const auto& descriptor : P::Descriptors)
                    wanted = wanted || ((!requested || requested == &descriptor) && name == descriptor.name);
                if (!wanted)
                    continue;
                NativeGarageGeometry geometry;
                if (!D::I::ReadImage(binding.image, address, geometry.words.data(), sizeof(geometry.words), true))
                    return ClockResult::ReadFailure;
                for (std::size_t slot = 0; slot != P::Descriptors.size(); ++slot)
                {
                    const auto& descriptor = P::Descriptors[slot];
                    if ((requested && requested != &descriptor) || name != descriptor.name || geometry.words != descriptor.geometry.words)
                        continue;
                    if (found[slot])
                        return ClockResult::OwnerConflict;
                    found[slot] = true;
                    auto& selected = next[slot];
                    selected.row.count = count;
                    selected.row.index = index;
                    selected.row.row = address;
                    selected.row.name = name;
                    selected.geometry = geometry;
                    if (properties)
                    {
                        std::array<std::uint8_t, 2> bytes{};
                        if (!D::I::ReadImage(binding.image, address + D::P::TypeOffset, bytes.data(), bytes.size(), true))
                            return ClockResult::ReadFailure;
                        selected.row.type = bytes[0];
                        selected.row.state = bytes[1];
                    }
                }
            }
            for (std::size_t slot = 0; slot != found.size(); ++slot)
            {
                if ((!requested || requested == &P::Descriptors[slot]) && !found[slot])
                    return ClockResult::OwnerConflict;
                if (found[slot])
                    for (std::size_t earlier = 0; earlier != slot; ++earlier)
                        if (found[earlier] && next[earlier].row.index == next[slot].row.index)
                            return ClockResult::OwnerConflict;
            }
            std::uint32_t after = 0;
            if (!D::I::ReadImage(binding.image, binding.image + D::P::CountRva, &after, sizeof(after), true))
                return ClockResult::ReadFailure;
            if (after != count || !C::Fence(binding.image, currentWorld) || !D::Memory::RevalidateGeometry(binding))
                return ClockResult::BindingChanged;
            if ((result = Begin(binding, owner)) != ClockResult::Ok)
                return result;
            world = currentWorld;
            output = next;
            return ClockResult::Ok;
        }
    }
    ClockResult NativeNumericGarageMemory::CheckReady() const noexcept
    {
        namespace D = NumericGarageDetail;
        auto result = D::Begin(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        // Reuse the existing transient completion/GC protocol and named
        // writer exclusion. No byte CAS, journal or persistent lease is used
        // to test whether all required startup identities currently exist.
        if (NamedGarageDetail::writing || !D::C::TryEnter())
            return ClockResult::Busy;
        D::C::Scope scope;
        D::C::Value firstWorld, nextWorld;
        D::Values   first, next;
        if ((result = D::Capture(binding, owner, nullptr, false, nullptr, firstWorld, first)) != ClockResult::Ok ||
            (result = D::Capture(binding, owner, nullptr, false, &firstWorld, nextWorld, next)) != ClockResult::Ok)
            return result;
        for (std::size_t slot = 0; slot != first.size(); ++slot)
            if (!D::Same(first[slot], next[slot], false))
                return ClockResult::OwnerConflict;
        return D::C::Fence(binding.image, nextWorld) ? D::Begin(binding, owner) : ClockResult::BindingChanged;
    }
    ClockResult NativeNumericGarageMemory::Read(std::uint32_t id, NativeGarageGeometry* geometry, std::uint8_t* byte, bool door) const noexcept
    {
        namespace D = NumericGarageDetail;
        const auto* descriptor = D::P::Find(id);
        if (!descriptor)
            return ClockResult::InvalidArgument;
        auto result = D::Begin(binding, owner);
        if (result != ClockResult::Ok)
            return result;
        if (NamedGarageDetail::writing || !D::C::TryEnter())
            return ClockResult::Busy;
        D::C::Scope scope;
        D::C::Value firstWorld, nextWorld;
        D::Values   first, next;
        const bool  properties = true;
        if ((result = D::Capture(binding, owner, descriptor, properties, nullptr, firstWorld, first)) != ClockResult::Ok ||
            (result = D::Capture(binding, owner, descriptor, properties, &firstWorld, nextWorld, next)) != ClockResult::Ok)
            return result;
        const auto slot = static_cast<std::size_t>(descriptor - D::P::Descriptors.data());
        if (!D::Same(first[slot], next[slot], properties, door))
            return ClockResult::OwnerConflict;
        if (!D::C::Fence(binding.image, nextWorld))
            return ClockResult::BindingChanged;
        if ((result = D::Begin(binding, owner)) != ClockResult::Ok)
            return result;
        // Output publication follows the complete same-world observation;
        // failures never partially overwrite an SDK position/size tuple.
        if (geometry)
            *geometry = next[slot].geometry;
        if (byte)
            *byte = door ? next[slot].row.state : next[slot].row.type;
        return ClockResult::Ok;
    }
    ClockResult NativeNumericGarageMemory::ReadGeometry(std::uint32_t id, NativeGarageGeometry& output) const noexcept
    {
        return Read(id, &output, nullptr, false);
    }
    ClockResult NativeNumericGarageMemory::ReadType(std::uint32_t id, std::uint8_t& output) const noexcept
    {
        return Read(id, nullptr, &output, false);
    }
    ClockResult NativeNumericGarageMemory::ReadOpen(std::uint32_t id, bool& output) const noexcept
    {
        std::uint8_t state = 0;
        const auto   result = Read(id, nullptr, &state, true);
        if (result == ClockResult::Ok)
            output = MtaGame::CGarageDoorStatePolicy::IsOpen(state);
        return result;
    }
    ClockResult NativeNumericGarageMemory::SetType(std::uint32_t id, std::uint8_t target, Receipt* receipt) const noexcept
    {
        return Set(id, false, target, receipt);
    }
    ClockResult NativeNumericGarageMemory::SetOpen(std::uint32_t id, bool open, Receipt* receipt) const noexcept
    {
        return Set(id, true, MtaGame::CGarageDoorStatePolicy::StateForOpen(open), receipt);
    }
    ClockResult NativeNumericGarageMemory::Set(std::uint32_t id, bool door, std::uint8_t target, Receipt* output) const noexcept
    {
        namespace D = NumericGarageDetail;
        Receipt receipt;
        receipt.target = target;
        const auto finish = [&](ClockResult result)
        {
            if (output)
                *output = receipt;
            return result;
        };
        const auto* descriptor = D::P::Find(id);
        if (!descriptor)
            return finish(ClockResult::InvalidArgument);
        auto result = D::Begin(binding, owner);
        if (result != ClockResult::Ok)
            return finish(result);
        if (NamedGarageDetail::writing || !D::C::TryEnter())
            return finish(ClockResult::Busy);
        D::C::Scope              completion;
        NamedGarageDetail::Scope writer;
        D::C::Value              firstWorld, currentWorld, entryWorld, afterWorld;
        D::Values                first, current, entry, after;
        const auto               slot = static_cast<std::size_t>(descriptor - D::P::Descriptors.data());
        if ((result = D::Capture(binding, owner, descriptor, true, nullptr, firstWorld, first)) != ClockResult::Ok ||
            (result = D::Capture(binding, owner, descriptor, true, &firstWorld, currentWorld, current)) != ClockResult::Ok)
            return finish(result);
        if (!D::Same(first[slot], current[slot], true, door))
            return finish(ClockResult::OwnerConflict);
        const auto& selected = current[slot].row;
        receipt.count = selected.count;
        receipt.index = selected.index;
        receipt.row = selected.row;
        receipt.address = selected.row + (door ? NamedGarageDetail::P::DoorStateOffset : NamedGarageDetail::P::TypeOffset);
        receipt.original = door ? selected.state : selected.type;
        if (!NativeImageAccess::ImagePages(binding.image, receipt.address, sizeof(std::uint8_t), true))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Capture(binding, owner, descriptor, true, &currentWorld, entryWorld, entry)) != ClockResult::Ok)
            return finish(result);
        if (!D::Same(current[slot], entry[slot], true, door))
            return finish(ClockResult::OwnerConflict);
#if defined(MTA_DE_NAMED_GARAGE_EXCHANGE)
        const auto exchange = &MTA_DE_NAMED_GARAGE_EXCHANGE;
#else
        const auto exchange = &NamedGarageDetail::Exchange;
#endif
        // Reuse the original named route's one exact-byte CAS and receipt.
        // Its field may persist after failure; the retained runtime must Stop,
        // never replay, rewind, or create a new allocation/lifetime journal.
        receipt.entered = true;
        if (!exchange(receipt.address, receipt.original, target, receipt.observed, receipt.exceptionCode))
            return finish(ClockResult::WriteFailure);
        receipt.observedValid = true;
        if (receipt.observed != receipt.original)
            return finish(ClockResult::OwnerConflict);
        receipt.accepted = true;
        if ((result = D::Capture(binding, owner, descriptor, true, &entryWorld, afterWorld, after)) != ClockResult::Ok)
            return finish(result);
        if (!NamedGarageDetail::SameIdentity(selected, after[slot].row) || current[slot].geometry.words != after[slot].geometry.words ||
            after[slot].row.type != (door ? selected.type : target) || (door && after[slot].row.state != target))
            return finish(ClockResult::OwnerConflict);
        if (!D::C::Fence(binding.image, afterWorld))
            return finish(ClockResult::BindingChanged);
        if ((result = D::Begin(binding, owner)) != ClockResult::Ok)
            return finish(result);
        receipt.verified = true;
        return finish(ClockResult::Ok);
    }
}
