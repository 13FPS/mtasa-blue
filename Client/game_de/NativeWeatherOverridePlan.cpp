/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check exact RIP store operands without code mutation or fake masks
 *****************************************************************************/
#include "NativeWeatherOverridePlan.h"

namespace mta::game_de::NativeWeatherOverrideProfile
{
    namespace
    {
        bool ValidImage(const Binding& binding) noexcept
        {
            return sizeof(void*) == 8 && binding.image && binding.image <= (std::numeric_limits<std::uintptr_t>::max)() - NativeWeatherScalarProfile::ImageSize;
        }
        bool ValidWriter(const Writer& writer) noexcept
        {
            if (writer.size > writer.original.size() || writer.displacement < 2 || writer.displacement + 4 > writer.size ||
                writer.rva > NativeWeatherScalarProfile::ImageSize || writer.size > NativeWeatherScalarProfile::ImageSize - writer.rva)
                return false;
            const auto index = static_cast<std::size_t>(writer.field);
            if (index >= NativeWeatherScalarProfile::Fields.size())
                return false;
            const auto& code = writer.original;
            const auto  offset = writer.displacement;
            if ((code[offset - 1] & 0xc7) != 5)
                return false;
            if (writer.kind == Kind::MovImmediate32)
            {
                if (writer.size != 10 || offset != 2 || code[0] != 0xc7 || code[1] != 0x05)
                    return false;
            }
            else if (writer.kind == Kind::Movss)
            {
                if (offset < 4 || code[0] != 0xf3 || writer.size != offset + 4 || code[offset - 3] != 0x0f || code[offset - 2] != 0x11)
                    return false;
            }
            else if (writer.kind == Kind::MovReg32)
            {
                if (writer.size != offset + 4 || code[offset - 2] != 0x89 || (offset != 2 && (offset != 3 || code[0] != 0x44)))
                    return false;
            }
            else
                return false;
            std::int32_t displacement = 0;
            std::memcpy(&displacement, code.data() + offset, sizeof(displacement));
            const auto target = static_cast<std::int64_t>(writer.rva + writer.size) + displacement;
            return target >= 0 && static_cast<std::uintptr_t>(target) == NativeWeatherScalarProfile::Fields[index];
        }
    }
    ClockResult Plan::AccumulatorRange(const Binding& binding, Field field, Range& output) noexcept
    {
        if (!ValidImage(binding) || static_cast<std::size_t>(field) >= WriterCounts.size())
            return ClockResult::InvalidArgument;
        if (!Admits(field))
            return ClockResult::UnsupportedBuild;
        const auto  limit = (std::numeric_limits<std::uintptr_t>::max)();
        Range       range{0, limit - 3};
        std::size_t count = 0;
        for (const auto& writer : Writers)
        {
            if (writer.field != field)
                continue;
            if (!ValidWriter(writer))
                return ClockResult::BindingChanged;
            const auto next = binding.image + writer.rva + writer.size;
            const auto low = next > 0x80000000ull ? next - 0x80000000ull : 0;
            const auto high = next <= limit - 0x7fffffffull ? next + 0x7fffffffull : limit;
            if (low > range.minimum)
                range.minimum = low;
            if (high < range.maximum)
                range.maximum = high;
            ++count;
        }
        if (count != WriterCounts[static_cast<std::size_t>(field)] || range.minimum > limit - 3)
            return ClockResult::BindingChanged;
        range.minimum = (range.minimum + 3) & ~static_cast<std::uintptr_t>(3);
        range.maximum &= ~static_cast<std::uintptr_t>(3);
        if (range.minimum > range.maximum)
            return ClockResult::BindingChanged;
        output = range;
        return ClockResult::Ok;
    }
    ClockResult Plan::Prepare(const Binding& binding, Field scalar, std::uintptr_t accumulator, std::uint64_t ownerGeneration, CodeRead read,
                              Plan& output) noexcept
    {
        Range      range;
        const auto result = AccumulatorRange(binding, scalar, range);
        if (result != ClockResult::Ok)
            return result;
        if (!ownerGeneration || !read || !accumulator || accumulator % 4 || accumulator < range.minimum || accumulator > range.maximum ||
            (accumulator >= binding.image && accumulator < binding.image + NativeWeatherScalarProfile::ImageSize))
            return ClockResult::InvalidArgument;
        if (accumulator < binding.image && binding.image - accumulator < sizeof(float))
            return ClockResult::InvalidArgument;
        Plan prepared;
        prepared.image = binding.image;
        prepared.scratch = accumulator;
        prepared.generation = ownerGeneration;
        prepared.field = scalar;
        for (const auto& writer : Writers)
        {
            if (writer.field != scalar)
                continue;
            std::array<unsigned char, 10> actual{};
            if (!read(binding.image + writer.rva, actual.data(), writer.size))
                return ClockResult::ReadFailure;
            if (std::memcmp(actual.data(), writer.original.data(), writer.size))
                return ClockResult::BindingChanged;
            if (prepared.count >= prepared.entries.size())
                return ClockResult::BindingChanged;
            auto& entry = prepared.entries[prepared.count++];
            entry.writer = writer;
            entry.redirected = writer.original;
            const auto next = binding.image + writer.rva + writer.size;
            const auto distance = accumulator >= next ? static_cast<std::int64_t>(accumulator - next) : -static_cast<std::int64_t>(next - accumulator);
            const auto displacement = static_cast<std::int32_t>(distance);
            std::memcpy(entry.redirected.data() + writer.displacement, &displacement, sizeof(displacement));
        }
        output = prepared;
        return ClockResult::Ok;
    }
    bool Ledger::RetainPossiblePatch(std::size_t index, std::uint64_t ownerGeneration) noexcept
    {
        if (!ownerGeneration || ownerGeneration != plan.Generation() || !plan.At(index))
            return false;
        retained |= 1u << index;
        return true;
    }
    std::size_t Ledger::PendingCount() const noexcept
    {
        std::size_t count = 0;
        for (std::size_t index = 0; index < plan.Count(); ++index)
            count += (retained >> index) & 1u;
        return count;
    }
    Observation Ledger::Observe(std::size_t index, std::uint64_t ownerGeneration, CodeRead read) const noexcept
    {
        const auto* entry = plan.At(index);
        if (!ownerGeneration || ownerGeneration != plan.Generation() || !entry || !read)
            return Observation::Refused;
        std::array<unsigned char, 10> actual{};
        if (!read(plan.ImageIdentity() + entry->writer.rva, actual.data(), entry->writer.size))
            return Observation::Unreadable;
        if (!std::memcmp(actual.data(), entry->writer.original.data(), entry->writer.size))
            return Observation::Stock;
        return !std::memcmp(actual.data(), entry->redirected.data(), entry->writer.size) ? Observation::Redirected : Observation::Foreign;
    }
    ClockResult Ledger::ClearIfStock(std::size_t index, std::uint64_t ownerGeneration, CodeRead read) noexcept
    {
        const auto observation = Observe(index, ownerGeneration, read);
        if (observation == Observation::Stock)
        {
            retained &= ~(1u << index);
            return ClockResult::Ok;
        }
        if (observation == Observation::Redirected)
            return ClockResult::Busy;
        if (observation == Observation::Foreign)
            return ClockResult::OwnerConflict;
        return observation == Observation::Unreadable ? ClockResult::ReadFailure : ClockResult::InvalidArgument;
    }
    bool Ledger::Originalize(std::uint64_t ownerGeneration, std::uintptr_t rva, unsigned char* bytes, std::size_t size, CodeRead read) const noexcept
    {
        if (!ownerGeneration || ownerGeneration != plan.Generation() || !bytes || !size || !read || rva > NativeWeatherScalarProfile::ImageSize ||
            size > NativeWeatherScalarProfile::ImageSize - rva)
            return false;
        for (std::size_t index = 0; index < plan.Count(); ++index)
        {
            if (!(retained & (1u << index)))
                continue;
            const auto& entry = *plan.At(index);
            const auto  begin = entry.writer.rva > rva ? entry.writer.rva : rva;
            const auto  end = entry.writer.rva + entry.writer.size < rva + size ? entry.writer.rva + entry.writer.size : rva + size;
            if (begin >= end)
                continue;
            std::array<unsigned char, 10> actual{};
            if (!read(plan.ImageIdentity() + entry.writer.rva, actual.data(), entry.writer.size))
                return false;
            const bool stock = !std::memcmp(actual.data(), entry.writer.original.data(), entry.writer.size);
            if ((!stock && std::memcmp(actual.data(), entry.redirected.data(), entry.writer.size)) ||
                std::memcmp(bytes + begin - rva, actual.data() + begin - entry.writer.rva, end - begin))
                return false;
            if (!stock)
                std::memcpy(bytes + begin - rva, entry.writer.original.data() + begin - entry.writer.rva, end - begin);
        }
        return true;
    }
}
namespace mta::game_de
{
    bool NativeWeatherOverrideCodeView::Originalize(const Profile& binding, std::uintptr_t rva, unsigned char* bytes, std::size_t size) const noexcept
    {
        if (!binding.image || binding.image > (std::numeric_limits<std::uintptr_t>::max)() - NativeWeatherScalarProfile::ImageSize || !generation || !read ||
            !bytes || !size || rva > NativeWeatherScalarProfile::ImageSize || size > NativeWeatherScalarProfile::ImageSize - rva)
            return false;
        for (const auto* ledger : ledgers)
            if (ledger && ledger->HasPending() &&
                (ledger->Prepared().ImageIdentity() != binding.image || !ledger->Originalize(generation, rva, bytes, size, read)))
                return false;
        return true;
    }
    ClockResult NativeWeatherOverrideCodeView::VerifyWriters(const Profile& binding) const noexcept
    {
        if (!binding.image || binding.image > (std::numeric_limits<std::uintptr_t>::max)() - NativeWeatherScalarProfile::ImageSize || !generation || !read)
            return ClockResult::InvalidArgument;
        for (const auto& writer : NativeWeatherOverrideProfile::Writers)
        {
            std::array<unsigned char, 10> actual{};
            if (!read(binding.image + writer.rva, actual.data(), writer.size))
                return ClockResult::ReadFailure;
            if (!Originalize(binding, writer.rva, actual.data(), writer.size) || std::memcmp(actual.data(), writer.original.data(), writer.size))
                return ClockResult::BindingChanged;
        }
        return ClockResult::Ok;
    }
}
