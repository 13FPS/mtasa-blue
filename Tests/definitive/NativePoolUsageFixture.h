/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise actual native allocation census without native pool calls
 ****************************************************************************/
#pragma once
#include "NativeWorldIplFixture.h"
#include "../../Client/game_de/CPoolsUsageDE.h"
#include "../../Client/game_de/NativePoolUsageMemory.h"
#include "../../Client/game_de/NativePedPoolProfile.h"
#include "../../Client/game_de/NativeTaskPoolProfile.h"
#include "../../Client/game_de/NativeWorldAddProfile.h"
#include "../../Client/game_de/NativeBuildingPoolDefaultProfile.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace NativePoolUsageFixture
{
    namespace M = mta::game_de::NativePoolUsageMemory;
    namespace W = NativeWorldIplFixture;
    namespace E = mta::game_de::NativeWorldIplLoadedProfile;
    namespace P = mta::game_de::NativePedPoolProfile;
    namespace T = mta::game_de::NativeTaskPoolProfile;
    namespace N = mta::game_de::NativeWorldAddProfile;
    using Kind = mta::game_de::NativePoolUsageAccess::Kind;
    using Value = mta::game_de::NativePoolUsageAccess::Value;
    using mta::game_de::ClockResult;
    inline constexpr std::array<Kind, 7>           Kinds = {Kind::Building,         Kind::Ped, Kind::Object, Kind::Dummy, Kind::Task, Kind::PointerDoubleLink,
                                                            Kind::PointerSingleLink};
    inline constexpr std::array<ePools, 7>         SdkKinds = {BUILDING_POOL,           PED_POOL, OBJECT_POOL, DUMMY_POOL, TASK_POOL, POINTER_DOUBLE_LINK_POOL,
                                                               POINTER_SINGLE_LINK_POOL};
    inline constexpr std::array<std::uintptr_t, 7> Roots = {E::BuildingRoot, P::RootRva,       E::ObjectRoot,   E::DummyRoot,
                                                            T::RootRva,      N::DoubleRootRva, N::SingleRootRva};

    struct State
    {
        W::Pool buildings{E::BuildingCapacity, E::BuildingBytes}, peds{P::NativeCapacity, P::ObjectStride}, objects{E::ObjectCapacity, E::ObjectBytes},
            dummies{E::DummyCapacity, E::DummyBytes}, tasks{T::NativeCapacity, T::ObjectStride}, doubles{N::DoubleCapacity, N::DoubleBytes},
            singles{N::SingleCapacity, N::SingleBytes};
        std::array<W::Pool*, 7> pools{&buildings, &peds, &objects, &dummies, &tasks, &doubles, &singles};
        std::uintptr_t          image;
        W::Writer               writer;
        void*                   host;
        State(std::uintptr_t image, W::Writer writer, void* host) : image(image), writer(writer), host(host) { Reset(); }
        bool Ready() const noexcept
        {
            return std::all_of(pools.begin(), pools.end(), [](const W::Pool* pool) { return pool->Ready(); });
        }
        template <class V>
        void Global(std::uintptr_t rva, const V& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        void Reset()
        {
            if (!Ready())
                return;
            const auto code = [&](const auto& pins)
            {
                for (const auto& pin : pins)
                    writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            };
            code(mta::game_de::NativeBuildingPoolDefaultProfile::Pins);
            code(P::Code);
            code(T::Code);
            code(mta::game_de::NativeWorldIplPoolProfile::Code);
            for (const auto& pin : mta::game_de::NativeWorldCoverProfile::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            for (const auto& pin : N::Code)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            writer(host, 0x117161f, E::ObjectPoolBytes.data(), E::ObjectPoolBytes.size(), PAGE_EXECUTE_READ);
            writer(host, 0x12128a0, E::ObjectAllocateBytes.data(), E::ObjectAllocateBytes.size(), PAGE_EXECUTE_READ);
            writer(host, 0x11716ef, E::DummyPoolBytes.data(), E::DummyPoolBytes.size(), PAGE_EXECUTE_READ);
            writer(host, 0x112e820, E::DummyAllocateBytes.data(), E::DummyAllocateBytes.size(), PAGE_EXECUTE_READ);
            for (std::size_t i = 0; i != pools.size(); ++i)
            {
                pools[i]->Reset();
                Global(Roots[i], pools[i]->Address());
            }
        }
    };

    // These wrappers retain the real Runtime's admission callback and change
    // only fixture-owned bytes at its last observed bookend. A fake successful
    // native owner would fail to exercise the final raw equality fence.
    struct Bookends
    {
        M::CheckOwner                        owner = nullptr, after = nullptr;
        unsigned                             calls = 0, mutateAt = 0;
        std::function<void()>                mutation;
        inline static thread_local Bookends* current = nullptr;
        static ClockResult                   Invoke(M::CheckOwner callback) noexcept
        {
            auto&      self = *current;
            const auto result = callback ? callback() : ClockResult::NotInitialized;
            ++self.calls;
            if (result == ClockResult::Ok && self.mutateAt == self.calls && self.mutation)
                self.mutation();
            return result;
        }
        static ClockResult Owner() noexcept { return Invoke(current->owner); }
        static ClockResult After() noexcept { return Invoke(current->after); }
    };

    template <class Pools, class NativeAccess, class Check>
    void Run(std::uintptr_t image, W::Writer writer, void* host, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner afterApplication,
             NativeAccess& native, Pools& sdk, Check check)
    {
        State state(image, writer, host);
        check(state.Ready(), "seven genuine owned native pool allocations exist");
        const auto unchanged = [&](std::size_t selected, const char* reason)
        {
            Value      output{0xabcde, 0xfedcb};
            const auto result = M::Read(image, generation, Kinds[selected], owner, afterApplication, output);
            check(result != ClockResult::Ok && output.capacity == 0xabcde && output.used == 0xfedcb, reason);
        };
        for (std::size_t selected = 0; selected != Kinds.size(); ++selected)
        {
            auto&      pool = *state.pools[selected];
            const auto capacity = static_cast<std::uint32_t>(pool.capacity);
            Value      value{0xabcde, 0xfedcb};
            check(
                M::Read(image, generation, Kinds[selected], owner, afterApplication, value) == ClockResult::Ok && value.capacity == capacity && value.used == 0,
                "each exact native pool reports empty occupancy with its own admitted allocation capacity");
            for (std::uint32_t i = 0; i != capacity; ++i)
                pool.flags[i] = static_cast<unsigned char>(0x80 | (i & 0x7f));
            check(M::Read(image, generation, Kinds[selected], owner, afterApplication, value) == ClockResult::Ok && value.used == 0,
                  "all seven low generation bits of a free native slot leave occupancy unchanged");
            for (std::uint32_t i = 0; i != capacity; ++i)
                pool.flags[i] &= 0x7f;
            check(M::Read(image, generation, Kinds[selected], owner, afterApplication, value) == ClockResult::Ok && value.used == capacity,
                  "all occupied allocation flags count even when their opaque object rows contain no qualified entity vtable");
            std::uint32_t expected = 0;
            for (std::uint32_t i = 0; i != capacity; ++i)
            {
                const bool occupied = i % 3 == 1 || i == 0 || i + 1 == capacity || i == 255 || i == 256 || i == 511 || i == 512;
                pool.flags[i] = static_cast<unsigned char>((i & 0x7f) | (occupied ? 0 : 0x80));
                expected += occupied;
            }
            const std::vector<unsigned char> before(pool.flags, pool.flags + capacity);
            check(M::Read(image, generation, Kinds[selected], owner, afterApplication, value) == ClockResult::Ok && value.capacity == capacity &&
                      value.used == expected,
                  "a complete mixed census includes both terminal slots and allocation-flag chunk boundaries");
            check(std::equal(before.begin(), before.end(), pool.flags), "the production allocation census performs no flag writes");
            Value forwarded;
            check(native.ReadPoolUsage(Kinds[selected], forwarded) == ClockResult::Ok && forwarded.capacity == capacity && forwarded.used == expected,
                  "the same actual Runtime owner forwards every native pool kind without replacing native occupancy with SDK registration count");
            check(sdk.ReadPoolUsage(SdkKinds[selected], forwarded) == ClockResult::Ok && forwarded.capacity == capacity && forwarded.used == expected &&
                      sdk.GetPoolCapacity(SdkKinds[selected]) == static_cast<int>(capacity) &&
                      sdk.GetNumberOfUsedSpaces(SdkKinds[selected]) == static_cast<int>(expected),
                  "both actual CPools SDK virtuals and the status path consume the same owner-bound native allocation census");
            if (Kinds[selected] == Kind::Ped)
                check(sdk.GetPedCount() == 0 && expected > 0, "native ped used spaces remain independent of the CPU borrowed-wrapper registration count");

            const auto address = pool.Address();
            const auto originalHeader = pool.header;
            state.Global(Roots[selected], std::uintptr_t{0});
            unchanged(selected, "missing native allocation root refuses without publishing zero capacity");
            state.Global(Roots[selected], address);
            for (const std::int32_t bad : {-1, 0, pool.capacity - 1, pool.capacity + 1})
            {
                W::Put(address, 0x10, bad);
                unchanged(selected, "capacity must match the existing exact native constructor geometry");
            }
            pool.header = originalHeader;
            for (const std::int32_t bad : {-2, pool.capacity})
            {
                W::Put(address, 0x14, bad);
                unchanged(selected, "native allocation cursor must remain within its real signed bounded representation");
            }
            pool.header = originalHeader;
            pool.header[0x18] = 0;
            unchanged(selected, "a non-owning header is not an admitted native startup allocation");
            pool.header[0x18] = 2;
            unchanged(selected, "malformed native allocation ownership byte refuses");
            pool.header = originalHeader;
            W::Put(address, 8, std::uintptr_t{0});
            unchanged(selected, "null flags cannot become a successful zero occupancy");
            W::Put(address, 8, reinterpret_cast<std::uintptr_t>(pool.objects));
            unchanged(selected, "object and flag allocation ranges must remain disjoint");
            W::Put(address, 8, address);
            unchanged(selected, "flags cannot alias their own native pool header");
            W::Put(address, 8, (std::numeric_limits<std::uintptr_t>::max)() - capacity / 2);
            unchanged(selected, "overflowed flags extent refuses before native memory consumption");
            pool.header = originalHeader;
            W::Put(address, 0, address);
            unchanged(selected, "object storage cannot alias its native allocation header");
            pool.header = originalHeader;
            DWORD      previous = 0;
            const bool guarded = VirtualProtect(pool.flags + capacity - 1, 1, PAGE_NOACCESS, &previous) != 0;
            check(guarded, "the owned terminal flag page can be made unreadable");
            if (guarded)
            {
                unchanged(selected, "an unreadable terminal allocation flag refuses the entire census");
                DWORD ignored = 0;
                check(VirtualProtect(pool.flags + capacity - 1, 1, previous, &ignored) != 0, "the owned terminal allocation page is restored");
            }

            Bookends bookends;
            bookends.owner = owner;
            bookends.after = afterApplication;
            Bookends::current = &bookends;
            check(M::Read(image, generation, Kinds[selected], &Bookends::Owner, &Bookends::After, value) == ClockResult::Ok,
                  "counting wrappers preserve the real current Runtime owner callbacks");
            const auto last = bookends.calls;
            check(last >= 4, "a complete census has both initial and final native owner bookends");
            const auto moving = [&](std::function<void()> mutation, std::function<void()> restore, const char* reason)
            {
                bookends.calls = 0;
                bookends.mutateAt = last;
                bookends.mutation = std::move(mutation);
                Value      output{0xabcde, 0xfedcb};
                const auto result = M::Read(image, generation, Kinds[selected], &Bookends::Owner, &Bookends::After, output);
                check(result != ClockResult::Ok && output.capacity == 0xabcde && output.used == 0xfedcb, reason);
                restore();
                bookends.mutateAt = 0;
                bookends.mutation = {};
            };
            moving([&] { state.Global(Roots[selected], std::uintptr_t{0}); }, [&] { state.Global(Roots[selected], address); },
                   "a root changed by the final real owner callback cannot escape the callback-free raw equality fence");
            moving([&] { W::Put(address, 0x14, pool.capacity - 1); }, [&] { pool.header = originalHeader; },
                   "a native pool header changed at the final owner bookend refuses publication");
            moving([&] { pool.header[31] ^= 1; }, [&] { pool.header[31] ^= 1; },
                   "the complete native allocation header remains sealed even where its final padding byte has no interpreted count meaning");
            moving([&] { pool.flags[capacity - 1] ^= 1; }, [&] { pool.flags[capacity - 1] ^= 1; },
                   "even occupancy-preserving generation drift in the final flag is sealed and refused");
            moving([&] { std::swap(pool.flags[0], pool.flags[2]); }, [&] { std::swap(pool.flags[0], pool.flags[2]); },
                   "swapping an occupied and free slot at the final bookend refuses even when the aggregate used-space count is unchanged");
            if (selected == 0)
            {
                bookends.calls = 0;
                bookends.mutateAt = last;
                bookends.mutation = [&]
                {
                    Value nested{0xabcde, 0xfedcb};
                    check(M::Read(image, generation, Kinds[selected], owner, afterApplication, nested) == ClockResult::Busy && nested.capacity == 0xabcde &&
                              nested.used == 0xfedcb,
                          "callback reentry cannot consume another census while the outer native source borrow holds the existing gate");
                };
                check(M::Read(image, generation, Kinds[selected], &Bookends::Owner, &Bookends::After, value) == ClockResult::Ok && value.used == expected,
                      "refused callback reentry preserves the outer complete native census");
                bookends.mutateAt = 0;
                bookends.mutation = {};
            }
            Bookends::current = nullptr;
            check(M::Read(image, generation, Kinds[selected], owner, afterApplication, value) == ClockResult::Ok && value.used == expected,
                  "restored exact native pool bytes remain admissible after independent malformed and moving cases");
            const std::array<std::uintptr_t, 7> initializer = {
                mta::game_de::NativeBuildingPoolDefaultProfile::Pins[0].rva, 0x11713c0, 0x117161f, 0x11716ef, 0x11718a0, 0x1171202, 0x1171130};
            unsigned char original = 0;
            std::memcpy(&original, reinterpret_cast<const void*>(image + initializer[selected]), sizeof(original));
            const auto corrupt = static_cast<unsigned char>(original ^ 1);
            writer(host, initializer[selected], &corrupt, sizeof(corrupt), PAGE_EXECUTE_READ);
            unchanged(selected, "each selected native constructor publication must retain its exact source-qualified code profile");
            writer(host, initializer[selected], &original, sizeof(original), PAGE_EXECUTE_READ);
            const std::array<std::uintptr_t, 7> allocator = {0x1036840, 0x121a860, 0x12128a0, 0x112e820, 0x12d2e20, 0x1173820, 0x1131ff0};
            std::memcpy(&original, reinterpret_cast<const void*>(image + allocator[selected]), sizeof(original));
            const auto corruptAllocator = static_cast<unsigned char>(original ^ 1);
            writer(host, allocator[selected], &corruptAllocator, sizeof(corruptAllocator), PAGE_EXECUTE_READ);
            unchanged(selected, "each selected native flag allocator must retain its exact source-qualified code profile");
            writer(host, allocator[selected], &original, sizeof(original), PAGE_EXECUTE_READ);
        }
        Value output{0xabcde, 0xfedcb};
        check(M::Read(image, 0, Kind::Building, owner, afterApplication, output) != ClockResult::Ok && output.capacity == 0xabcde && output.used == 0xfedcb,
              "an absent Runtime generation cannot publish native allocation capacity");
        check(M::Read(image, generation, Kind::Building, nullptr, afterApplication, output) != ClockResult::Ok && output.capacity == 0xabcde &&
                  output.used == 0xfedcb,
              "a missing owner callback preserves both caller output fields");
        check(M::Read(image, generation, Kind::Building, owner, nullptr, output) != ClockResult::Ok && output.capacity == 0xabcde && output.used == 0xfedcb,
              "a missing post-application owner callback does not authorize a native census");
        check(M::Read(image, generation, static_cast<Kind>(255), owner, afterApplication, output) == ClockResult::InvalidArgument &&
                  output.capacity == 0xabcde && output.used == 0xfedcb,
              "unsupported native pool kinds refuse without coercion to another pool");
        for (const auto unsupported :
             {VEHICLE_POOL, COL_MODEL_POOL, EVENT_POOL, TASK_ALLOCATOR_POOL, PED_INTELLIGENCE_POOL, PED_ATTRACTOR_POOL, ENTRY_INFO_NODE_POOL, NODE_ROUTE_POOL,
              PATROL_ROUTE_POOL, POINT_ROUTE_POOL, ENV_MAP_MATERIAL_POOL, ENV_MAP_ATOMIC_POOL, SPEC_MAP_MATERIAL_POOL, MAX_POOLS, static_cast<ePools>(-1)})
        {
            output = {0xabcde, 0xfedcb};
            check(sdk.ReadPoolUsage(unsupported, output) == ClockResult::InvalidArgument && output.capacity == 0xabcde && output.used == 0xfedcb,
                  "unqualified SDK pool ids preserve explicit absence of native allocation authority");
            bool refusedCapacity = false, refusedUsed = false;
            try
            {
                (void)sdk.GetPoolCapacity(unsupported);
            }
            catch (const std::runtime_error&)
            {
                refusedCapacity = true;
            }
            try
            {
                (void)sdk.GetNumberOfUsedSpaces(unsupported);
            }
            catch (const std::runtime_error&)
            {
                refusedUsed = true;
            }
            check(refusedCapacity && refusedUsed, "unsupported CPools virtuals throw instead of fabricating an empty native pool");
        }
        std::puts("Production native pool usage census completed");
    }
}
