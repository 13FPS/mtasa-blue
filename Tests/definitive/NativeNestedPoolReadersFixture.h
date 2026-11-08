/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise nested SDK readers while their unqualified effects stay pure
 ****************************************************************************/
#pragma once
#include "NativePoolUsageFixture.h"
#include "../../Client/game_de/CBuildingsPoolUsageDE.h"
#include "../../Client/game_de/CPtrNodeSingleLinkPoolUsageDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace NativeNestedPoolReadersFixture
{
    namespace U = NativePoolUsageFixture;
    namespace M = mta::game_de::NativePoolUsageMemory;
    using mta::game_de::CBuildingsPoolUsageDE;
    using mta::game_de::ClockResult;
    using mta::game_de::CPoolsUsageDE;
    using mta::game_de::CPtrNodeSingleLinkPoolUsageDE;
    using mta::game_de::NativePoolUsageAccess;
    using Value = NativePoolUsageAccess::Value;
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::abort();
    }

    // Only the existing read methods are inherited. Any unexpected allocation,
    // resize, reset or backup dispatch terminates this test rather than making
    // a read-only nested receiver appear to provide those native effects.
    class Buildings final : public CBuildingsPoolUsageDE
    {
    public:
        explicit Buildings(CPoolsUsageDE& pools) noexcept : CBuildingsPoolUsageDE(pools) {}
        CBuilding* AddBuilding(CClientBuilding*, std::uint16_t, CVector*, CVector*, std::uint8_t) override { Unqualified(); }
        void       RemoveBuilding(CBuilding*) override { Unqualified(); }
        bool       Resize(int) override { Unqualified(); }
        void       RemoveAllWithBackup() override { Unqualified(); }
        void       RestoreBackup() override { Unqualified(); }
    };
    class Singles final : public CPtrNodeSingleLinkPoolUsageDE
    {
    public:
        explicit Singles(CPoolsUsageDE& pools) noexcept : CPtrNodeSingleLinkPoolUsageDE(pools) {}
        bool Resize(std::size_t) override { Unqualified(); }
        void ResetCapacity() override { Unqualified(); }
    };
    static_assert(!std::is_abstract_v<Buildings> && !std::is_abstract_v<Singles>);
    static_assert(std::is_convertible_v<Buildings*, CBuildingsPool*> && std::is_convertible_v<Singles*, CPtrNodeSingleLinkPool*>);

    class MissingBuildingAdd : public CBuildingsPoolUsageDE
    {
        void RemoveBuilding(CBuilding*) override;
        bool Resize(int) override;
        void RemoveAllWithBackup() override;
        void RestoreBackup() override;
    };
    class MissingBuildingRemove : public CBuildingsPoolUsageDE
    {
        CBuilding* AddBuilding(CClientBuilding*, std::uint16_t, CVector*, CVector*, std::uint8_t) override;
        bool       Resize(int) override;
        void       RemoveAllWithBackup() override;
        void       RestoreBackup() override;
    };
    class MissingBuildingResize : public CBuildingsPoolUsageDE
    {
        CBuilding* AddBuilding(CClientBuilding*, std::uint16_t, CVector*, CVector*, std::uint8_t) override;
        void       RemoveBuilding(CBuilding*) override;
        void       RemoveAllWithBackup() override;
        void       RestoreBackup() override;
    };
    class MissingBuildingRemoveBackup : public CBuildingsPoolUsageDE
    {
        CBuilding* AddBuilding(CClientBuilding*, std::uint16_t, CVector*, CVector*, std::uint8_t) override;
        void       RemoveBuilding(CBuilding*) override;
        bool       Resize(int) override;
        void       RestoreBackup() override;
    };
    class MissingBuildingRestoreBackup : public CBuildingsPoolUsageDE
    {
        CBuilding* AddBuilding(CClientBuilding*, std::uint16_t, CVector*, CVector*, std::uint8_t) override;
        void       RemoveBuilding(CBuilding*) override;
        bool       Resize(int) override;
        void       RemoveAllWithBackup() override;
    };
    class MissingSingleResize : public CPtrNodeSingleLinkPoolUsageDE
    {
        void ResetCapacity() override;
    };
    class MissingSingleReset : public CPtrNodeSingleLinkPoolUsageDE
    {
        bool Resize(std::size_t) override;
    };
    static_assert(std::is_abstract_v<MissingBuildingAdd> && std::is_abstract_v<MissingBuildingRemove> && std::is_abstract_v<MissingBuildingResize> &&
                  std::is_abstract_v<MissingBuildingRemoveBackup> && std::is_abstract_v<MissingBuildingRestoreBackup> &&
                  std::is_abstract_v<MissingSingleResize> && std::is_abstract_v<MissingSingleReset>);

    template <class Check>
    void Refused(Buildings& buildings, Singles& singles, Check check, const char* reason)
    {
        Value building{0xabcde, 0xfedcb}, single{0xabcde, 0xfedcb};
        check(buildings.ReadUsage(building) != ClockResult::Ok && building.capacity == 0xabcde && building.used == 0xfedcb &&
                  singles.ReadUsage(single) != ClockResult::Ok && single.capacity == 0xabcde && single.used == 0xfedcb,
              reason);
        CBuildingsPool&               buildingSdk = buildings;
        const CBuildingsPool&         constBuildingSdk = buildings;
        const CPtrNodeSingleLinkPool& singleSdk = singles;
        bool                          size = false, free = false, capacity = false, used = false;
        try
        {
            (void)constBuildingSdk.GetSize();
        }
        catch (const std::runtime_error&)
        {
            size = true;
        }
        try
        {
            (void)buildingSdk.HasFreeBuildingSlot();
        }
        catch (const std::runtime_error&)
        {
            free = true;
        }
        try
        {
            (void)singleSdk.GetCapacity();
        }
        catch (const std::runtime_error&)
        {
            capacity = true;
        }
        try
        {
            (void)singleSdk.GetUsedSize();
        }
        catch (const std::runtime_error&)
        {
            used = true;
        }
        check(size && free && capacity && used, "all nested SDK usage virtuals retain explicit native owner refusal");
    }

    template <class Check>
    void Run(U::State& state, Buildings& buildings, Singles& singles, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner afterApplication,
             Check check)
    {
        CBuildingsPool&               buildingSdk = buildings;
        const CBuildingsPool&         constBuildingSdk = buildings;
        const CPtrNodeSingleLinkPool& singleSdk = singles;
        const auto                    expect = [&](std::uint32_t buildingUsed, std::uint32_t singleUsed, const char* reason)
        {
            Value b{0xabcde, 0xfedcb}, s{0xabcde, 0xfedcb};
            check(buildings.ReadUsage(b) == ClockResult::Ok && singles.ReadUsage(s) == ClockResult::Ok &&
                      b.capacity == static_cast<std::uint32_t>(state.buildings.capacity) && b.used == buildingUsed &&
                      s.capacity == static_cast<std::uint32_t>(state.singles.capacity) && s.used == singleUsed &&
                      constBuildingSdk.GetSize() == state.buildings.capacity && buildingSdk.HasFreeBuildingSlot() == (buildingUsed < b.capacity) &&
                      singleSdk.GetCapacity() == static_cast<std::size_t>(state.singles.capacity) && singleSdk.GetUsedSize() == singleUsed,
                  reason);
        };
        expect(0, 0, "both nested SDK receivers consume the same actual empty native allocation banks through CPools");
        for (auto* pool : {&state.buildings, &state.singles})
            for (std::int32_t i = 0; i != pool->capacity; ++i)
                pool->flags[i] = static_cast<unsigned char>(i & 0x7f);
        expect(static_cast<std::uint32_t>(state.buildings.capacity), static_cast<std::uint32_t>(state.singles.capacity),
               "a full native Building bank has no free building slot while native Single used size equals its capacity");
        std::array<std::uint32_t, 2> expected{};
        std::size_t                  selected = 0;
        for (auto* pool : {&state.buildings, &state.singles})
        {
            for (std::int32_t i = 0; i != pool->capacity; ++i)
            {
                const bool occupied = i == 0 || i + 1 == pool->capacity || i == 255 || i == 256 || i == 511 || i == 512 || i % 5 == 3;
                pool->flags[i] = static_cast<unsigned char>((i & 0x7f) | (occupied ? 0 : 0x80));
                expected[selected] += occupied;
            }
            ++selected;
        }
        const std::vector<unsigned char> buildingBefore(state.buildings.flags, state.buildings.flags + state.buildings.capacity);
        const std::vector<unsigned char> singleBefore(state.singles.flags, state.singles.flags + state.singles.capacity);
        expect(expected[0], expected[1], "nested SDK readers include both terminal flags, chunk edges and native generation-bearing mixed allocation states");
        check(std::equal(buildingBefore.begin(), buildingBefore.end(), state.buildings.flags) &&
                  std::equal(singleBefore.begin(), singleBefore.end(), state.singles.flags),
              "nested SDK usage readers do not alter native allocation flags");
        for (auto* pool : {&state.buildings, &state.singles})
            for (std::int32_t i = 0; i != pool->capacity; ++i)
                pool->flags[i] ^= 0x7f;
        expect(expected[0], expected[1], "nested native count readers ignore all seven generation bits while respecting the free high bit");

        selected = 0;
        for (auto* pool : {&state.buildings, &state.singles})
        {
            const auto kind = selected == 0 ? U::Kind::Building : U::Kind::PointerSingleLink;
            const auto root = selected == 0 ? U::Roots[0] : U::Roots[6];
            const auto originalHeader = pool->header;
            const auto readNested = [&](Value& output) { return selected == 0 ? buildings.ReadUsage(output) : singles.ReadUsage(output); };
            const auto reject = [&](const char* reason)
            {
                Value output{0xabcde, 0xfedcb};
                check(readNested(output) != ClockResult::Ok && output.capacity == 0xabcde && output.used == 0xfedcb, reason);
                bool first = false, second = false;
                try
                {
                    if (selected == 0)
                        (void)constBuildingSdk.GetSize();
                    else
                        (void)singleSdk.GetCapacity();
                }
                catch (const std::runtime_error&)
                {
                    first = true;
                }
                try
                {
                    if (selected == 0)
                        (void)buildingSdk.HasFreeBuildingSlot();
                    else
                        (void)singleSdk.GetUsedSize();
                }
                catch (const std::runtime_error&)
                {
                    second = true;
                }
                check(first && second, "both selected nested virtuals propagate the real CPools owner refusal instead of publishing an empty bank");
            };
            state.Global(root, std::uintptr_t{0});
            reject("nested status readers preserve both caller fields when their exact native allocation root is absent");
            state.Global(root, pool->Address());
            U::W::Put(pool->Address(), 0x10, pool->capacity - 1);
            reject("nested SDK readers cannot substitute a different capacity for the qualified native allocation");
            pool->header = originalHeader;
            pool->header[0x18] = 0;
            reject("nested SDK readers retain native pool ownership validation");
            pool->header = originalHeader;
            U::W::Put(pool->Address(), 8, reinterpret_cast<std::uintptr_t>(pool->objects));
            reject("nested native allocation flags cannot alias the object storage");
            pool->header = originalHeader;
            DWORD      previous = 0;
            const bool guarded = VirtualProtect(pool->flags + pool->capacity - 1, 1, PAGE_NOACCESS, &previous) != 0;
            check(guarded, "the nested reader's owned terminal flag page can be made unreadable");
            if (guarded)
            {
                reject("an unreadable terminal flag refuses each nested SDK path");
                DWORD ignored = 0;
                check(VirtualProtect(pool->flags + pool->capacity - 1, 1, previous, &ignored) != 0, "the nested fixture restores its owned allocation page");
            }
            const std::array<std::uintptr_t, 2> pins =
                selected == 0 ? std::array<std::uintptr_t, 2>{mta::game_de::NativeBuildingPoolDefaultProfile::Pins[0].rva, 0x1036840}
                              : std::array<std::uintptr_t, 2>{0x1171130, 0x1131ff0};
            for (const auto rva : pins)
            {
                unsigned char original = 0;
                std::memcpy(&original, reinterpret_cast<const void*>(state.image + rva), sizeof(original));
                const auto corrupt = static_cast<unsigned char>(original ^ 1);
                state.writer(state.host, rva, &corrupt, sizeof(corrupt), PAGE_EXECUTE_READ);
                reject("nested usage paths retain exact native constructor and flag allocator source qualification");
                state.writer(state.host, rva, &original, sizeof(original), PAGE_EXECUTE_READ);
            }
            U::Bookends bookends;
            bookends.owner = owner;
            bookends.after = afterApplication;
            U::Bookends::current = &bookends;
            Value current;
            check(M::Read(state.image, generation, kind, &U::Bookends::Owner, &U::Bookends::After, current) == ClockResult::Ok,
                  "nested delegates share the underlying census with its actual Runtime owner callbacks");
            const auto last = bookends.calls;
            const auto moving = [&](std::function<void()> mutation, std::function<void()> restore, const char* reason)
            {
                bookends.calls = 0;
                bookends.mutateAt = last;
                bookends.mutation = std::move(mutation);
                Value output{0xabcde, 0xfedcb};
                check(M::Read(state.image, generation, kind, &U::Bookends::Owner, &U::Bookends::After, output) != ClockResult::Ok &&
                          output.capacity == 0xabcde && output.used == 0xfedcb,
                      reason);
                restore();
                bookends.mutateAt = 0;
                bookends.mutation = {};
                check(readNested(output) == ClockResult::Ok && output.used == expected[selected],
                      "restored source bytes make the exact same nested delegate admissible again");
            };
            moving([&] { state.Global(root, std::uintptr_t{0}); }, [&] { state.Global(root, pool->Address()); },
                   "a final actual owner callback cannot replace the native root consumed by the nested delegate's census");
            moving([&] { pool->header[31] ^= 1; }, [&] { pool->header[31] ^= 1; },
                   "the underlying nested census seals all 32 header bytes after its final real owner callback");
            moving([&] { pool->flags[pool->capacity - 1] ^= 1; }, [&] { pool->flags[pool->capacity - 1] ^= 1; },
                   "generation-only drift in the terminal flag refuses the underlying snapshot used by nested SDK readers");
            moving([&] { std::swap(pool->flags[0], pool->flags[1]); }, [&] { std::swap(pool->flags[0], pool->flags[1]); },
                   "an unchanged aggregate count cannot hide a final occupied/free slot swap from the nested delegate's complete census");
            U::Bookends::current = nullptr;
            ++selected;
        }
        expect(expected[0], expected[1], "both actual nested SDK receivers remain usable after all independent source refusal cases are restored");
        std::puts("Production nested pool reader SDK methods completed");
    }
}
