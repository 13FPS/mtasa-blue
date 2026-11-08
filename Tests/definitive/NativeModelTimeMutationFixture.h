/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned timed-model rows and real CAS boundaries for current owners
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeModelTimeMemory.h"
#include "../../Client/game_de/NativeModelTimeDomain.h"
#include "../../Client/game_de/NativeModelTimeProfile.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include "../../Client/game_de/CModelInfoTimeDE.h"
#include "../../Client/game_de/CGameModelTimeResetDE.h"
#include <windows.h>
#include <intrin.h>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

// Include this header before the fixture selects its intrinsic name macro.
// The wrapper always executes the genuine CPU CAS; only independent owned
// source mutation and counters surround the final production instruction.
namespace ClockModelTimeMutationTest
{
    inline std::array<std::uintptr_t, 2> fields{};
    inline std::array<unsigned, 2>       exchanges{};
    inline std::uintptr_t                providerCount = 0;
    inline bool                          failAfterCommitted = false, injected = false;
    inline void (*afterExchange)() noexcept = nullptr;
    inline SHORT CompareExchange(volatile SHORT* address, SHORT desired, SHORT expected) noexcept
    {
        const auto observed = ::_InterlockedCompareExchange16(address, desired, expected);
        const auto at = reinterpret_cast<std::uintptr_t>(address);
        for (std::size_t index = 0; index != fields.size(); ++index)
        {
            if (!fields[index] || at != fields[index])
                continue;
            ++exchanges[index];
            if (observed == expected)
            {
                if (failAfterCommitted && !injected && index == 0)
                {
                    const std::int32_t missing = 2;
                    std::memcpy(reinterpret_cast<void*>(providerCount), &missing, sizeof(missing));
                    injected = true;
                }
                if (afterExchange)
                    afterExchange();
            }
        }
        return observed;
    }
}
namespace NativeModelTimeMutationFixture
{
    namespace P = mta::game_de::NativeModelTimeProfile;
    namespace B = mta::game_de::NativeModelBankProfile;
    namespace M = mta::game_de::NativeModelTimeMemory;
    using mta::game_de::ClockResult;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class T>
    void Store(std::uintptr_t at, std::size_t offset, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(at + offset), &value, sizeof(value));
    }
    struct Storage
    {
        std::uintptr_t image;
        Writer         writer;
        void*          host;
        alignas(std::uintptr_t) std::array<unsigned char, 0x100> nonTime{}, secondType{};
        Storage(std::uintptr_t image, Writer writer, void* host) : image(image), writer(writer), host(host)
        {
            for (const auto& pin : P::Pins)
                writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            std::array<std::uintptr_t, 16> table{};
            for (std::size_t index = 0; index != std::size(P::VtableMethods); ++index)
                table[index] = image + P::VtableMethods[index];
            table[15] = image + 0xace850;
            writer(host, P::VtableRva, table.data(), sizeof(table), PAGE_READONLY);
            const std::array<unsigned char, 8> animationLeaf{0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc3};
            writer(host, 0xace850, animationLeaf.data(), animationLeaf.size(), PAGE_EXECUTE_READ);
            const std::array<unsigned char, 6> type{0xb0, 3, 0xc3, 0xcc, 0xcc, 0xcc};
            writer(host, P::TypeMethodRva, type.data(), type.size(), PAGE_EXECUTE_READ);
            std::vector<unsigned char> pool(P::PoolCapacity * P::RowBytes, 0xa5);
            for (std::size_t index = 0; index != P::PoolCapacity; ++index)
            {
                auto*      row = pool.data() + index * P::RowBytes;
                const auto vtable = image + P::VtableRva;
                std::memcpy(row, &vtable, sizeof(vtable));
                const std::array<unsigned char, 4> tuple =
                    index == 0 ? std::array<unsigned char, 4>{6, 18, 0xff, 0xff} : std::array<unsigned char, 4>{20, 6, 0x23, 1};
                std::memcpy(row + P::TimeOffset, tuple.data(), tuple.size());
            }
            writer(host, P::PoolRva, pool.data(), pool.size(), PAGE_READWRITE);
            Global(P::PoolCountRva, std::int32_t{2});
            Bind(3, Row(0));
            Bind(4, Row(1));
            Bind(7, Row(0));
            Bind(8, 0);
            constexpr std::uintptr_t           nonTimeTable = 0x431bf00, nonTimeLeaf = 0x1100030, otherTable = 0x431c000;
            const std::array<unsigned char, 6> nonTimeCode{0xb8, 6, 0, 0, 0, 0xc3};
            writer(host, nonTimeLeaf, nonTimeCode.data(), nonTimeCode.size(), PAGE_EXECUTE_READ);
            auto nonTimeMethods = table;
            nonTimeMethods[4] = image + nonTimeLeaf;
            writer(host, nonTimeTable, nonTimeMethods.data(), sizeof(nonTimeMethods), PAGE_READONLY);
            writer(host, otherTable, table.data(), sizeof(table), PAGE_READONLY);
            Store(reinterpret_cast<std::uintptr_t>(nonTime.data()), 0, image + nonTimeTable);
            Store(reinterpret_cast<std::uintptr_t>(secondType.data()), 0, image + otherTable);
            Bind(9, reinterpret_cast<std::uintptr_t>(nonTime.data()));
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        std::uintptr_t               Row(std::size_t index = 0) const noexcept { return image + P::PoolRva + index * P::RowBytes; }
        void                         Bind(std::uint32_t model, std::uintptr_t metadata) { Global(B::BankRva + model * B::SlotBytes, metadata); }
        std::array<unsigned char, 4> Tuple(std::size_t index = 0) const noexcept
        {
            std::array<unsigned char, 4> tuple{};
            std::memcpy(tuple.data(), reinterpret_cast<void*>(Row(index) + P::TimeOffset), tuple.size());
            return tuple;
        }
        void          Tuple(const std::array<unsigned char, 4>& tuple, std::size_t index = 0) { Store(Row(index), P::TimeOffset, tuple); }
        std::uint16_t Hours(std::size_t index = 0) const noexcept
        {
            const auto    tuple = Tuple(index);
            std::uint16_t value = 0;
            std::memcpy(&value, tuple.data(), sizeof(value));
            return value;
        }
        std::vector<unsigned char> RowBytes(std::size_t index = 0) const
        {
            std::vector<unsigned char> result(P::RowBytes);
            std::memcpy(result.data(), reinterpret_cast<void*>(Row(index)), result.size());
            return result;
        }
    };
    // Calibrate the actual post-CAS callback count, then refuse the next
    // callback before a second row can enter. No fixed implementation count
    // or fake successful owner is substituted for the real native owner.
    struct Gate
    {
        M::CheckOwner                    owner = nullptr, after = nullptr;
        unsigned                         calls = 0, limit = (std::numeric_limits<unsigned>::max)();
        bool                             counting = false;
        inline static thread_local Gate* current = nullptr;
        static ClockResult               Owner() noexcept
        {
            const auto result = current->owner();
            if (result == ClockResult::Ok && current->counting && ++current->calls > current->limit)
                return ClockResult::BindingChanged;
            return result;
        }
        static ClockResult After() noexcept { return current->after(); }
        static void        Began() noexcept
        {
            current->counting = true;
            current->calls = 0;
        }
    };
    template <class Check>
    void MemoryRefusals(Storage& storage, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner after, Check check)
    {
        M::Value value;
        check(M::Capture(storage.image, generation, 8, owner, after, value) == ClockResult::Ok && !value.timed && !value.model.nativeModelInfo,
              "qualified absent model has no time info and creates no write authority");
        check(M::Capture(storage.image, generation, 9, owner, after, value) == ClockResult::Ok && !value.timed && value.model.nativeModelInfo,
              "qualified current non-time metadata follows the genuine false predicate");
        storage.Bind(3, reinterpret_cast<std::uintptr_t>(storage.secondType.data()));
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::UnsupportedBuild,
              "the independent second type3 table is explicitly refused rather than treated as this static Time producer");
        Store(reinterpret_cast<std::uintptr_t>(storage.secondType.data()), 0, storage.image + P::VtableRva);
        check(M::Capture(storage.image, generation, 3, owner, after, value) != ClockResult::Ok,
              "an arbitrary heap row with the exact supported timed vtable still lacks allocated static-prefix authority");
        storage.Bind(3, storage.Row());
        storage.Global(P::PoolCountRva, std::int32_t{1});
        check(M::Capture(storage.image, generation, 4, owner, after, value) != ClockResult::Ok,
              "a matching timed row beyond the constructor-published allocated prefix cannot receive a write");
        storage.Global(P::PoolCountRva, std::int32_t{169});
        storage.Bind(5, storage.Row(168));
        check(M::Capture(storage.image, generation, 5, owner, after, value) == ClockResult::Ok && value.timed && value.poolCount == 169,
              "the last of the genuine169 static rows is admitted only inside its allocated prefix");
        storage.Global(P::PoolCountRva, std::int32_t{2});
        storage.Bind(5, 0);
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::Ok && value.timed,
              "the original typed timed receiver remains current after independent refusal cases");
        const auto original = value;
        auto       other = value;
        other.model.modelIndex = 7;
        other.model.slotAddress = storage.image + B::BankRva + 7 * B::SlotBytes;
        check(!M::SameIdentity(original, other) && M::SameStorageIdentity(M::Identity(original), other),
              "bank aliases preserve actual storage identity while keeping their exact routes distinct");
        const auto change = [&](auto mutate, const char* reason)
        {
            auto changed = original;
            mutate(changed);
            check(!M::SameStorageIdentity(M::Identity(original), changed), reason);
        };
        change([](M::Value& v) { ++v.generation; }, "storage defaults cannot cross Runtime generations");
        change([](M::Value& v) { ++v.poolCount; }, "the allocated static prefix is retained identity");
        change([](M::Value& v) { ++v.context.animationMethod; }, "a changed sibling metadata virtual cannot renew a retained default");
        change([](M::Value& v) { ++v.context.rootMembership.outerAddress; }, "full root membership identity is preserved beyond index and serial");
        change([](M::Value& v) { ++v.context.providerMembership.itemAddress; },
               "full provider membership routing is retained without token lifetime authority");
        change([](M::Value& v) { v.tuple[2] ^= 1; }, "the parser-owned pairing word is an immutable sibling of the two mutable hours");
    }
    template <class Check>
    void KnownPrefix(Storage& storage, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner after, Check check)
    {
        namespace F = ClockModelTimeMutationTest;
        const auto first = storage.Tuple(), second = storage.Tuple(1);
        Gate       gate{owner, after};
        Gate::current = &gate;
        M::Value value;
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::Ok, "calibration starts from a real admitted timed row");
        F::afterExchange = &Gate::Began;
        M::Receipt receipt;
        check(M::Write(storage.image, value, storage.Hours(), &Gate::Owner, &Gate::After, receipt) == ClockResult::Ok && receipt.accepted,
              "calibration observes the real post-CAS owner checks without modifying the original hours");
        const auto postCalls = gate.calls;
        check(postCalls > 0, "the actual successful CAS has current owner checks before its post-acknowledgement returns");
        F::afterExchange = nullptr;
        mta::game_de::NativeModelTimeDomain domain;
        bool                                applied = false;
        check(domain.Set(storage.image, generation, 3, 0x1111, owner, after, applied) == ClockResult::Ok && applied &&
                  domain.Set(storage.image, generation, 4, 0x2222, owner, after, applied) == ClockResult::Ok && domain.Pending(),
              "two real timed edits prepare an independently owned restoration prefix");
        const auto counts = F::exchanges;
        gate.calls = 0;
        gate.limit = postCalls;
        gate.counting = false;
        F::afterExchange = &Gate::Began;
        check(domain.ResetAll(storage.image, generation, &Gate::Owner, &Gate::After) != ClockResult::Ok && domain.Pending() && !domain.Uncertain() &&
                  storage.Tuple() == first && storage.Hours(1) == 0x2222 && F::exchanges[0] == counts[0] + 1 && F::exchanges[1] == counts[1],
              "known pre-entry refusal after an acknowledged first restore retains the default map without entering the second CAS");
        F::afterExchange = nullptr;
        gate.counting = false;
        const auto acknowledged = F::exchanges[0];
        check(domain.ResetAll(storage.image, generation, owner, after) == ClockResult::Ok && !domain.Pending() && domain.CanDiscard() &&
                  storage.Tuple() == first && storage.Tuple(1) == second && F::exchanges[0] == acknowledged && F::exchanges[1] == counts[1] + 1,
              "explicit healthy retry freshly validates the accepted prefix and restores only the remaining suffix without CAS replay");
        Gate::current = nullptr;
    }
}
