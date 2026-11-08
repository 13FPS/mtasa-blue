/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned model distance rows and real CAS boundaries for current owners
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeModelLodMemory.h"
#include "../../Client/game_de/NativeModelLodDomain.h"
#include "../../Client/game_de/NativeModelLodProfile.h"
#include "../../Client/game_de/NativeModelObjectGroupProfile.h"
#include "../../Client/game_de/NativeModelTimeProfile.h"
#include "../../Client/game_de/NativeModelWheelProfile.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include "../../Client/game_de/CModelInfoLodDE.h"
#include "../../Client/game_de/CGameModelLodResetDE.h"
#include <windows.h>
#include <intrin.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

// Include this header before the fixture selects its intrinsic name macro.
// The wrapper always executes the genuine CPU CAS; only independent owned
// source mutation and counters surround the final production instruction.
namespace ClockModelLodMutationTest
{
    inline std::array<std::uintptr_t, 2> fields{};
    inline std::array<unsigned, 2>       exchanges{};
    inline std::uintptr_t                providerCount = 0;
    inline bool                          failAfterCommitted = false, injected = false;
    inline void (*afterExchange)() noexcept = nullptr;
    inline LONG CompareExchange(volatile LONG* address, LONG desired, LONG expected) noexcept
    {
        const auto observed = ::_InterlockedCompareExchange(address, desired, expected);
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
namespace NativeModelLodMutationFixture
{
    namespace P = mta::game_de::NativeModelLodProfile;
    namespace A = mta::game_de::NativeModelObjectGroupProfile;
    namespace T = mta::game_de::NativeModelTimeProfile;
    namespace V = mta::game_de::NativeModelWheelProfile;
    namespace B = mta::game_de::NativeModelBankProfile;
    namespace M = mta::game_de::NativeModelLodMemory;
    using mta::game_de::ClockResult;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class T>
    void Store(std::uintptr_t at, std::size_t offset, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(at + offset), &value, sizeof(value));
    }
    inline std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    inline float Float(std::uint32_t bits) noexcept
    {
        float value = 0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    inline bool RaceExchange(std::uintptr_t at, std::uint32_t expected, std::uint32_t desired, std::uint32_t& observed, std::uint32_t& exception) noexcept
    {
        // Mutate only this fixture's admitted image field after the final
        // production equality fence, then execute the genuine CPU CAS. This
        // proves a competing preimage cannot be overwritten by that window.
        const std::uint32_t competing = 0x43800000u;
        std::memcpy(reinterpret_cast<void*>(at), &competing, sizeof(competing));
        LONG from = 0, to = 0;
        std::memcpy(&from, &expected, sizeof(from));
        std::memcpy(&to, &desired, sizeof(to));
        const LONG result = ::_InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(at), to, from);
        std::memcpy(&observed, &result, sizeof(observed));
        exception = 0;
        return true;
    }
    struct Storage
    {
        std::uintptr_t image;
        Writer         writer;
        void*          host;
        alignas(std::uintptr_t) std::array<unsigned char, 0x100> foreign{};
        Storage(std::uintptr_t image, Writer writer, void* host) : image(image), writer(writer), host(host)
        {
            const auto pins = [&](const auto& values)
            {
                for (const auto& pin : values)
                    writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            };
            pins(A::Pins);
            pins(T::Pins);
            pins(V::Pins);
            pins(P::Pins);
            const auto table = [&](std::uintptr_t rva, const auto& values)
            {
                std::array<std::uintptr_t, 16> methods{};
                for (std::size_t index = 0; index != std::size(values); ++index)
                    methods[index] = image + values[index];
                if (std::size(values) == 8)
                    methods[15] = image + 0xace850;
                writer(host, rva, methods.data(), sizeof(methods), PAGE_READONLY);
                return methods;
            };
            auto methods = table(A::VtableRva, A::VtableMethods);
            table(T::VtableRva, T::VtableMethods);
            table(V::VtableRva, V::VtableMethods);
            const std::array<unsigned char, 8> animationLeaf{0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc3};
            writer(host, 0xace850, animationLeaf.data(), animationLeaf.size(), PAGE_EXECUTE_READ);
            const auto pool = [&](std::uintptr_t rva, std::size_t stride, std::size_t count, std::uintptr_t vtable, std::uint32_t initial)
            {
                std::vector<unsigned char> bytes(stride * count, 0xa5);
                for (std::size_t index = 0; index != count; ++index)
                {
                    const auto bits = index ? Bits(300.0f) : initial;
                    std::memcpy(bytes.data() + index * stride, &vtable, sizeof(vtable));
                    std::memcpy(bytes.data() + index * stride + P::FieldOffset, &bits, sizeof(bits));
                }
                writer(host, rva, bytes.data(), bytes.size(), PAGE_READWRITE);
            };
            pool(A::PoolRva, A::PoolStride, A::PoolCapacity, image + A::VtableRva, Bits(2000.0f));
            pool(T::PoolRva, T::RowBytes, T::PoolCapacity, image + T::VtableRva, Bits(100.0f));
            pool(V::PoolRva, V::PoolStride, V::PoolCapacity, image + V::VtableRva, Bits(75.0f));
            Global(A::PoolCountRva, std::int32_t{2});
            Global(T::PoolCountRva, std::int32_t{1});
            Global(V::PoolCountRva, std::int32_t{1});
            Bind(3, Row());
            Bind(4, Row(1));
            Bind(5, image + T::PoolRva);
            Bind(6, image + V::PoolRva);
            Bind(7, Row());
            Bind(8, 0);
            constexpr std::uintptr_t foreignTable = 0x431c000;
            writer(host, foreignTable, methods.data(), sizeof(methods), PAGE_READONLY);
            Store(reinterpret_cast<std::uintptr_t>(foreign.data()), 0, image + foreignTable);
            Bind(9, reinterpret_cast<std::uintptr_t>(foreign.data()));
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        std::uintptr_t Row(std::size_t index = 0) const noexcept { return image + A::PoolRva + index * A::PoolStride; }
        void           Bind(std::uint32_t model, std::uintptr_t metadata) { Global(B::BankRva + model * B::SlotBytes, metadata); }
        std::uint32_t  BitsAt(std::size_t index = 0) const noexcept
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, reinterpret_cast<void*>(Row(index) + P::FieldOffset), sizeof(bits));
            return bits;
        }
        void                       Distance(std::uint32_t bits, std::size_t index = 0) { Store(Row(index), P::FieldOffset, bits); }
        std::vector<unsigned char> RowBytes(std::size_t index = 0) const
        {
            std::vector<unsigned char> bytes(A::PoolStride);
            std::memcpy(bytes.data(), reinterpret_cast<void*>(Row(index)), bytes.size());
            return bytes;
        }
        bool OutsideFieldUnchanged(const std::vector<unsigned char>& before, std::size_t index = 0) const
        {
            auto current = RowBytes(index), expected = before;
            std::fill_n(current.begin() + P::FieldOffset, P::FieldBytes, 0);
            std::fill_n(expected.begin() + P::FieldOffset, P::FieldBytes, 0);
            return current == expected;
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
        check(M::Capture(storage.image, generation, 8, owner, after, value) == ClockResult::Ok && !value.present && value.bits == 0,
              "a qualified null model bank association has zero current distance without write authority");
        const auto unchanged = value;
        check(M::Capture(storage.image, generation, 9, owner, after, value) == ClockResult::UnsupportedBuild && M::SameIdentity(value, unchanged),
              "an unknown present family is refused with the caller output unchanged");
        std::array<std::uintptr_t, 16> otherTime{};
        for (std::size_t index = 0; index != std::size(T::VtableMethods); ++index)
            otherTime[index] = storage.image + T::VtableMethods[index];
        otherTime[15] = storage.image + 0xace850;
        storage.writer(storage.host, 0x431c100, otherTime.data(), sizeof(otherTime), PAGE_READONLY);
        Store(reinterpret_cast<std::uintptr_t>(storage.foreign.data()), 0, storage.image + 0x431c100);
        check(M::Capture(storage.image, generation, 9, owner, after, value) == ClockResult::UnsupportedBuild,
              "a different type3 vtable cannot inherit the first Time producer's distance layout");
        Store(reinterpret_cast<std::uintptr_t>(storage.foreign.data()), 0, storage.image + A::VtableRva);
        storage.Bind(9, reinterpret_cast<std::uintptr_t>(storage.foreign.data()));
        check(M::Capture(storage.image, generation, 9, owner, after, value) != ClockResult::Ok,
              "an arbitrary readable heap row with the exact supported vtable lacks constructor-backed static storage authority");
        storage.Global(A::PoolCountRva, std::int32_t{1});
        check(M::Capture(storage.image, generation, 4, owner, after, value) != ClockResult::Ok,
              "a correctly aligned Atomic row outside the current allocated prefix cannot receive distance writes");
        storage.Global(A::PoolCountRva, std::int32_t{2});
        storage.Bind(9, storage.Row() + sizeof(std::uintptr_t));
        check(M::Capture(storage.image, generation, 9, owner, after, value) != ClockResult::Ok,
              "an interior static row address cannot borrow the surrounding typed allocation");
        storage.Bind(9, 0);
        const auto last = [&](std::uintptr_t pool, std::uintptr_t count, std::size_t capacity, std::size_t stride, std::int32_t restore)
        {
            storage.Global(count, static_cast<std::int32_t>(capacity));
            storage.Bind(9, storage.image + pool + (capacity - 1) * stride);
            check(M::Capture(storage.image, generation, 9, owner, after, value) == ClockResult::Ok && value.present &&
                      value.poolCount == static_cast<std::int32_t>(capacity),
                  "each genuine family's final constructor-backed row is admitted inside its exact published capacity");
            storage.Global(count, static_cast<std::int32_t>(capacity + 1));
            check(M::Capture(storage.image, generation, 9, owner, after, value) != ClockResult::Ok,
                  "a count beyond the independently proven family capacity cannot widen native distance authority");
            storage.Global(count, restore);
            storage.Bind(9, 0);
        };
        last(A::PoolRva, A::PoolCountRva, A::PoolCapacity, A::PoolStride, 2);
        last(T::PoolRva, T::PoolCountRva, T::PoolCapacity, T::RowBytes, 1);
        last(V::PoolRva, V::PoolCountRva, V::PoolCapacity, V::PoolStride, 1);
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::Ok && value.family == M::Family::Atomic,
              "the real normal Atomic metadata row survives independent admission refusals");
        const auto original = value;
        auto       alias = value;
        alias.model.modelIndex = 7;
        alias.model.slotAddress = storage.image + B::BankRva + 7 * B::SlotBytes;
        check(!M::SameIdentity(original, alias) && M::SameStorageIdentity(M::Identity(original), alias),
              "shared native distance storage keeps distinct bank routes and model-ID cache keys");
        const auto change = [&](auto mutate, const char* reason)
        {
            auto current = original;
            mutate(current);
            check(!M::SameStorageIdentity(M::Identity(original), current), reason);
        };
        change([](M::Value& v) { ++v.generation; }, "native distance defaults cannot cross Runtime generations");
        change([](M::Value& v) { ++v.poolCount; }, "allocated prefix count remains part of the retained storage identity");
        change([](M::Value& v) { ++v.context.animationMethod; }, "a different sibling virtual cannot renew an old distance default");
        change([](M::Value& v) { ++v.context.rootMembership.outerAddress; }, "full current root membership identity includes its outer object");
        change([](M::Value& v) { ++v.context.providerMembership.itemAddress; }, "full provider membership identity includes the actual item route");
        change([](M::Value& v) { v.family = M::Family::Vehicle; }, "a different native family cannot inherit the same numeric distance offset");
        M::Receipt raced;
        check(M::Write(storage.image, original, Bits(180.0f), owner, after, raced, &RaceExchange) == ClockResult::OwnerConflict && raced.entered &&
                  !raced.accepted && !raced.uncertain && raced.observed == 0x43800000u && storage.BitsAt() == 0x43800000u,
              "the genuine CAS refuses a competing field preimage introduced after the final raw fence without overwriting it");
        storage.Distance(original.bits);
        for (const auto selected : {std::uint32_t{5}, std::uint32_t{6}})
        {
            check(M::Capture(storage.image, generation, selected, owner, after, value) == ClockResult::Ok && value.present &&
                      value.family == (selected == 5 ? M::Family::Time : M::Family::Vehicle),
                  "the independently constructor-backed Time and Vehicle pools admit their own distance field");
            M::Receipt receipt;
            const auto before = value.bits;
            check(M::Write(storage.image, value, 0x80000000u, owner, after, receipt) == ClockResult::Ok && receipt.accepted,
                  "the other proven native families preserve the exact signed-zero Float32 bits");
            check(M::Capture(storage.image, generation, selected, owner, after, value) == ClockResult::Ok && value.bits == 0x80000000u &&
                      M::Write(storage.image, value, before, owner, after, receipt) == ClockResult::Ok,
                  "freshly borrowed typed fields can restore their exact original representation");
        }
        const auto originalThreshold = P::Threshold;
        auto       changedThreshold = originalThreshold;
        changedThreshold[0] ^= 1;
        storage.writer(storage.host, P::ThresholdRva, changedThreshold.data(), changedThreshold.size(), PAGE_READONLY);
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::UnsupportedBuild,
              "changing the immutable distance-consumer threshold revokes the source-qualified field authority");
        storage.writer(storage.host, P::ThresholdRva, originalThreshold.data(), originalThreshold.size(), PAGE_READONLY);
        auto changedConsumer = P::DistanceConsumer;
        changedConsumer.back() ^= 1;
        storage.writer(storage.host, P::DistanceConsumerRva, changedConsumer.data(), changedConsumer.size(), PAGE_EXECUTE_READ);
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::UnsupportedBuild,
              "drift in the complete independent distance consumer revokes the same current field authority");
        storage.writer(storage.host, P::DistanceConsumerRva, P::DistanceConsumer.data(), P::DistanceConsumer.size(), PAGE_EXECUTE_READ);
    }
    template <class Check>
    void KnownAbsentPrefix(Storage& storage, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner after, Check check)
    {
        namespace F = ClockModelLodMutationTest;
        const auto                         first = storage.BitsAt(), second = storage.BitsAt(1);
        mta::game_de::NativeModelLodDomain domain;
        check(domain.Set(storage.image, generation, 3, Bits(160.0f), owner, after) == ClockResult::Ok &&
                  domain.Set(storage.image, generation, 4, Bits(170.0f), owner, after) == ClockResult::Ok,
              "independent model-ID defaults prepare an absent-prefix restoration failure");
        storage.Bind(3, 0);
        storage.Distance(Bits(171.0f), 1);
        const auto counts = F::exchanges;
        check(domain.ResetAll(storage.image, generation, owner, after) == ClockResult::OwnerConflict && domain.Pending() && F::exchanges == counts,
              "a genuinely absent first key is acknowledged without native entry while foreign suffix drift retains the default map");
        storage.Bind(3, storage.Row());
        storage.Distance(Bits(170.0f), 1);
        check(domain.ResetAll(storage.image, generation, owner, after) == ClockResult::OwnerConflict && domain.Pending() && F::exchanges == counts,
              "a later reappearing row cannot inherit a previous absent acknowledgement or blindly replay its original");
        check(domain.Set(storage.image, generation, 3, Bits(160.0f), owner, after) == ClockResult::Ok &&
                  domain.ResetAll(storage.image, generation, owner, after) == ClockResult::Ok && !domain.Pending() && storage.BitsAt() == first &&
                  storage.BitsAt(1) == second,
              "an explicit freshly validated Set renews the reappeared key and permits a healthy ordered restoration");
    }
    template <class Check>
    void KnownPrefix(Storage& storage, std::uint64_t generation, M::CheckOwner owner, M::CheckOwner after, Check check)
    {
        namespace F = ClockModelLodMutationTest;
        const auto first = storage.BitsAt(), second = storage.BitsAt(1);
        Gate       gate{owner, after};
        Gate::current = &gate;
        M::Value value;
        check(M::Capture(storage.image, generation, 3, owner, after, value) == ClockResult::Ok, "prefix calibration borrows the actual current distance row");
        F::afterExchange = &Gate::Began;
        M::Receipt receipt;
        check(M::Write(storage.image, value, first, &Gate::Owner, &Gate::After, receipt) == ClockResult::Ok && receipt.accepted,
              "same-bits genuine CAS measures the real successful post-acknowledgement owner callbacks");
        const auto postCalls = gate.calls;
        check(postCalls > 0, "the actual distance write rechecks its owner after acknowledgement");
        F::afterExchange = nullptr;
        mta::game_de::NativeModelLodDomain domain;
        check(domain.Set(storage.image, generation, 3, Bits(50.0f), owner, after) == ClockResult::Ok &&
                  domain.Set(storage.image, generation, 7, Bits(100.0f), owner, after) == ClockResult::Ok &&
                  domain.Set(storage.image, generation, 4, Bits(200.0f), owner, after) == ClockResult::Ok && domain.Pending(),
              "two model-ID aliases and an independent row retain separate first defaults in real SDK key order");
        const auto counts = F::exchanges;
        gate.calls = 0;
        gate.limit = postCalls;
        gate.counting = false;
        F::afterExchange = &Gate::Began;
        check(domain.ResetAll(storage.image, generation, &Gate::Owner, &Gate::After) != ClockResult::Ok && domain.Pending() && !domain.Uncertain() &&
                  storage.BitsAt() == first && storage.BitsAt(1) == Bits(200.0f) && F::exchanges[0] == counts[0] + 1 && F::exchanges[1] == counts[1],
              "an acknowledged first-ID restore survives a known pre-entry refusal of the next ordered ID without entering its CAS");
        F::afterExchange = nullptr;
        gate.counting = false;
        const auto accepted = F::exchanges[0];
        check(domain.ResetAll(storage.image, generation, owner, after) == ClockResult::Ok && !domain.Pending() && domain.CanDiscard() &&
                  storage.BitsAt() == Bits(50.0f) && storage.BitsAt(1) == second && F::exchanges[0] == accepted + 1 && F::exchanges[1] == counts[1] + 1,
              "retry does not replay the accepted prefix and applies the later alias's distinct original in ascending model-ID order");
        // The local journal's final alias value is intentional. Restore the
        // fixture's no-longer-owned setup bytes before independent scenarios.
        storage.Distance(first);
        Gate::current = nullptr;
    }
}
