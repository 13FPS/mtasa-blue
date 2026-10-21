/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned nonexecutable-memory coverage of actual weather owner journals
 *****************************************************************************/
#include "../../Client/game_de/NativeWeatherOverrideOwner.h"
#include "../../Client/game_de/NativeWeatherOverrideSlot.h"
#include <intrin.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include <windows.h>

namespace
{
    using namespace mta::game_de;
    namespace P = NativeWeatherOverrideProfile;
    namespace A = NativeWeatherOverrideAtomicProfile;
    using Field = NativeWeatherScalarProfile::Field;
    using Exchange = NativeWeatherOverridePlatform::Exchange;
    using Bytes = NativeWeatherOverridePlatform::Bytes;
    std::size_t checks = 0;
    void        Require(bool value, const char* label)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "Failed check %zu: %s\n", checks, label);
            std::exit(1);
        }
    }
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t result = 0;
        std::memcpy(&result, &value, 4);
        return result;
    }
    struct Host final : NativeWeatherOverrideHost
    {
        std::uint64_t generation = 37;
        ClockResult   normal = ClockResult::Ok, recover = ClockResult::Ok, park = ClockResult::Ok;
        bool          held = false, failFinish = false, safe = true;
        std::size_t   parks = 0, finishes = 0;
        ClockResult   Check(std::uint64_t value, bool recovering) noexcept override
        {
            return value != generation ? ClockResult::OwnerConflict : recovering ? recover : normal;
        }
        ClockResult ParkPeers(const Window* windows, std::size_t count) noexcept override
        {
            ++parks;
            CheckWindows(windows, count);
            held = true;  // A failed park can still own a partial suspension.
            return park;
        }
        static void CheckWindows(const Window* windows, std::size_t count) noexcept
        {
            Require(windows && count == A::Windows.size(), "host receives complete immutable exclusion list");
            for (std::size_t index = 0; index < count; ++index)
            {
                Require(windows[index].ExclusionBegin() <= windows[index].writerRva, "exclude instruction start outside CAS window");
                Require(windows[index].ExclusionBegin() <= windows[index].windowRva, "exclude full CAS start");
                Require(windows[index].ExclusionEnd() >= windows[index].writerRva + windows[index].writerSize, "exclude instruction end");
                Require(windows[index].ExclusionEnd() >= windows[index].windowRva + windows[index].windowSize, "exclude full CAS end");
            }
        }
        bool PeersHeld() const noexcept override { return held; }
        bool FinishPeers(bool safeToResume) noexcept override
        {
            ++finishes;
            safe &= safeToResume;
            if (failFinish)
                return false;
            held = false;
            return true;
        }
    };
    struct Fixture;
    Fixture* current = nullptr;
    struct Fixture
    {
        unsigned char* image = nullptr;
        bool           valid = true, unreadableCode = false, unreadableScalar = false;
        bool           failAfterCodeExchange = false, failAfterScalarExchange = false;
        bool           foreignScalarAfterExchange = false;
        Fixture()
        {
            image = static_cast<unsigned char*>(VirtualAlloc(nullptr, NativeWeatherScalarProfile::ImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(image != nullptr, "owned nonexecutable sparse image allocated");
            current = this;
            for (const auto& writer : P::Writers)
                std::memcpy(image + writer.rva, writer.original.data(), writer.size);
            for (const auto& window : A::Windows)
                std::memcpy(image + window.windowRva, window.stock.data(), window.windowSize);
            for (const auto field :
                 {Field::Rain, Field::WetRoads, Field::Foggyness, Field::Fog, Field::RainFog, Field::WaterFog, Field::Sandstorm, Field::Rainbow})
                SetBits(field, Bits(0.125f));
        }
        ~Fixture()
        {
            current = nullptr;
            VirtualFree(image, 0, MEM_RELEASE);
        }
        NativeWeatherScalarProfile::Binding Binding() const noexcept
        {
            NativeWeatherScalarProfile::Binding binding{};
            binding.image = reinterpret_cast<std::uintptr_t>(image);
            return binding;
        }
        static bool Read(std::uintptr_t address, void* output, std::size_t size) noexcept
        {
            if (!current || current->unreadableCode || !output || !size)
                return false;
            const auto base = reinterpret_cast<std::uintptr_t>(current->image);
            if (address < base || address - base > NativeWeatherScalarProfile::ImageSize || size > NativeWeatherScalarProfile::ImageSize - (address - base))
                return false;
            SIZE_T read = 0;
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), output, size, &read) && read == size;
        }
        std::uint32_t GetBits(Field field) const noexcept
        {
            std::uint32_t value = 0;
            std::memcpy(&value, image + NativeWeatherScalarProfile::Fields[static_cast<std::size_t>(field)], 4);
            return value;
        }
        void SetBits(Field field, std::uint32_t value) noexcept
        {
            std::memcpy(image + NativeWeatherScalarProfile::Fields[static_cast<std::size_t>(field)], &value, 4);
        }
        bool Stock(Field field) const noexcept
        {
            for (const auto& writer : P::Writers)
                if (writer.field == field && std::memcmp(image + writer.rva, writer.original.data(), writer.size))
                    return false;
            return true;
        }
        std::uintptr_t Target(const P::Writer& writer) const noexcept
        {
            std::int32_t displacement = 0;
            std::memcpy(&displacement, image + writer.rva + writer.displacement, 4);
            return static_cast<std::uintptr_t>(static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(image) + writer.rva + writer.size) + displacement);
        }
    };
    Exchange AtomicWindow(std::uintptr_t address, std::size_t size, const Bytes& before, const Bytes& after) noexcept
    {
        LONG64 expected = 0, low = 0, high = 0;
        std::memcpy(&expected, before.data(), 8);
        std::memcpy(&low, after.data(), 8);
        std::memcpy(&high, after.data() + 8, 8);
        alignas(16) LONG64 expected128[2]{};
        std::memcpy(expected128, before.data(), 16);
        __try
        {
            const bool changed = size == 8 ? _InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address), low, expected) == expected
                                           : _InterlockedCompareExchange128(reinterpret_cast<volatile LONG64*>(address), high, low, expected128) != 0;
            return changed ? Exchange::Exchanged : Exchange::Conflict;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return Exchange::Unavailable;
        }
    }
    struct Platform final : NativeWeatherOverridePlatform
    {
        Fixture&                                 fixture;
        Host&                                    host;
        bool                                     atomic128 = IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128) != FALSE;
        bool                                     failAllocation = false, failRelease = false, failOpen = false, failClose = false, failFlush = false;
        bool                                     failScalar = false, failAfterAllocation = false;
        bool                                     expireAfterOpen = false, expireAfterScalarRead = false, revokeRecoveryAfterScalarRead = false;
        std::size_t                              allocations = 0, releases = 0, codeExchanges = 0, scalarExchanges = 0, unavailableExchange = 0;
        std::vector<std::uintptr_t>              scratch;
        std::array<NativeWeatherOverrideSlot, 8> slots{};
        explicit Platform(Fixture& fixture, Host& host) noexcept : fixture(fixture), host(host) {}
        ~Platform()
        {
            for (auto& slot : slots)
                slot.Release();
        }
        CodeRead Reader() const noexcept override { return &Fixture::Read; }
        bool     Revalidate(const Binding& binding, const NativeWeatherOverrideCodeView& view) noexcept override
        {
            if (!fixture.valid || binding.image != reinterpret_cast<std::uintptr_t>(fixture.image) || view.VerifyWriters(binding) != ClockResult::Ok)
                return false;
            for (const auto& window : A::Windows)
            {
                Bytes bytes{};
                if (!Fixture::Read(binding.image + window.windowRva, bytes.data(), window.windowSize) ||
                    !view.Originalize(binding, window.windowRva, bytes.data(), window.windowSize) || bytes != window.stock)
                    return false;
            }
            return true;
        }
        bool        CompareExchange128() const noexcept override { return atomic128; }
        ClockResult AllocateNear(const Binding& binding, Field field, const P::Range&, std::uintptr_t& output) noexcept override
        {
            Require(!host.held, "scratch allocation precedes suspension");
            ++allocations;
            if (failAllocation)
                return ClockResult::WriteFailure;
            const auto                                       index = static_cast<std::size_t>(field);
            NativeWeatherOverridePlacementProfile::Placement placement;
            auto                                             result = NativeWeatherOverridePlacementProfile::Placement::Prepare(binding, field, placement);
            if (result != ClockResult::Ok)
                return result;
            result = NativeWeatherOverrideSlot::Allocate(placement, slots[index]);
            if (slots[index].HasAllocation())
            {
                output = slots[index].Accumulator();
                scratch.push_back(output);
            }
            return failAfterAllocation ? ClockResult::WriteFailure : result;
        }
        bool ScratchValid(std::uintptr_t address) const noexcept override
        {
            for (const auto& slot : slots)
                if (slot.HasAllocation() && slot.Accumulator() == address)
                    return slot.Valid();
            return false;
        }
        bool ReleaseScratch(std::uintptr_t address) noexcept override
        {
            if (failRelease)
                return false;
            for (auto& slot : slots)
                if (slot.HasAllocation() && slot.Accumulator() == address)
                {
                    if (!slot.Release())
                        return false;
                    for (auto entry = scratch.begin(); entry != scratch.end(); ++entry)
                        if (*entry == address)
                        {
                            scratch.erase(entry);
                            break;
                        }
                    ++releases;
                    return true;
                }
            return false;
        }
        bool OpenCode(const Binding&, std::uintptr_t address, std::size_t size, std::uint32_t& previous) noexcept override
        {
            Require(host.held, "code protection borrows complete peer hold");
            Require(address % size == 0 && (size == 8 || size == 16), "protection covers complete aligned word");
            if (failOpen)
                return false;
            previous = PAGE_READWRITE;
            if (expireAfterOpen)
                host.normal = ClockResult::Expired;
            return true;
        }
        bool CloseCode(const Binding&, std::uintptr_t, std::size_t, std::uint32_t previous) noexcept override
        {
            Require(host.held && previous == PAGE_READWRITE, "restore original fixture protection under same hold");
            return !failClose;
        }
        Exchange ExchangeCode(const Binding&, std::uintptr_t address, std::size_t size, const Bytes& before, const Bytes& after) noexcept override
        {
            Require(host.held && address % size == 0, "only aligned whole-word code exchanges while parked");
            ++codeExchanges;
            if (unavailableExchange == codeExchanges)
                return Exchange::Unavailable;
            const auto result = AtomicWindow(address, size, before, after);
            if (result == Exchange::Exchanged && fixture.failAfterCodeExchange)
                fixture.unreadableCode = true;
            return result;
        }
        bool Flush(std::uintptr_t address, std::size_t size) noexcept override
        {
            Require(host.held, "cache acknowledgement under peer hold");
            return !failFlush && FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(address), size) != FALSE;
        }
        bool ReadScalar(const Binding&, Field field, std::uint32_t& output) noexcept override
        {
            if (fixture.unreadableScalar)
                return false;
            output = fixture.GetBits(field);
            if (expireAfterScalarRead)
                host.normal = ClockResult::Expired;
            if (revokeRecoveryAfterScalarRead)
                host.recover = ClockResult::WrongThread;
            return true;
        }
        Exchange ExchangeScalar(const Binding&, Field field, std::uint32_t expected, std::uint32_t desired) noexcept override
        {
            Require(host.held, "scalar acceptance occurs after writer suppression under peer hold");
            ++scalarExchanges;
            if (failScalar)
                return Exchange::Unavailable;
            const auto address = reinterpret_cast<std::uintptr_t>(fixture.image) + NativeWeatherScalarProfile::Fields[static_cast<std::size_t>(field)];
            const auto actual = static_cast<std::uint32_t>(
                _InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address), static_cast<LONG>(desired), static_cast<LONG>(expected)));
            if (actual != expected)
                return Exchange::Conflict;
            if (fixture.failAfterScalarExchange)
                fixture.unreadableScalar = true;
            if (fixture.foreignScalarAfterExchange)
            {
                fixture.foreignScalarAfterExchange = false;
                fixture.SetBits(field, Bits(0.875f));
            }
            return Exchange::Exchanged;
        }
    };
    struct Scenario
    {
        Fixture                    fixture;
        Host                       host;
        Platform                   platform{fixture, host};
        NativeWeatherOverrideOwner owner{host, platform};
        Scenario() { Require(owner.Bind(fixture.Binding(), host.generation) == ClockResult::Ok, "owned controlled binding admitted"); }
        void Clean()
        {
            Require(owner.RestoreAll() == ClockResult::Ok, "all override debt retires");
            Require(!owner.HasReferences() && !owner.NeedsRecovery() && platform.scratch.empty(), "no scratch, code or journal debt after cleanup");
            Require(!host.held && host.safe && !owner.UnsafeCode(), "ordinary continuation never receives torn code or permanent suspension");
        }
    };
    void CapabilitiesAndIntent()
    {
        Scenario   s;
        const auto allocations = s.platform.allocations;
        for (const auto field : {Field::Rain})
        {
            Require(!s.owner.Admits(field), "unproved whole-group atomic capability refused");
            Require(s.owner.Set(field, 0.5f) == ClockResult::UnsupportedBuild, "unsupported setter is explicit");
            Require(s.owner.Reset(field) == ClockResult::UnsupportedBuild, "unsupported reset is explicit");
        }
        Require(s.platform.allocations == allocations && s.host.parks == 0, "refusals acquire no code or suspension ownership");
        s.platform.atomic128 = false;
        for (const auto field : {Field::WetRoads, Field::Foggyness, Field::Fog, Field::RainFog, Field::WaterFog, Field::Sandstorm, Field::Rainbow})
            Require(s.owner.Admits(field), "every complete group now admits constrained aligned64 independently of128 support");
        Require(s.owner.Set(Field::Fog, (std::numeric_limits<float>::quiet_NaN)()) == ClockResult::InvalidArgument, "NaN refuses before native ownership");
        Require(s.owner.Set(Field::Fog, (std::numeric_limits<float>::infinity)()) == ClockResult::InvalidArgument, "infinity refuses");
        Require(s.owner.Set(static_cast<Field>(255), 0) == ClockResult::InvalidArgument, "invalid enum refuses");
        for (const float value : {-0.0f, -1.25f, 2.5f, 0.625f})
        {
            const auto allocated = s.platform.allocations;
            Require(s.owner.Set(Field::Fog, value) == ClockResult::Ok, "actual persistent Fog scalar accepted");
            Require(s.fixture.GetBits(Field::Fog) == Bits(value), "finite scalar bits preserved without invented clamp");
            Require(s.owner.HasReferences() && !s.owner.NeedsRecovery(), "successful intent retains valid writers but no recovery debt");
            Require(s.owner.CodeView().VerifyWriters(s.fixture.Binding()) == ClockResult::Ok, "actual same-generation writer code view qualifies");
            Require(!s.fixture.Stock(Field::Fog), "all Fog writer instructions redirected");
            for (const auto& writer : P::Writers)
                if (writer.field == Field::Fog)
                {
                    const auto target = s.fixture.Target(writer);
                    Require(s.platform.ScratchValid(target), "actual RIP target decodes to this owner's near accumulator");
                    const auto derivedWrite = Bits(0.9375f);
                    std::memcpy(reinterpret_cast<void*>(target), &derivedWrite, 4);
                    Require(s.fixture.GetBits(Field::Fog) == Bits(value), "native producer's redirected target does not overwrite accepted scalar");
                }
            if (allocated)
                Require(s.platform.allocations == allocated, "repeated Set reuses retained plan and allocation");
        }
        const auto accepted = s.fixture.GetBits(Field::Fog);
        Require(s.owner.Reset(Field::Fog) == ClockResult::Ok, "Reset restores both exact native writer operands");
        Require(s.fixture.Stock(Field::Fog) && s.fixture.GetBits(Field::Fog) == accepted, "Reset restores writers without rewinding successful scalar intent");
        s.Clean();
    }
    void IndependentGroups()
    {
        Scenario s;
        Require(s.owner.Set(Field::Fog, 0.2f) == ClockResult::Ok, "Fog admitted");
        Require(s.owner.Set(Field::Rainbow, 0.7f) == ClockResult::Ok, "Rainbow admitted independently");
        const bool sandstorm = true;
        if (sandstorm)
            Require(s.owner.Set(Field::Sandstorm, 0.9f) == ClockResult::Ok, "constrained aligned64 Sandstorm group admitted as a whole");
        Require(s.owner.Bind(s.fixture.Binding(), 38) == ClockResult::Busy, "binding replacement cannot orphan native references");
        Require(s.owner.Reset(Field::Fog) == ClockResult::Ok && s.fixture.Stock(Field::Fog), "independent Fog Reset");
        Require(!s.fixture.Stock(Field::Rainbow), "other group's suppression survives Fog Reset");
        Require(s.owner.CodeView().VerifyWriters(s.fixture.Binding()) == ClockResult::Ok, "mixed remaining groups have exact retained masks");
        s.Clean();
        Require(s.fixture.GetBits(Field::Fog) == Bits(0.2f) && s.fixture.GetBits(Field::Rainbow) == Bits(0.7f), "End preserves accepted native scalar values");
        if (sandstorm)
            Require(s.fixture.Stock(Field::Sandstorm) && s.fixture.GetBits(Field::Sandstorm) == Bits(0.9f),
                    "Sandstorm End restores all three windows without scalar rewind");
    }
    void EveryGroupPersistentLifecycle()
    {
        Scenario s;
        s.platform.atomic128 = false;
        std::array<std::uintptr_t, 8> allocations{};
        for (const auto field : {Field::WetRoads, Field::Foggyness, Field::Fog, Field::RainFog, Field::WaterFog, Field::Sandstorm, Field::Rainbow})
        {
            const auto index = static_cast<std::size_t>(field);
            Require(s.owner.Set(field, 0.625f) == ClockResult::Ok, "each complete native group accepts persistent intent with64-bit CAS only");
            allocations[index] = s.platform.slots[index].AllocationIdentity();
            Require(s.platform.slots[index].Accumulator() != allocations[index], "slot identity is distinct from interior RIP target");
            Require(s.owner.Set(field, -0.0f) == ClockResult::Ok && s.platform.slots[index].AllocationIdentity() == allocations[index],
                    "repeated Set reuses immutable constrained allocation for this group");
            for (const auto& writer : P::Writers)
                if (writer.field == field)
                    Require(s.fixture.Target(writer) == s.platform.slots[index].Accumulator(), "every actual producer decodes to exact retained interior slot");
            Require(s.fixture.GetBits(field) == Bits(-0.0f), "native accepted bits are preserved for every group");
        }
        Require(s.platform.scratch.size() == 7 && s.owner.CodeView().VerifyWriters(s.fixture.Binding()) == ClockResult::Ok,
                "all seven private allocations and30 exact native writer masks coexist");
        for (const auto field : {Field::WetRoads, Field::RainFog, Field::Foggyness, Field::WaterFog, Field::Sandstorm, Field::Fog, Field::Rainbow})
        {
            Require(s.owner.Reset(field) == ClockResult::Ok && s.fixture.Stock(field), "independent exact group reset restores full original instructions");
            Require(s.fixture.GetBits(field) == Bits(-0.0f), "independent Reset leaves accepted scalar intent unchanged");
            Require(s.owner.CodeView().VerifyWriters(s.fixture.Binding()) == ClockResult::Ok, "remaining constrained groups retain exact lifecycle masks");
        }
        s.Clean();
    }
    void RetainAllocationConfirmationFailure()
    {
        Scenario s;
        s.platform.failAfterAllocation = true;
        Require(s.owner.Set(Field::WaterFog, 0.5f) == ClockResult::WriteFailure, "post-allocation failure is an explicit refused mutation");
        Require(s.owner.HasReferences() && s.owner.NeedsRecovery() && s.platform.scratch.size() == 1,
                "interior pointer is captured even when allocator cannot publish success");
        Require(s.platform.codeExchanges == 0 && s.fixture.Stock(Field::WaterFog), "unconfirmed allocation grants no native code mutation");
        s.platform.failAfterAllocation = false;
        s.Clean();
        Require(s.platform.releases == 1, "same recovery owner releases true allocation base after failed confirmation");
    }
    void PartialCodeRollback()
    {
        for (const auto field : {Field::WetRoads, Field::Foggyness, Field::Fog, Field::RainFog, Field::WaterFog, Field::Sandstorm, Field::Rainbow})
        {
            Scenario capability;
            if (!capability.owner.Admits(field))
                continue;
            for (std::size_t failure = 1; failure <= P::WriterCounts[static_cast<std::size_t>(field)]; ++failure)
            {
                Scenario s;
                s.platform.unavailableExchange = failure;
                Require(s.owner.Set(field, 0.5f) != ClockResult::Ok, "each injected atomic no-write failure is refused");
                Require(s.fixture.Stock(field), "narrow rollback restores every earlier successful operand");
                Require(s.fixture.GetBits(field) == Bits(0.125f), "scalar untouched when producer suppression did not finish");
                Require(!s.owner.HasReferences() && s.platform.scratch.empty(), "fully confirmed rollback retires its allocation");
                s.Clean();
            }
        }
    }
    void RetainedCodeReadDebt()
    {
        Scenario s;
        s.fixture.failAfterCodeExchange = true;
        Require(s.owner.Set(Field::Fog, 0.5f) != ClockResult::Ok, "readback failure never becomes setter success");
        Require(s.owner.HasReferences() && s.owner.NeedsRecovery(), "readback debt retains code plan and accumulator");
        Require(s.host.safe && !s.host.held && !s.owner.UnsafeCode(), "indivisible code permits ordinary return despite unreadable acknowledgement");
        Require(s.owner.Set(Field::Rainbow, 0.4f) == ClockResult::Busy, "new mutations refuse while recovery pending");
        const auto before = s.platform.codeExchanges;
        ++s.host.generation;
        Require(s.owner.Recover() == ClockResult::OwnerConflict && s.platform.codeExchanges == before, "stale generation cannot restore retained code");
        --s.host.generation;
        s.fixture.failAfterCodeExchange = false;
        s.fixture.unreadableCode = false;
        s.Clean();
        Require(s.fixture.Stock(Field::Fog), "later qualified callback restores unreadable atomic outcome exactly");
    }
    void RetainedAcknowledgeDebt()
    {
        for (int fault = 0; fault < 3; ++fault)
        {
            Scenario s;
            s.platform.failFlush = fault == 0;
            s.platform.failClose = fault == 1;
            s.platform.failRelease = fault == 2;
            if (fault == 2)
            {
                Require(s.owner.Set(Field::Fog, 0.5f) == ClockResult::Ok, "successful Set before allocation release failure");
                Require(s.owner.Reset(Field::Fog) == ClockResult::RestoreFailure, "failed Free retains owned allocation");
            }
            else
                Require(s.owner.Set(Field::Fog, 0.5f) != ClockResult::Ok, "flush/protection acknowledgement failure refuses success");
            Require(s.owner.HasReferences() && s.owner.NeedsRecovery(), "each cleanup failure retains actual debt");
            Require(!s.host.held && s.host.safe && !s.owner.UnsafeCode(), "no partial operand or permanent suspension from acknowledgement debt");
            s.platform.failFlush = s.platform.failClose = s.platform.failRelease = false;
            s.Clean();
        }
    }
    void ScalarJournal()
    {
        {
            Scenario s;
            s.fixture.failAfterScalarExchange = true;
            Require(s.owner.Set(Field::Fog, 0.5f) != ClockResult::Ok, "scalar readback failure retains transactional write");
            Require(s.owner.HasReferences() && s.owner.NeedsRecovery(), "partial scalar journal retained");
            Require(s.fixture.GetBits(Field::Fog) == Bits(0.5f), "failed transaction's unconfirmed scalar remains explicit debt");
            s.fixture.failAfterScalarExchange = false;
            s.fixture.unreadableScalar = false;
            s.Clean();
            Require(s.fixture.GetBits(Field::Fog) == Bits(0.125f), "only failed transaction rolls back captured scalar");
        }
        {
            Scenario s;
            s.fixture.foreignScalarAfterExchange = true;
            Require(s.owner.Set(Field::Fog, 0.5f) != ClockResult::Ok, "later third scalar value cannot become success");
            Require(s.fixture.GetBits(Field::Fog) == Bits(0.875f), "foreign scalar edit preserved during partial rollback");
            Require(s.owner.SupersededCount() == 1, "superseded scalar debt reported honestly");
            Require(!s.owner.HasReferences() && !s.owner.NeedsRecovery(), "superseded scalar retires without pinning restored code forever");
            s.Clean();
            Require(s.owner.SupersededCount() == 1, "supersession receipt survives cleanup for this generation");
        }
    }
    void ForeignCodeAndScope()
    {
        Scenario s;
        Require(s.owner.Set(Field::Rainbow, 0.6f) == ClockResult::Ok, "initial Rainbow intent");
        const auto* window = A::Find(0x1291e7e);
        Bytes       original{};
        std::memcpy(original.data(), s.fixture.image + window->windowRva, window->windowSize);
        s.fixture.image[window->windowRva + window->windowSize - 1] ^= 1;  // Independent foreign neighbor edit.
        const auto before = s.platform.codeExchanges;
        Require(s.owner.Reset(Field::Rainbow) == ClockResult::BindingChanged, "foreign neighbor bytes cannot become an atomic patch baseline");
        Require(s.platform.codeExchanges == before && s.owner.HasReferences(), "foreign code preserved with native reference lifetime retained");
        Require(s.owner.Recover() == ClockResult::BindingChanged && s.owner.NeedsRecovery(), "unknown code remains explicit same-owner debt");
        Require(s.fixture.image[window->windowRva + window->windowSize - 1] == static_cast<unsigned char>(original[window->windowSize - 1] ^ 1),
                "recovery does not overwrite foreign code");
        std::memcpy(s.fixture.image + window->windowRva, original.data(), window->windowSize);
        s.host.recover = ClockResult::WrongThread;
        Require(s.owner.Recover() == ClockResult::WrongThread && s.platform.codeExchanges == before, "foreign callback cannot recover now-valid code");
        s.host.recover = ClockResult::Ok;
        s.Clean();
    }
    void PeerAndAdmissionFailures()
    {
        {
            Scenario s;
            s.host.normal = ClockResult::Expired;
            Require(s.owner.Set(Field::Fog, 0.5f) == ClockResult::Expired && s.platform.allocations == 0, "expired ordinary scope acquires nothing");
            s.host.normal = ClockResult::Ok;
            s.host.park = ClockResult::Busy;
            Require(s.owner.Set(Field::Fog, 0.5f) == ClockResult::Busy, "partial park failure refuses mutation");
            Require(!s.host.held && s.fixture.Stock(Field::Fog) && s.owner.NeedsRecovery(), "partial hold released while unused allocation debt retained");
            s.host.park = ClockResult::Ok;
            s.Clean();
        }
        {
            Scenario s;
            s.host.failFinish = true;
            Require(s.owner.Set(Field::Fog, 0.5f) == ClockResult::RestoreFailure, "failed OS peer resume is not claimed success");
            Require(s.host.held && s.owner.HasReferences() && s.owner.NeedsRecovery(), "existing host retains peer handles and owner resources");
            s.host.failFinish = false;
            Require(s.host.FinishPeers(true), "host's qualified retry resumes retained peers");
            s.Clean();
        }
    }
    void ImmediateMutationAuthority()
    {
        {
            Scenario s;
            s.platform.expireAfterOpen = true;
            Require(s.owner.Set(Field::Fog, 0.5f) == ClockResult::Expired, "grant revoked during protection helper refuses code CAS");
            Require(s.platform.codeExchanges == 0 && s.platform.scalarExchanges == 0, "no cached authority survives code protection bookend");
            Require(s.fixture.Stock(Field::Fog) && !s.owner.HasReferences(), "unmodified code and temporary protection debt retire exactly");
            s.platform.expireAfterOpen = false;
            s.host.normal = ClockResult::Ok;
            s.Clean();
        }
        {
            Scenario s;
            s.platform.expireAfterScalarRead = true;
            Require(s.owner.Set(Field::Fog, 0.5f) == ClockResult::Expired, "grant revoked by capture read refuses scalar CAS");
            Require(s.platform.scalarExchanges == 0 && s.fixture.GetBits(Field::Fog) == Bits(0.125f), "captured value does not grant delayed scalar write");
            Require(s.fixture.Stock(Field::Fog) && !s.owner.HasReferences(), "narrow rollback after expiry removes only prepared writer suppression");
            s.platform.expireAfterScalarRead = false;
            s.host.normal = ClockResult::Ok;
            s.Clean();
        }
        {
            Scenario s;
            s.fixture.failAfterScalarExchange = true;
            Require(s.owner.Set(Field::Fog, 0.5f) != ClockResult::Ok, "prepare recoverable scalar rollback journal");
            s.fixture.failAfterScalarExchange = false;
            s.fixture.unreadableScalar = false;
            s.platform.revokeRecoveryAfterScalarRead = true;
            const auto exchanges = s.platform.scalarExchanges;
            Require(s.owner.Recover() == ClockResult::WrongThread && s.owner.NeedsRecovery(), "recovery grant revoked during observation retains debt");
            Require(s.platform.scalarExchanges == exchanges && s.fixture.GetBits(Field::Fog) == Bits(0.5f),
                    "rollback does not reuse earlier recovery authority");
            s.platform.revokeRecoveryAfterScalarRead = false;
            s.host.recover = ClockResult::Ok;
            s.Clean();
            Require(s.fixture.GetBits(Field::Fog) == Bits(0.125f), "next true recovery grant restores only failed scalar transaction");
        }
    }
}
int main()
{
    static_assert(sizeof(void*) == 8);
    CapabilitiesAndIntent();
    IndependentGroups();
    EveryGroupPersistentLifecycle();
    RetainAllocationConfirmationFailure();
    PartialCodeRollback();
    RetainedCodeReadDebt();
    RetainedAcknowledgeDebt();
    ScalarJournal();
    ForeignCodeAndScope();
    PeerAndAdmissionFailures();
    ImmediateMutationAuthority();
    std::printf("Passed %zu native weather override owner checks (64-bit; owned-memory)\n", checks);
    return 0;
}
