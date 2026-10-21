/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact native operand plans and owned-code profile recovery proofs
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherOverridePlan.h"
#include "../../Client/game_de/NativeImageAccess.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <optional>

namespace
{
    namespace P = mta::game_de::NativeWeatherScalarProfile;
    namespace T = mta::game_de::NativeWeatherTransitionProfile;
    namespace O = mta::game_de::NativeWeatherOverrideProfile;
    using View = mta::game_de::NativeWeatherOverrideCodeView;
    using Result = mta::game_de::ClockResult;
    using Field = P::Field;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
            std::exit(1);
        }
    }
}
#if defined(MTA_TEST_WEATHER_OVERRIDE_CONTROLLED)
namespace
{
    std::uintptr_t selectedImage = 0, deniedImageRva = 0;
    unsigned       imageReads = 0, failImageReadAt = 0;
    bool           buildAllowed = true;
    HMODULE WINAPI FixtureModule(LPCWSTR name)
    {
        return name ? ::GetModuleHandleW(name) : reinterpret_cast<HMODULE>(selectedImage);
    }
}
namespace mta::game_de::ScalarFixtureImage
{
    bool CheckExecutableBuild() noexcept
    {
        return buildAllowed;
    }
    bool ImagePages(std::uintptr_t image, std::uintptr_t address, std::size_t size, bool = false, bool = false) noexcept
    {
        return image && image == selectedImage && address >= image && address - image <= P::ImageSize && size <= P::ImageSize - (address - image) &&
               !(deniedImageRva && address <= image + deniedImageRva && size > image + deniedImageRva - address);
    }
    bool ReadImage(std::uintptr_t image, std::uintptr_t address, void* output, std::size_t size, bool writable = false, bool executable = false) noexcept
    {
        ++imageReads;
        if (imageReads == failImageReadAt || !ImagePages(image, address, size, writable, executable))
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) && copied == size;
    }
    bool SectionContains(const IMAGE_SECTION_HEADER& section, std::size_t rva, std::size_t size, DWORD required, DWORD forbidden) noexcept
    {
        return NativeImageAccess::SectionContains(section, rva, size, required, forbidden);
    }
}
    #define NativeImageAccess ScalarFixtureImage
    #define GetModuleHandleW  FixtureModule
#endif
#include "../../Client/game_de/NativeWeatherTransitionProfile.cpp"
#include "../../Client/game_de/NativeWeatherScalarProfile.cpp"
#if defined(MTA_TEST_WEATHER_OVERRIDE_CONTROLLED)
    #undef NativeImageAccess
    #undef GetModuleHandleW
namespace
{
    class OwnedImage
    {
    public:
        unsigned char* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, P::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        OwnedImage()
        {
            Check(bytes != nullptr, "allocate only owned ordinary-host image memory");
            selectedImage = reinterpret_cast<std::uintptr_t>(bytes);
            auto& dos = *reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
            dos.e_magic = IMAGE_DOS_SIGNATURE;
            dos.e_lfanew = 0x100;
            auto& nt = *reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + 0x100);
            nt.Signature = IMAGE_NT_SIGNATURE;
            nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
            nt.FileHeader.NumberOfSections = 2;
            nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
            nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
            nt.OptionalHeader.SizeOfImage = static_cast<DWORD>(P::ImageSize);
            auto* sections = reinterpret_cast<IMAGE_SECTION_HEADER*>(bytes + 0x100 + sizeof(IMAGE_NT_HEADERS64));
            sections[0].VirtualAddress = 0x1000;
            sections[0].Misc.VirtualSize = 0x3f40000;
            sections[0].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
            sections[1].VirtualAddress = 0x3f41000;
            sections[1].Misc.VirtualSize = static_cast<DWORD>(P::ImageSize - 0x3f41000);
            sections[1].Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
            for (const auto& pin : T::Code)
                std::memcpy(bytes + pin.rva, pin.bytes.data(), pin.size);
            for (const auto& pin : P::Code)
                std::memcpy(bytes + pin.rva, pin.bytes.data(), pin.size);
            for (const auto& writer : O::Writers)
                std::memcpy(bytes + writer.rva, writer.original.data(), writer.size);
            for (const auto& name : P::Names)
            {
                std::memcpy(bytes + name.nameRva, name.name, std::strlen(name.name) + 1);
                const auto pointer = static_cast<std::uint64_t>(selectedImage + name.nameRva);
                std::memcpy(bytes + name.recordRva, &pointer, sizeof(pointer));
                std::memcpy(bytes + name.recordRva + (name.propertyOffset ? 36 : 8), &name.value, sizeof(name.value));
            }
            const wchar_t intensity[] = L"RainbowIntensity", mesh[] = L"RainbowMesh";
            std::memcpy(bytes + P::RainbowIntensityNameRva, intensity, sizeof(intensity));
            std::memcpy(bytes + P::RainbowMeshNameRva, mesh, sizeof(mesh));
            for (std::size_t index = 0; index < P::Fields.size(); ++index)
            {
                const auto value = static_cast<float>(index) / 8.0f;
                std::memcpy(bytes + P::Fields[index], &value, sizeof(value));
            }
        }
        ~OwnedImage()
        {
            selectedImage = 0;
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
    };
}
#endif

