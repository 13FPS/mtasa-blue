/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Typed native writer suppression plans and strict retained-code views
 *****************************************************************************/
#pragma once
#include "NativeWeatherScalarProfile.h"
#include <game/CGameClockService.h>
#include <cstring>
#include <limits>

namespace mta::game_de::NativeWeatherOverrideProfile
{
    using Field = NativeWeatherScalarProfile::Field;
    using Binding = NativeWeatherScalarProfile::Binding;
    using CodeRead = bool (*)(std::uintptr_t, void*, std::size_t) noexcept;
    enum class Kind : std::uint8_t
    {
        Movss,
        MovReg32,
        MovImmediate32
    };
    struct Writer
    {
        Field                         field;
        std::uintptr_t                rva;
        std::size_t                   size, displacement;
        Kind                          kind;
        std::array<unsigned char, 10> original;
    };
    extern const std::array<Writer, 37>         Writers;
    inline constexpr std::array<std::size_t, 8> WriterCounts = {7, 15, 2, 2, 4, 3, 3, 1};
    // This is low-level direct-store plan admission only. Rain's separate
    // direction-aware serializer and old/new borrow retirement owner must be
    // installed in the same cohort; these seven stores alone are incomplete.
    inline constexpr bool Admits(Field field) noexcept
    {
        const auto index = static_cast<std::size_t>(field);
        return index < WriterCounts.size();
    }
    struct Range
    {
        std::uintptr_t minimum = 0, maximum = 0;
    };
    class Plan final
    {
    public:
        struct Entry
        {
            Writer                        writer{};
            std::array<unsigned char, 10> redirected{};
        };
        // Addresses are borrowed from an already qualified native owner. This
        // computes/checks bytes only; it never grants code-write authority.
        static ClockResult AccumulatorRange(const Binding&, Field, Range&) noexcept;
        static ClockResult Prepare(const Binding&, Field, std::uintptr_t accumulator, std::uint64_t generation, CodeRead, Plan&) noexcept;
        std::uintptr_t     ImageIdentity() const noexcept { return image; }
        std::uintptr_t     Accumulator() const noexcept { return scratch; }
        std::uint64_t      Generation() const noexcept { return generation; }
        Field              Scalar() const noexcept { return field; }
        std::size_t        Count() const noexcept { return count; }
        const Entry*       At(std::size_t index) const noexcept { return index < count ? &entries[index] : nullptr; }

    private:
        std::uintptr_t        image = 0, scratch = 0;
        std::uint64_t         generation = 0;
        Field                 field = Field::Rain;
        std::size_t           count = 0;
        std::array<Entry, 15> entries{};
    };
    enum class Observation : std::uint8_t
    {
        Refused,
        Unreadable,
        Stock,
        Redirected,
        Foreign
    };
    class Ledger final
    {
    public:
        explicit Ledger(const Plan& prepared) noexcept : plan(prepared) {}
        const Plan& Prepared() const noexcept { return plan; }
        bool        RetainPossiblePatch(std::size_t index, std::uint64_t ownerGeneration) noexcept;
        bool        HasPending() const noexcept { return retained != 0; }
        std::size_t PendingCount() const noexcept;
        Observation Observe(std::size_t, std::uint64_t ownerGeneration, CodeRead) const noexcept;
        ClockResult ClearIfStock(std::size_t, std::uint64_t ownerGeneration, CodeRead) noexcept;
        bool        Originalize(std::uint64_t ownerGeneration, std::uintptr_t pinRva, unsigned char* bytes, std::size_t size, CodeRead) const noexcept;

    private:
        // Retain BEFORE an attempted patch. Unconfirmed code writes and all
        // references to scratch keep this ledger, allocation and module alive.
        // Only exact original bytes clear debt; foreign code is preserved and
        // cannot become a guessed successful Reset or permission to free data.
        const Plan    plan;
        std::uint32_t retained = 0;
    };
}
namespace mta::game_de
{
    class NativeWeatherOverrideCodeView final
    {
    public:
        using Profile = NativeWeatherScalarProfile::Binding;
        using Ledger = NativeWeatherOverrideProfile::Ledger;
        using CodeRead = NativeWeatherOverrideProfile::CodeRead;
        NativeWeatherOverrideCodeView(const std::array<const Ledger*, 8>& owned, std::uint64_t generation, CodeRead rawReader) noexcept
            : ledgers(owned), generation(generation), read(rawReader)
        {
        }
        bool        Originalize(const Profile&, std::uintptr_t rva, unsigned char*, std::size_t) const noexcept;
        ClockResult VerifyWriters(const Profile&) const noexcept;

    private:
        const std::array<const Ledger*, 8> ledgers;
        const std::uint64_t                generation;
        const CodeRead                     read;
    };
}
