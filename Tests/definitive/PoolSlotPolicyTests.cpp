/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual classic pool selection plus shared x86/x64 bounds contracts
 *****************************************************************************/
#include <windows.h>
#include <SharedUtil.IntTypes.h>
#include <SharedUtil.Defines.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
#include <game/CPoolSlotPolicy.h>
// The actual classic SDK deliberately uses Microsoft's anonymous bitfield
// union. Preserve its one-byte layout rather than replacing it in this test.
#pragma warning(push)
#pragma warning(disable : 4201)
#include <CPoolSAInterface.h>
#pragma warning(pop)

namespace
{
    using Policy = MtaGame::CPoolSlotPolicy;
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
        {
            std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, why);
            std::exit(EXIT_FAILURE);
        }
    }

    // These are ordinary caller-owned CPU rows. No CPed, native pool allocation,
    // game address or wrapper registry is fabricated or published by this test.
    struct Row
    {
        unsigned first = 0;
        unsigned second = 0;
    };
    using ActualPool = CPoolSAInterface<Row>;
    struct OriginalLayout
    {
        Row*              objects;
        tPoolObjectFlags* flags;
        int               count;
        int               cursor;
        bool              owns;
        bool              spare;
    };
    static_assert(std::is_standard_layout_v<ActualPool>);
    static_assert(sizeof(tPoolObjectFlags) == 1);
    static_assert(sizeof(ActualPool) == sizeof(OriginalLayout));
    static_assert(alignof(ActualPool) == alignof(OriginalLayout));
    static_assert(offsetof(ActualPool, m_pObjects) == offsetof(OriginalLayout, objects));
    static_assert(offsetof(ActualPool, m_byteMap) == offsetof(OriginalLayout, flags));
    static_assert(offsetof(ActualPool, m_nSize) == offsetof(OriginalLayout, count));
    static_assert(offsetof(ActualPool, m_nFirstFree) == offsetof(OriginalLayout, cursor));
    static_assert(offsetof(ActualPool, m_bOwnsAllocations) == offsetof(OriginalLayout, owns));
    static_assert(offsetof(ActualPool, field_11) == offsetof(OriginalLayout, spare));

    std::vector<std::int32_t> ExpectedProbeOrder(std::int32_t size, std::int32_t cursor)
    {
        std::vector<std::int32_t> result;
        if (size <= 0)
            return result;
        // Describe the existing two consecutive ranges, rather than reproduce
        // the implementation's mutable cursor/wrap loop. A failed search can
        // revisit its suffix; that observable legacy order is intentional.
        const auto start = cursor >= -1 && cursor <= size - 2 ? cursor + 1 : 0;
        for (auto index = start; index < size; ++index)
            result.push_back(index);
        for (std::int32_t index = 0; index < size; ++index)
            result.push_back(index);
        return result;
    }

    void PureSelection()
    {
        for (std::int32_t size = 1; size <= 9; ++size)
        {
            const auto masks = std::uint32_t{1} << static_cast<unsigned>(size);
            for (std::uint32_t mask = 0; mask < masks; ++mask)
                for (std::int32_t cursor = -2; cursor <= size + 1; ++cursor)
                {
                    auto expected = ExpectedProbeOrder(size, cursor);
                    auto selected = Policy::InvalidSlot;
                    for (std::size_t offset = 0; offset < expected.size(); ++offset)
                        if (mask & (std::uint32_t{1} << static_cast<unsigned>(expected[offset])))
                        {
                            selected = static_cast<std::uint32_t>(expected[offset]);
                            expected.resize(offset + 1);
                            break;
                        }
                    std::vector<std::int32_t> probes;
                    const auto                found = Policy::FindFreeAfter(size, cursor,
                                                                            [&](std::int32_t slot)
                                                                            {
                                                                 Check(slot >= 0 && slot < size, "probe stays within actual signed capacity");
                                                                 probes.push_back(slot);
                                                                 return (mask & (std::uint32_t{1} << static_cast<unsigned>(slot))) != 0;
                                                             });
                    Check(found == selected, "all occupancy combinations preserve selected classic slot");
                    Check(probes == expected, "classic range order and failure revisits are preserved exactly");
                }
        }
        unsigned calls = 0;
        Check(Policy::FindFreeAfter(0, 0,
                                    [&](std::int32_t)
                                    {
                                        ++calls;
                                        return true;
                                    }) == Policy::InvalidSlot &&
                  !calls,
              "zero capacity never probes");
        Check(Policy::FindFreeAfter(-1, -1,
                                    [&](std::int32_t)
                                    {
                                        ++calls;
                                        return true;
                                    }) == Policy::InvalidSlot &&
                  !calls,
              "negative capacity never probes");
        for (const auto cursor : {(std::numeric_limits<std::int32_t>::min)(), (std::numeric_limits<std::int32_t>::max)()})
        {
            Check(Policy::FindFreeAfter(1, cursor, [](std::int32_t slot) { return slot == 0; }) == 0,
                  "malformed extreme cursor clamps without signed addition overflow");
        }
        std::array<unsigned, 3> visits{};
        Check(Policy::FindFreeAfter(3, -1, [&](std::int32_t slot) { return slot == 1 && ++visits[1] == 2; }) == 1,
              "stateful probe still observes the genuine second pass");
    }

    void Bounds()
    {
        constexpr std::array<std::int32_t, 8>   counts = {-1, 0, 1, 4, 140, 1000, 7500, (std::numeric_limits<std::int32_t>::max)()};
        constexpr std::array<std::int32_t, 10>  signedSlots = {(std::numeric_limits<std::int32_t>::min)(), -2, -1, 0, 1, 3, 139, 999, 7499,
                                                               (std::numeric_limits<std::int32_t>::max)()};
        constexpr std::array<std::uint32_t, 11> unsignedSlots = {0, 1, 3, 139, 140, 999, 1000, 7499, 7500, 0x7fffffffu, 0xffffffffu};
        for (auto count : counts)
        {
            for (auto slot : signedSlots)
            {
                const auto expected = count > 0 && static_cast<std::int64_t>(slot) >= 0 && static_cast<std::int64_t>(slot) < count;
                Check(Policy::ContainsIndex(count, slot) == expected, "signed index is compared in its original range");
            }
            for (auto slot : unsignedSlots)
            {
                const auto expected = count > 0 && static_cast<std::uint64_t>(slot) < static_cast<std::uint64_t>(count);
                Check(Policy::ContainsIndex(count, slot) == expected, "DE unsigned handle slot cannot sign-wrap into a valid row");
                Check(Policy::IndexOrInvalid(count, slot) == (expected ? static_cast<std::int32_t>(slot) : -1),
                      "classic index conversion casts only an admitted representable slot");
            }
        }
    }

    void ActualClassicCaller()
    {
        std::array<Row, 4>              rows{};
        std::array<tPoolObjectFlags, 4> flags{};
        ActualPool                      pool;
        pool.m_pObjects = rows.data();
        pool.m_byteMap = flags.data();
        pool.m_nSize = static_cast<int>(rows.size());
        pool.m_nFirstFree = -1;
        pool.m_bOwnsAllocations = false;
        pool.field_11 = false;
        for (auto& flag : flags)
        {
            flag.nId = 37;
            flag.bEmpty = false;
        }
        const auto beforeFull = flags;
        Check(pool.GetFreeSlot() == Policy::InvalidSlot && pool.m_nFirstFree == -1, "real classic full-pool search preserves its caller-owned cursor");
        Check(std::memcmp(beforeFull.data(), flags.data(), sizeof(flags)) == 0, "real classic search does not allocate or change any generation byte");
        flags[1].bEmpty = true;
        flags[3].bEmpty = true;
        const auto beforeFree = flags;
        Check(pool.GetFreeSlot() == 1 && pool.m_nFirstFree == 1, "real classic caller retains the selected cursor");
        Check(pool.GetFreeSlot() == 3 && pool.m_nFirstFree == 3, "next actual classic search continues after its selected slot");
        Check(pool.GetFreeSlot() == 1 && pool.m_nFirstFree == 1, "real classic caller wraps and an empty slot remains unreserved");
        Check(std::memcmp(beforeFree.data(), flags.data(), sizeof(flags)) == 0, "three selections leave allocation/generation ownership untouched");
        Check(pool.IsContains(0) && !pool.IsContains(1) && pool.IsContains(2) && !pool.IsContains(3),
              "real classic occupancy method consults the existing one-byte bitfield");
        Check(!pool.IsContains(-1) && !pool.IsContains(4), "real classic bounds refuse before consulting a flag");
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            Check(pool.GetObject(static_cast<std::int32_t>(index)) == &rows[index], "unchanged actual classic row-stride lookup");
            Check(pool.GetObjectIndexSafe(&rows[index]) == static_cast<std::int32_t>(index), "actual safe row lookup uses shared admitted index");
        }
        Check(pool.GetObjectIndexSafe(rows.data() + rows.size()) == -1, "real classic one-past row is outside its capacity");
        pool.m_nSize = 0;
        pool.m_byteMap = nullptr;
        Check(pool.GetFreeSlot() == Policy::InvalidSlot && pool.m_nFirstFree == 1, "uninitialized classic pool does not probe or alter cursor");
        Check(!pool.IsContains(0), "zero classic capacity short-circuits absent native flags");
        pool.m_nSize = 4;
        Check(pool.GetFreeSlot() == Policy::InvalidSlot && pool.m_nFirstFree == 1, "absent classic flag map remains guarded by real caller");
    }
}

int main()
{
    Bounds();
    PureSelection();
    ActualClassicCaller();
    std::printf("Passed %u actual MTA pool slot policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
}
