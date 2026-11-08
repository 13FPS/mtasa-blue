/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine current memory reads over complete owned world head graphs
 ****************************************************************************/
#include "NativeWorldMembershipFixture.h"
namespace NativeWorldMembershipFixture
{
    // The existing host supplies its real current Runtime owner callbacks and
    // world/heap source graph. No final-original double or native call is used.
    // Image writes below are owned fixture setup/corruption only.
    template <class Check>
    void Run(std::uintptr_t image, W::Writer writer, void* host, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner afterApplication, Check check)
    {
        using mta::game_de::ClockResult;
        State state(image, writer, host);
        check(state.Ready(), "membership owned native allocations exist");
        check(P::TotalHeads == 30469 && M::Profile(image), "complete Add/moving source and all physical head extents are qualified");
        const auto read = [&](M::Value& value) { return M::Capture(image, generation, state.kind, 0, owner, afterApplication, value); };
        M::Value   value;
        check(read(value) == ClockResult::Ok && value.occurrences.empty() && value.pools[0].reachable == 0 && value.pools[1].reachable == 0,
              "empty complete native head graphs report callback-local world absence");
        state.Node(0, 10, state.Entity(), 0x13579);
        state.Node(1, 10, state.Entity(), 0x24681, 0x24689);
        check(read(value) == ClockResult::Ok && value.occurrences.empty(),
              "unreachable occupied IPL/Cover-style nodes remain opaque, including their unqualified links");
        state.Reset();
        state.EveryGroup();
        check(read(value) == ClockResult::Ok && value.occurrences.size() == 8 && value.counts == std::array<std::size_t, 7>{1, 1, 2, 1, 1, 1, 1} &&
                  value.pools[0].reachable == 2 && value.pools[1].reachable == 6,
              "every bank's last head and moving singleton are scanned; two distinct selected nodes in one repeat head survive");
        check(value.pools[0].native.free == A::SingleCapacity - 3 && value.pools[1].native.free == A::DoubleCapacity - 7 && M::Seal(image, value),
              "allocated orphans stay counted without becoming a world membership or preventing a healthy seal");
        check(value.occurrences[0].headRva == P::NormalRva + (P::NormalHeads - 2) * 8 && value.occurrences.back().group == M::Group::Moving,
              "observations retain exact physical head identity and do not collapse selected payloads");
        state.Global(P::LodRva, state.singles.Row(10));
        check(!M::Seal(image, value), "a world head outside the selected cached rectangle invalidates the equality fence");
        state.Global(P::LodRva, std::uintptr_t{0});
        check(M::Seal(image, value), "restoring the owned source bytes permits the original callback-local equality fence");
        state.singles.flags[10] ^= 1;
        check(!M::Seal(image, value), "an orphan allocation generation still belongs to the sealed native pool snapshot");
        state.singles.flags[10] ^= 1;
        W::Put(state.doubles.Row(10), 0x10, std::uintptr_t{1});
        check(!M::Seal(image, value), "opaque orphan bytes are sealed without pretending to understand their links");
        W::Put(state.doubles.Row(10), 0x10, std::uintptr_t{0x24689});
        check(M::Seal(image, value), "all current raw bookends match again");
        const auto unchanged = [&](const char* reason)
        {
            M::Value output;
            output.generation = 0xfedcba;
            output.moving = 0x12345;
            output.counts.fill(99);
            const auto result = read(output);
            check(result != ClockResult::Ok && output.generation == 0xfedcba && output.moving == 0x12345 && output.counts[6] == 99, reason);
        };
        state.Global(P::NormalRva, state.singles.Row(0));
        unchanged("one physical Single node shared between separate world heads refuses the entire report");
        state.Global(P::NormalRva, std::uintptr_t{0});
        state.Global(P::MovingRva, state.doubles.Row(4));
        unchanged("one physical Double node shared with another head refuses rather than deduplicating it");
        state.Global(P::MovingRva, state.doubles.Row(5));
        W::Put(state.singles.Row(0), 8, state.singles.Row(0));
        unchanged("a Single cycle is bounded by actual pool slots and refuses output publication");
        W::Put(state.singles.Row(0), 8, std::uintptr_t{0});
        state.doubles.flags[2] |= 0x80;
        unchanged("a reachable next link into a genuinely free native slot refuses");
        state.doubles.flags[2] &= 0x7f;
        W::Put(state.doubles.Row(2), 0x10, std::uintptr_t{0});
        unchanged("Double successor backlinks must agree with the actual predecessor");
        W::Put(state.doubles.Row(2), 0x10, state.doubles.Row(1));
        W::Put(state.doubles.Row(1), 0x10, state.doubles.Row(0));
        unchanged("a Double head has no predecessor");
        W::Put(state.doubles.Row(1), 0x10, std::uintptr_t{0});
        state.Global(P::MovingRva, state.doubles.Row(5) + 8);
        unchanged("interior pointer into a native Double slot is never treated as another node");
        state.Global(P::MovingRva, state.doubles.Row(A::DoubleCapacity));
        unchanged("one-past native pool address refuses without following readable adjacent memory");
        state.Global(P::MovingRva, state.doubles.Row(5));
        const auto originalFlags = reinterpret_cast<std::uintptr_t>(state.doubles.flags);
        W::Put(state.doubles.Address(), 8, reinterpret_cast<std::uintptr_t>(state.singles.flags));
        unchanged("the two native flag allocations must be disjoint");
        W::Put(state.doubles.Address(), 8, originalFlags);
        const auto typedRoot = state.entities.buildings.Address();
        state.Global(A::DoubleRootRva, typedRoot);
        unchanged("a typed entity pool is not a substitute for the native Double allocator owner");
        state.Global(A::DoubleRootRva, state.doubles.Address());
        W::Put(state.doubles.Address(), 0x10, std::int32_t{static_cast<std::int32_t>(A::DoubleCapacity + 1)});
        unchanged("native capacity is exact rather than inferred from readable allocation extent");
        W::Put(state.doubles.Address(), 0x10, std::int32_t{static_cast<std::int32_t>(A::DoubleCapacity)});
        check(read(value) == ClockResult::Ok && value.occurrences.size() == 8, "restored owned native geometry is admissible");
        W::Put(state.Entity(), 0x20, std::uintptr_t{0x13579});
        check(read(value) == ClockResult::Ok && value.occurrences.size() == 8, "read-only membership does not borrow or dereference an actor payload");
        state.Reset(M::Kind::Dummy);
        state.EveryGroup();
        check(read(value) == ClockResult::Ok && value.entity.kind == M::Kind::Dummy && value.occurrences.size() == 8,
              "actual occupied typed Dummy receiver uses the same complete graph scope");
        M::Value output;
        output.generation = 0xfedcba;
        check(M::Capture(image, generation, state.kind, 0, nullptr, afterApplication, output) == ClockResult::NotInitialized && output.generation == 0xfedcba,
              "missing current owner refuses without publishing a report");
        check(M::Capture(image, 0, state.kind, 0, owner, afterApplication, output) == ClockResult::InvalidArgument && output.generation == 0xfedcba,
              "an absent owner generation is not an admitted observation");
    }
}