#if defined(MTA_TEST_WEATHER_OVERRIDE_CONTROLLED)
namespace
{
    unsigned rawCalls = 0;
    bool     denyRaw = false;
    bool     RawCode(std::uintptr_t address, void* output, std::size_t size) noexcept
    {
        ++rawCalls;
        if (denyRaw || !selectedImage || address < selectedImage || address - selectedImage > P::ImageSize || size > P::ImageSize - (address - selectedImage))
            return false;
        SIZE_T copied = 0;
        return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &copied) && copied == size;
    }
    class OwnedScratch
    {
    public:
        unsigned char* bytes = nullptr;
        explicit OwnedScratch(const P::Binding& binding)
        {
            O::Range range;
            Check(O::Plan::AccumulatorRange(binding, Field::WetRoads, range) == Result::Ok, "compute native common RIP range for owned scratch allocation");
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            const auto granularity = static_cast<std::uintptr_t>(info.dwAllocationGranularity);
            auto       cursor = (binding.image + P::ImageSize + granularity - 1) & ~(granularity - 1);
            for (unsigned attempt = 0; attempt < 256 && cursor <= range.maximum; ++attempt)
            {
                MEMORY_BASIC_INFORMATION region{};
                if (VirtualQuery(reinterpret_cast<const void*>(cursor), &region, sizeof(region)) != sizeof(region))
                    break;
                if (region.State == MEM_FREE)
                {
                    bytes = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(cursor), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
                    if (bytes)
                        break;
                }
                const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
                if (end <= cursor || end > (std::numeric_limits<std::uintptr_t>::max)() - granularity)
                    break;
                cursor = (end + granularity - 1) & ~(granularity - 1);
            }
            Check(bytes != nullptr, "allocate only fixture-owned writable nonexecutable scratch within signed32 RIP reach");
        }
        ~OwnedScratch()
        {
            if (bytes)
                VirtualFree(bytes, 0, MEM_RELEASE);
        }
    };
    void CopyEntry(OwnedImage& image, const O::Plan::Entry& entry, bool redirected)
    {
        std::memcpy(image.bytes + entry.writer.rva, redirected ? entry.redirected.data() : entry.writer.original.data(), entry.writer.size);
    }
}
#endif
int main()
{
    P::Binding binding{11};
#if !defined(MTA_TEST_WEATHER_OVERRIDE_CONTROLLED)
    Check(!P::Qualify(binding) && binding.image == 11, "shipping exact image refuses ordinary host before override identity");
    Check(!P::Revalidate(binding), "ordinary executable cannot borrow supported native weather binding");
    std::printf("Passed %u native weather override plan checks (64-bit; shipping-wrong-image)\n", checks);
#else
    OwnedImage image;
    Check(P::Qualify(binding) && binding.image == selectedImage, "qualified actual weather identity in owned controlled image");
    OwnedScratch                          scratch(binding);
    const auto                            accumulator = reinterpret_cast<std::uintptr_t>(scratch.bytes);
    constexpr std::uint64_t               generation = 31;
    const std::array<const O::Ledger*, 8> none{};
    const View                            stock(none, generation, RawCode);
    Check(stock.VerifyWriters(binding) == Result::Ok && P::Revalidate(binding, stock), "all37 exact native writer instructions qualify in stock image");
    O::Range range{17, 19};
    Check(O::Plan::AccumulatorRange(binding, Field::Rain, range) == Result::UnsupportedBuild && range.minimum == 17 && range.maximum == 19,
          "unresolved Rain serializer address escape prevents false complete setter plan");
    Check(O::Plan::AccumulatorRange(binding, static_cast<Field>(255), range) == Result::InvalidArgument && range.minimum == 17,
          "unknown field preserves range output");
    Check(O::Plan::AccumulatorRange({0}, Field::WetRoads, range) == Result::InvalidArgument, "invalid image cannot acquire writer range");
    Check(O::Plan::AccumulatorRange({(std::numeric_limits<std::uintptr_t>::max)() - 1}, Field::WetRoads, range) == Result::InvalidArgument,
          "overflow image refuses before pointer arithmetic");
    for (std::size_t scalar = 1; scalar < P::Fields.size(); ++scalar)
    {
        const auto field = static_cast<Field>(scalar);
        O::Plan    plan;
        Check(O::Plan::Prepare(binding, field, accumulator + scalar * 4, generation, RawCode, plan) == Result::Ok && plan.Count() == O::WriterCounts[scalar],
              "prepare every qualified native writer group");
        Check(plan.ImageIdentity() == binding.image && plan.Scalar() == field && plan.Accumulator() == accumulator + scalar * 4 &&
                  plan.Generation() == generation,
              "sealed plan retains exact image field scratch and generation identity");
        Check(plan.At(plan.Count()) == nullptr, "out-of-range patch entry cannot become arbitrary instruction");
        for (std::size_t index = 0; index < plan.Count(); ++index)
        {
            const auto&  entry = *plan.At(index);
            std::int32_t displacement = 0;
            std::memcpy(&displacement, entry.redirected.data() + entry.writer.displacement, sizeof(displacement));
            const auto next = binding.image + entry.writer.rva + entry.writer.size;
            const auto target = displacement >= 0 ? next + static_cast<std::uint32_t>(displacement)
                                                  : next - static_cast<std::uint64_t>(-static_cast<std::int64_t>(displacement));
            Check(target == plan.Accumulator(), "actual full instruction length including immediate tail determines redirected RIP target");
            for (std::size_t byte = 0; byte < entry.writer.size; ++byte)
                if (byte < entry.writer.displacement || byte >= entry.writer.displacement + 4)
                    Check(entry.redirected[byte] == entry.writer.original[byte],
                          "operand-only plan preserves exact opcode register and trailing immediate bytes");
        }
        O::Range reachable;
        Check(O::Plan::AccumulatorRange(binding, field, reachable) == Result::Ok, "intersect all signed32 RIP ranges for group");
        O::Plan boundary;
        Check(O::Plan::Prepare(binding, field, reachable.minimum ? reachable.minimum : 4, generation, RawCode, boundary) == Result::Ok,
              "lower nonnull aligned common signed32 boundary encodes without wrap");
        Check(O::Plan::Prepare(binding, field, reachable.maximum, generation, RawCode, boundary) == Result::Ok,
              "upper aligned common signed32 boundary encodes without wrap");
        const auto boundaryGeneration = boundary.Generation();
        const auto calls = rawCalls;
        Check(O::Plan::Prepare(binding, field, reachable.minimum >= 4 ? reachable.minimum - 4 : 0, generation, RawCode, boundary) == Result::InvalidArgument &&
                  rawCalls == calls,
              "below-range scratch refuses before any code read");
        Check(O::Plan::Prepare(binding, field, reachable.maximum + 4, generation, RawCode, boundary) == Result::InvalidArgument &&
                  boundary.Generation() == boundaryGeneration,
              "above-range scratch cannot truncate RIP displacement or overwrite output plan");
        Check(O::Plan::Prepare(binding, field, accumulator + 1, generation, RawCode, boundary) == Result::InvalidArgument,
              "unaligned fake accumulator refuses");
        Check(O::Plan::Prepare(binding, field, binding.image + P::Fields[scalar], generation, RawCode, boundary) == Result::InvalidArgument,
              "native image memory cannot be substituted for owned fake accumulator");
        Check(O::Plan::Prepare(binding, field, accumulator, 0, RawCode, boundary) == Result::InvalidArgument &&
                  O::Plan::Prepare(binding, field, accumulator, generation, nullptr, boundary) == Result::InvalidArgument,
              "missing owner generation or raw reader refuses plan");
        auto& changed = image.bytes[plan.At(0)->writer.rva];
        changed ^= 1;
        Check(O::Plan::Prepare(binding, field, accumulator, generation, RawCode, boundary) == Result::BindingChanged,
              "changed native writer opcode prevents planning");
        changed ^= 1;
        O::Ledger                       ledger(plan);
        std::array<const O::Ledger*, 8> owned{};
        owned[scalar] = &ledger;
        const View view(owned, generation, RawCode), stale(owned, generation + 1, RawCode);
        Check(!ledger.RetainPossiblePatch(plan.Count(), generation) && !ledger.RetainPossiblePatch(0, generation + 1),
              "unknown entry and stale generation cannot declare patch debt");
        for (std::size_t index = 0; index < plan.Count(); ++index)
        {
            Check(ledger.RetainPossiblePatch(index, generation), "journal possible native patch before mutation attempt");
            CopyEntry(image, *plan.At(index), true);
            Check(ledger.Observe(index, generation, RawCode) == O::Observation::Redirected,
                  "exact live owned instruction recognized without general operand mask");
        }
        Check(ledger.PendingCount() == plan.Count() && ledger.HasPending(), "scratch/module debt counts every possible live native reference");
        Check(!P::Revalidate(binding) && P::Revalidate(binding, view) && view.VerifyWriters(binding) == Result::Ok,
              "actual getter profile accepts only same-generation recorded exact owned redirects");
        Check(!P::Revalidate(binding, stale) && !P::Revalidate(binding, stock), "stale owner or unrecorded redirect cannot bypass real producer pins");
        for (std::size_t index = 0; index < plan.Count(); ++index)
        {
            const auto& entry = *plan.At(index);
            image.bytes[entry.writer.rva] ^= 1;
            Check(ledger.Observe(index, generation, RawCode) == O::Observation::Foreign && !P::Revalidate(binding, view),
                  "foreign opcode is never hidden by owned displacement masking");
            Check(ledger.ClearIfStock(index, generation, RawCode) == Result::OwnerConflict && ledger.HasPending(),
                  "foreign code is preserved and retains scratch lifetime debt");
            image.bytes[entry.writer.rva] ^= 1;
            image.bytes[entry.writer.rva + entry.writer.displacement] ^= 1;
            Check(!P::Revalidate(binding, view), "foreign displacement to different target is not accepted as owned scratch");
            image.bytes[entry.writer.rva + entry.writer.displacement] ^= 1;
            Check(ledger.ClearIfStock(index, generation, RawCode) == Result::Busy, "live redirected instruction cannot claim successful Reset");
        }
        const auto&                  first = *plan.At(0);
        std::array<unsigned char, 4> fragment{};
        std::memcpy(fragment.data(), first.redirected.data() + first.writer.displacement, fragment.size());
        Check(ledger.Originalize(generation, first.writer.rva + first.writer.displacement, fragment.data(), fragment.size(), RawCode) &&
                  !std::memcmp(fragment.data(), first.writer.original.data() + first.writer.displacement, fragment.size()),
              "partial pin originalization verifies the whole actual owned instruction");
        image.bytes[first.writer.rva] ^= 1;
        std::memcpy(fragment.data(), first.redirected.data() + first.writer.displacement, fragment.size());
        Check(!ledger.Originalize(generation, first.writer.rva + first.writer.displacement, fragment.data(), fragment.size(), RawCode),
              "opcode corruption outside partial pin still refuses masking");
        image.bytes[first.writer.rva] ^= 1;
        denyRaw = true;
        Check(view.VerifyWriters(binding) == Result::ReadFailure && ledger.ClearIfStock(0, generation, RawCode) == Result::ReadFailure && ledger.HasPending(),
              "unreadable code neither qualifies nor clears possible native references");
        denyRaw = false;
        for (std::size_t index = 0; index < plan.Count(); ++index)
        {
            CopyEntry(image, *plan.At(index), false);
            Check(ledger.ClearIfStock(index, generation, RawCode) == Result::Ok, "only exact restored original instruction clears one patch debt");
        }
        Check(!ledger.HasPending() && ledger.PendingCount() == 0 && P::Revalidate(binding, view),
              "all references retired before owned scratch can be released");
        Check(ledger.RetainPossiblePatch(0, generation) && P::Revalidate(binding, view) && ledger.HasPending(),
              "unconfirmed attempt with live stock bytes retains acknowledgement debt without fake mask");
        Check(ledger.ClearIfStock(0, generation, RawCode) == Result::Ok && !ledger.HasPending(),
              "confirmed no-write attempt explicitly retires before scratch release");
    }
    std::array<O::Plan, 8>                  plans;
    std::array<std::optional<O::Ledger>, 8> ledgers;
    std::array<const O::Ledger*, 8>         all{};
    for (std::size_t scalar = 1; scalar < plans.size(); ++scalar)
    {
        Check(O::Plan::Prepare(binding, static_cast<Field>(scalar), accumulator + scalar * 4, generation, RawCode, plans[scalar]) == Result::Ok,
              "prepare multiple independent actual scalar groups while stock");
        ledgers[scalar].emplace(plans[scalar]);
        all[scalar] = &*ledgers[scalar];
    }
    for (std::size_t scalar = 1; scalar < plans.size(); ++scalar)
        for (std::size_t index = 0; index < plans[scalar].Count(); ++index)
        {
            Check(ledgers[scalar]->RetainPossiblePatch(index, generation), "record all multi-field native references");
            CopyEntry(image, *plans[scalar].At(index), true);
        }
    const View combined(all, generation, RawCode);
    Check(!P::Revalidate(binding) && P::Revalidate(binding, combined), "all seven simultaneous groups qualify without arbitrary byte masks");
    for (std::size_t index = 0; index < plans[2].Count(); ++index)
    {
        CopyEntry(image, *plans[2].At(index), false);
        Check(ledgers[2]->ClearIfStock(index, generation, RawCode) == Result::Ok, "Reset one scalar retires only its own operands");
    }
    Check(!ledgers[2]->HasPending() && ledgers[1]->HasPending() && P::Revalidate(binding, combined),
          "other owned overrides persist through independent scalar Reset");
    const O::Writer* rain = nullptr;
    for (const auto& writer : O::Writers)
        if (writer.field == Field::Rain)
        {
            rain = &writer;
            break;
        }
    Check(rain != nullptr, "actual external Rain producer remains part of complete writer identity");
    image.bytes[rain->rva] ^= 1;
    Check(combined.VerifyWriters(binding) == Result::BindingChanged, "extra producer changes outside standard getter pins still refuse full override identity");
    image.bytes[rain->rva] ^= 1;
    for (std::size_t scalar = 1; scalar < plans.size(); ++scalar)
        for (std::size_t index = 0; index < plans[scalar].Count(); ++index)
        {
            CopyEntry(image, *plans[scalar].At(index), false);
            Check(ledgers[scalar]->ClearIfStock(index, generation, RawCode) == Result::Ok,
                  "retire every exact owned reference before fixture scratch destruction");
        }
    Check(stock.VerifyWriters(binding) == Result::Ok && P::Revalidate(binding), "complete original code preserved after all scalar Reset proofs");
    std::printf("Passed %u native weather override plan checks (64-bit; controlled-image)\n", checks);
#endif
    return 0;
}
