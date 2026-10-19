/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Native FStructBaseChain observations over guarded owned buffers
 *****************************************************************************/
#include "../../Client/game_de/NativeClassChainAccess.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>

namespace
{
    using Access = mta::game_de::NativeClassChainAccess;
    unsigned checks = 0;
    void     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u native class-chain checks: %s\n", checks, reason);
            std::exit(EXIT_FAILURE);
        }
    }
    template <typename T, std::size_t N>
    void Put(std::array<unsigned char, N>& bytes, std::size_t offset, T value)
    {
        Check(offset <= N && sizeof(value) <= N - offset, "owned fixture field fits its independent byte layout");
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    // Literal offsets/identities express the saved native contract independently
    // of the helper. Virtual addresses are translated only into these arrays.
    constexpr std::uintptr_t candidate = 0x1000, base = 0x2000, chain = 0x4000;
    struct Fixture;
    Fixture* current = nullptr;
    struct Fixture
    {
        enum class Mutation
        {
            None,
            CandidatePointer,
            CandidateIndex,
            BasePointer,
            BaseIndex,
            Selected,
            BetweenHeaders,
            BetweenSlots
        };
        std::array<unsigned char, 0x40>   object{}, expected{};
        std::array<unsigned char, 64 * 8> slots{};
        unsigned                          reads = 0, candidateReads = 0, baseReads = 0, slotReads = 0, denied = 0, failAt = 0;
        std::uintptr_t                    deniedAddress = 0;
        Mutation                          mutation = Mutation::None;

        Fixture()
        {
            current = this;
            Header(4, 2);
            Put(slots, 2 * 8, base + 0x30);
            // A metaclass/adjacent-field decoy must not influence the result.
            Put(object, 0x10, std::uintptr_t{0x99990000});
            Put(expected, 0x3c, std::uint32_t{0xfeedcafe});
        }
        void Header(std::int32_t candidateIndex, std::int32_t baseIndex)
        {
            Put(object, 0x30, chain);
            Put(object, 0x38, candidateIndex);
            // Native does not dereference the expected base's stored array.
            Put(expected, 0x30, std::uintptr_t{0});
            Put(expected, 0x38, baseIndex);
        }
        bool Read(std::uintptr_t at, void* destination, std::size_t bytes) noexcept
        {
            ++reads;
            const unsigned char* source = nullptr;
            if (at == candidate + 0x30 && bytes == 12)
            {
                ++candidateReads;
                source = object.data() + 0x30;
            }
            else if (at == base + 0x30 && bytes == 12)
            {
                ++baseReads;
                source = expected.data() + 0x30;
            }
            else if (at >= chain && at - chain <= slots.size() - 8 && bytes == 8 && (at - chain) % 8 == 0)
            {
                ++slotReads;
                source = slots.data() + (at - chain);
            }
            if (!source || at == deniedAddress || reads == failAt)
            {
                ++denied;
                // A guarded failure may have partially overwritten a local
                // sample; it must never alter the caller's relation output.
                std::memset(destination, 0xa7, bytes);
                return false;
            }
            std::memcpy(destination, source, bytes);
            if (at == candidate + 0x30 && bytes == 12)
            {
                if (mutation == Mutation::CandidatePointer && candidateReads == 2)
                {
                    const std::uintptr_t changed = chain + 8;
                    std::memcpy(destination, &changed, sizeof(changed));
                }
                if ((mutation == Mutation::CandidateIndex && candidateReads == 2) || (mutation == Mutation::BetweenHeaders && candidateReads >= 3))
                {
                    const std::int32_t changed = 5;
                    std::memcpy(static_cast<unsigned char*>(destination) + 8, &changed, sizeof(changed));
                }
            }
            if (at == base + 0x30 && bytes == 12 && baseReads == 2)
            {
                if (mutation == Mutation::BasePointer)
                {
                    const std::uintptr_t changed = 0x88880000;
                    std::memcpy(destination, &changed, sizeof(changed));
                }
                if (mutation == Mutation::BaseIndex)
                {
                    const std::int32_t changed = 3;
                    std::memcpy(static_cast<unsigned char*>(destination) + 8, &changed, sizeof(changed));
                }
            }
            if (bytes == 8 && ((mutation == Mutation::Selected && slotReads == 2) || (mutation == Mutation::BetweenSlots && slotReads >= 3)))
            {
                const std::uintptr_t changed = candidate + 0x30;
                std::memcpy(destination, &changed, sizeof(changed));
            }
            return true;
        }
        static bool GuardedRead(std::uintptr_t at, void* destination, std::size_t bytes) noexcept { return current && current->Read(at, destination, bytes); }
        bool        Observe(bool& value, std::uintptr_t candidateAddress = candidate, std::uintptr_t baseAddress = base) const
        {
            return Access(GuardedRead).IsChildOf(candidateAddress, baseAddress, value);
        }
    };

    void PositiveAndNegative()
    {
        Fixture    fixture;
        const auto beforeObject = fixture.object, beforeBase = fixture.expected;
        const auto beforeSlots = fixture.slots;
        bool       relation = false;
        Check(fixture.Observe(relation) && relation, "selected base-chain subobject proves the derived relation");
        Check(fixture.reads == 12 && fixture.slotReads == 4 && !fixture.denied, "observation reads two bounded headers and one selected slot coherently");
        Check(fixture.object == beforeObject && fixture.expected == beforeBase && fixture.slots == beforeSlots, "read-only source buffers remain unchanged");

        for (const auto wrong : {std::uintptr_t{0}, base, candidate + 0x30, chain + 2 * 8})
        {
            Fixture wrongType;
            Put(wrongType.slots, 2 * 8, wrong);
            Put(wrongType.object, 0x10, base + 0x30);
            relation = true;
            Check(wrongType.Observe(relation) && !relation, "null, UClass, metaclass decoy and self-referential array identities are stable false");
            Check(wrongType.slotReads == 4 && !wrongType.denied, "array values are compared once, never followed as a linked inheritance list");
        }
        Fixture tooShallow;
        tooShallow.Header(2, 3);
        Put(tooShallow.object, 0x30, std::uintptr_t{0});
        relation = true;
        Check(tooShallow.Observe(relation) && !relation && !tooShallow.slotReads,
              "a shallower candidate is stable false without dereferencing an unused chain");

        for (const std::int32_t index : {0, 63})
        {
            Fixture boundary;
            boundary.Header(index, index);
            Put(boundary.slots, static_cast<std::size_t>(index) * 8, base + 0x30);
            relation = false;
            Check(boundary.Observe(relation) && relation, "first and last admitted depth indices use the native equality semantics");
        }
        Fixture opaqueBase;
        Put(opaqueBase.expected, 0x30, (std::numeric_limits<std::uintptr_t>::max)());
        relation = false;
        Check(opaqueBase.Observe(relation) && relation && !opaqueBase.denied, "base stored array pointer is opaque and never dereferenced");
        Fixture sameClass;
        Put(sameClass.slots, 4 * 8, candidate + 0x30);
        relation = false;
        Check(sameClass.Observe(relation, candidate, candidate) && relation, "a class observes itself through its actual indexed base-chain identity");
        Fixture corruptSelf;
        relation = true;
        Check(corruptSelf.Observe(relation, candidate, candidate) && !relation, "equal class pointers do not bypass the native array equality test");
    }

    void Refusals()
    {
        for (bool sentinel : {false, true})
        {
            Fixture fixture;
            bool    relation = sentinel;
            Check(!fixture.Observe(relation, 0, base) && relation == sentinel && !fixture.reads, "null candidate refuses without changing output");
            Check(!fixture.Observe(relation, candidate, 0) && relation == sentinel && !fixture.reads, "null expected base refuses without changing output");
            Check(!Access(nullptr).IsChildOf(candidate, base, relation) && relation == sentinel, "absent guarded read policy refuses");
        }
        for (const auto index : {std::int32_t{-1}, (std::numeric_limits<std::int32_t>::min)(), std::int32_t{64}, (std::numeric_limits<std::int32_t>::max)()})
        {
            for (bool badBase : {false, true})
            {
                Fixture fixture;
                fixture.Header(badBase ? 4 : index, badBase ? index : 2);
                bool relation = true;
                Check(!fixture.Observe(relation) && relation && !fixture.slotReads, "negative or over-budget index refuses before array arithmetic");
            }
        }
        for (const auto address : {(std::numeric_limits<std::uintptr_t>::max)() - 0x2f, (std::numeric_limits<std::uintptr_t>::max)() - 0x30})
        {
            Fixture fixture;
            bool    relation = true;
            Check(!fixture.Observe(relation, address, base) && relation && !fixture.reads, "candidate header addition or copied span overflow refuses");
            Check(!fixture.Observe(relation, candidate, address) && relation, "base header addition or copied span overflow refuses");
        }
        for (const auto pointer : {std::uintptr_t{0}, (std::numeric_limits<std::uintptr_t>::max)() - 15, (std::numeric_limits<std::uintptr_t>::max)() - 20})
        {
            Fixture fixture;
            Put(fixture.object, 0x30, pointer);
            bool relation = true;
            Check(!fixture.Observe(relation) && relation && !fixture.slotReads, "null chain, selected-slot addition and selected span overflow refuse");
        }
        for (const auto denied : {candidate + 0x30, base + 0x30, chain + 2 * 8})
        {
            Fixture fixture;
            fixture.deniedAddress = denied;
            bool relation = true;
            Check(!fixture.Observe(relation) && relation && fixture.denied == 1, "qualified read refusal preserves the output even after a partial local copy");
        }
        // Every actual read boundary can fail independently; none may promote
        // a half sample or overwrite either initial relation value.
        for (unsigned read = 1; read <= 12; ++read)
        {
            for (bool sentinel : {false, true})
            {
                Fixture fixture;
                fixture.failAt = read;
                bool relation = sentinel;
                Check(!fixture.Observe(relation) && relation == sentinel && fixture.denied == 1, "each guarded read failure preserves caller output");
            }
        }
    }

    void Stability()
    {
        for (const auto mutation :
             {Fixture::Mutation::CandidatePointer, Fixture::Mutation::CandidateIndex, Fixture::Mutation::BasePointer, Fixture::Mutation::BaseIndex,
              Fixture::Mutation::Selected, Fixture::Mutation::BetweenHeaders, Fixture::Mutation::BetweenSlots})
        {
            for (bool sentinel : {false, true})
            {
                Fixture fixture;
                fixture.mutation = mutation;
                bool relation = sentinel;
                Check(!fixture.Observe(relation) && relation == sentinel, "header and selected-slot changes within or between samples refuse boundedly");
                Check(fixture.reads <= 12, "unstable native observations never retry or chase a pointer chain indefinitely");
            }
        }
    }
}

int main()
{
    static_assert(sizeof(std::uintptr_t) == 8);
    PositiveAndNegative();
    Refusals();
    Stability();
    std::printf("Passed %u native class-chain checks\n", checks);
    std::puts("Native base+30 equality, stable false, bounded index, overflow, guarded refusal and coherent header/slot checks completed");
    return EXIT_SUCCESS;
}
