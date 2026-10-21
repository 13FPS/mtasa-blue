/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Prove all eight direct groups fit actual aligned64 data exchanges
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherOverridePlacement.h"
#include "../../Client/game_de/NativeWeatherOverrideSlot.h"
#include <cstdio>
#include <cstdlib>
#include <intrin.h>
#include <optional>
#include <windows.h>

namespace
{
    using namespace mta::game_de;
    namespace P = NativeWeatherOverridePlacementProfile;
    namespace L = NativeWeatherOverrideProfile;
    using Field = NativeWeatherScalarProfile::Field;
    std::size_t    checks = 0;
    unsigned char* image = nullptr;
    void           Require(bool condition, const char* label)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "Failed check %zu: %s\n", checks, label);
            std::exit(1);
        }
    }
    bool Read(std::uintptr_t address, void* output, std::size_t size) noexcept
    {
        const auto base = reinterpret_cast<std::uintptr_t>(image);
        if (!image || address < base || address - base > NativeWeatherScalarProfile::ImageSize ||
            size > NativeWeatherScalarProfile::ImageSize - (address - base))
            return false;
        SIZE_T count = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &count) && count == size;
    }
    bool Exchange(std::uintptr_t address, const std::array<unsigned char, 8>& before, const std::array<unsigned char, 8>& after) noexcept
    {
        LONG64 expected = 0, desired = 0;
        std::memcpy(&expected, before.data(), 8);
        std::memcpy(&desired, after.data(), 8);
        return address % 8 == 0 && _InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), desired, expected) == expected;
    }
    std::uintptr_t ActualTarget(const L::Writer& writer) noexcept
    {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, image + writer.rva + writer.displacement, 4);
        return static_cast<std::uintptr_t>(static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(image) + writer.rva + writer.size) + displacement);
    }
    void FixturePermissions(std::uintptr_t address, std::size_t minimum)
    {
        MEMORY_BASIC_INFORMATION page{};
        Require(VirtualQuery(reinterpret_cast<const void*>(address), &page, sizeof(page)) != 0, "owned fixture page exists");
        Require(page.Type == MEM_PRIVATE && page.State == MEM_COMMIT && page.Protect == PAGE_READWRITE && page.RegionSize >= minimum,
                "all native-looking fixture bytes and scratch are nonexecutable owned RW memory");
    }
}
int main()
{
    static_assert(sizeof(void*) == 8);
    image = static_cast<unsigned char*>(VirtualAlloc(nullptr, NativeWeatherScalarProfile::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(image != nullptr, "owned nonexecutable image allocated");
    FixturePermissions(reinterpret_cast<std::uintptr_t>(image), NativeWeatherScalarProfile::ImageSize);
    for (const auto& writer : L::Writers)
        std::memcpy(image + writer.rva, writer.original.data(), writer.size);
    for (const auto& window : P::Windows)
        std::memcpy(image + window.windowRva, window.stock.data(), window.stock.size());
    NativeWeatherScalarProfile::Binding binding{};
    binding.image = reinterpret_cast<std::uintptr_t>(image);
    std::array<std::optional<L::Ledger>, 8>  journals;
    std::array<NativeWeatherOverrideSlot, 8> slots;
    constexpr std::uint64_t                  generation = 123;
    P::Placement                             unchanged;
    Require(P::Placement::Prepare({}, Field::Fog, unchanged) == ClockResult::InvalidArgument && !unchanged.Count(),
            "unbound placement refuses without publication");
    Require(P::Placement::Prepare(binding, Field::Rain, unchanged) == ClockResult::Ok && unchanged.Period() == 65536 && unchanged.Count(),
            "Rain's direct-store placement preserves low16 bytes without granting serializer authority");
    Require(P::Placement::Prepare(binding, static_cast<Field>(255), unchanged) == ClockResult::InvalidArgument, "invalid scalar refuses");
    for (const auto field : {Field::Rain, Field::WetRoads, Field::Foggyness, Field::Fog, Field::RainFog, Field::WaterFog, Field::Sandstorm, Field::Rainbow})
    {
        const auto   index = static_cast<std::size_t>(field);
        P::Placement placement;
        Require(P::Placement::Prepare(binding, field, placement) == ClockResult::Ok, "complete group receives constrained placement pattern");
        const auto period = field == Field::WetRoads || field == Field::Foggyness ? 16777216u : 65536u;
        Require(placement.Period() == period && placement.Count() && placement.Count() <= 65536,
                "period derived from worst boundary prefix; signed32 range remains bounded");
        Require(placement.Offset() == (binding.image + NativeWeatherScalarProfile::Fields[index]) % P::AllocationBytes,
                "interior offset preserves field's low16 bits");
        P::Candidate first;
        bool         haveFirst = false;
        for (std::size_t probe = 0; probe < placement.Count(); ++probe)
            if (placement.At(probe, first))
            {
                haveFirst = true;
                break;
            }
        Require(haveFirst && placement.Accepts(first), "at least one independent arithmetic candidate exists");
        const auto   nativeBlock = (binding.image + NativeWeatherScalarProfile::Fields[index]) & ~(P::AllocationBytes - 1);
        const auto   nativeIndex = static_cast<std::size_t>((nativeBlock - first.allocation) / placement.Period());
        P::Candidate untouched{17, 19};
        Require(!placement.At(nativeIndex, untouched) && untouched.allocation == 17 && untouched.accumulator == 19,
                "native image block is refused without mutating output");
        Require(!placement.At(placement.Count(), untouched) && untouched.allocation == 17, "candidate index cannot escape prepared signed32 domain");
        auto wrong = first;
        wrong.accumulator += 4;
        Require(!placement.Accepts(wrong), "arbitrary interior pointer does not grant preserved-boundary semantics");
        Require(NativeWeatherOverrideSlot::Allocate(placement, slots[index]) == ClockResult::Ok && slots[index].Valid(),
                "real Win32 private64KiB allocation admits a congruent interior slot");
        Require(placement.Accepts({slots[index].AllocationIdentity(), slots[index].Accumulator()}), "actual allocation satisfies complete placement contract");
        FixturePermissions(slots[index].AllocationIdentity(), P::AllocationBytes);
        Require(slots[index].Accumulator() - slots[index].AllocationIdentity() == placement.Offset(),
                "allocation identity stays distinct from accumulator pointer");
        const auto allocation = slots[index].AllocationIdentity();
        Require(NativeWeatherOverrideSlot::Allocate(placement, slots[index]) == ClockResult::Busy && slots[index].AllocationIdentity() == allocation,
                "new allocation cannot replace retained slot ownership");
        L::Plan logical;
        Require(L::Plan::Prepare(binding, field, slots[index].Accumulator(), generation, &Read, logical) == ClockResult::Ok,
                "existing exact logical writer plan accepts constrained slot");
        L::Plan noncongruent;
        Require(L::Plan::Prepare(binding, field, slots[index].Accumulator() + 4, generation, &Read, noncongruent) == ClockResult::Ok,
                "arbitrary near plan by itself proves no atomic boundary");
        journals[index].emplace(logical);
        for (std::size_t site = 0; site < logical.Count(); ++site)
        {
            const auto* entry = logical.At(site);
            const auto* window = P::Find(entry->writer.rva);
            Require(window && window->field == field && window->windowRva % 8 == 0, "each complete native producer has one exact aligned64 window");
            std::array<unsigned char, 8> desired{};
            Require(P::Build(*window, *entry, desired), "all changed displacement bytes fit in selected aligned64 word");
            if (window->unchangedLowBytes)
            {
                std::array<unsigned char, 8> refused{};
                Require(!P::Build(*window, *noncongruent.At(site), refused), "wrong low displacement bytes cannot be copied partially across boundary");
                const auto operand = entry->writer.rva + entry->writer.displacement;
                for (std::size_t byte = 0; byte < window->unchangedLowBytes; ++byte)
                    Require(entry->writer.original[entry->writer.displacement + byte] == entry->redirected[entry->writer.displacement + byte] &&
                                operand + byte < window->windowRva,
                            "preserved displacement prefix remains byte-identical outside atomic word");
            }
            Require(journals[index]->RetainPossiblePatch(site, generation), "actual code reference is journaled before atomic data exchange");
            Require(Exchange(binding.image + window->windowRva, window->stock, desired), "one real aligned64 CAS changes complete remaining operand");
            Require(!Exchange(binding.image + window->windowRva, window->stock, desired), "failed atomic compare does not overwrite redirected window");
            Require(ActualTarget(entry->writer) == slots[index].Accumulator(),
                    "independent RIP decode resolves exact owned accumulator after boundary-preserving CAS");
            Require(journals[index]->Observe(site, generation, &Read) == L::Observation::Redirected,
                    "whole native instruction classifies exact same-generation redirect");
        }
        std::array<const L::Ledger*, 8> pointers{};
        for (std::size_t owner = 0; owner < journals.size(); ++owner)
            if (journals[owner])
                pointers[owner] = &*journals[owner];
        Require(NativeWeatherOverrideCodeView(pointers, generation, &Read).VerifyWriters(binding) == ClockResult::Ok,
                "simultaneous constrained groups reuse existing exact full-instruction masks");
    }
    for (std::size_t index = 0; index < journals.size(); ++index)
        if (journals[index])
        {
            for (std::size_t site = 0; site < journals[index]->Prepared().Count(); ++site)
            {
                const auto*                  entry = journals[index]->Prepared().At(site);
                const auto*                  window = P::Find(entry->writer.rva);
                std::array<unsigned char, 8> redirected{};
                Require(P::Build(*window, *entry, redirected), "immutable reset uses same constrained plan");
                Require(Exchange(binding.image + window->windowRva, redirected, window->stock), "reset atomically restores exact remaining bytes");
                Require(journals[index]->ClearIfStock(site, generation, &Read) == ClockResult::Ok, "whole stock instruction retires only its native reference");
            }
            Require(!journals[index]->HasPending(), "every native reference retired before release");
            DWORD previous = 0, ignored = 0;
            Require(VirtualProtect(reinterpret_cast<void*>(slots[index].AllocationIdentity()), P::AllocationBytes, PAGE_READONLY, &previous) != FALSE,
                    "inject foreign scratch protection change");
            Require(!slots[index].Valid() && !slots[index].Release() && slots[index].HasAllocation(),
                    "foreign allocation state preserves explicit retained lifetime debt");
            Require(VirtualProtect(reinterpret_cast<void*>(slots[index].AllocationIdentity()), P::AllocationBytes, previous, &ignored) != FALSE,
                    "restore independently owned allocation protection");
            Require(slots[index].Release() && !slots[index].HasAllocation() && !slots[index].Accumulator(),
                    "explicit release frees allocation base, never interior pointer");
            Require(slots[index].Release(), "empty slot release has no hidden native side effect");
        }
    for (const auto& writer : L::Writers)
        Require(std::memcmp(image + writer.rva, writer.original.data(), writer.size) == 0, "all37 native-looking writers including Rain restore exactly");
    Require(VirtualFree(image, 0, MEM_RELEASE) != FALSE, "owned image released");
    image = nullptr;
    std::printf("Passed %zu native weather constrained placement checks (64-bit; owned-memory)\n", checks);
    return 0;
}
