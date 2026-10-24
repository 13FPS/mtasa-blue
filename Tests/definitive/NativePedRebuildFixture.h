/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned native-shaped rebuild receivers shared by primitive/owner tests
 ****************************************************************************/
#pragma once
#include "NativePedClothesFixture.h"
#include "../../Client/game_de/NativePedRebuildProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativePedIntelligenceProfile.h"
#include "../../Client/game_de/NativeTaskPoolProfile.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include "../../Client/game_de/NativeStreamingRequestProfile.h"
#include <array>
#include <cstring>
namespace NativePedRebuildFixture
{
    using Writer = NativePedClothesFixture::Writer;
    namespace P = mta::game_de::NativePedRebuildProfile;
    namespace I = mta::game_de::NativePedIntelligenceProfile;
    namespace T = mta::game_de::NativeTaskPoolProfile;
    inline constexpr std::uintptr_t RenderVtableRva = 0x44ff000, RenderEventRva = 0x1100030, TypeRva = 0x1100010;
    template <class V>
    inline void Store(void* address, std::size_t offset, const V& value)
    {
        std::memcpy(static_cast<unsigned char*>(address) + offset, &value, sizeof(value));
    }
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        NativePedClothesFixture::Install(image, writer, host);
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePedMetadataProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : I::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : T::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeModelLoadedProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto group = image + P::GroupRva;
        const auto branch = static_cast<std::uint32_t>(P::BranchRva);
        writer(host, P::GroupSlotRva, &group, sizeof(group), PAGE_READONLY);
        writer(host, P::TableSlotRva, &branch, sizeof(branch), PAGE_EXECUTE_READ);
        for (const auto& slot : P::Virtuals)
        {
            const auto target = image + slot.target;
            writer(host, P::PlayerVtableRva + slot.slot, &target, sizeof(target), PAGE_READONLY);
            unsigned char            first = 0xc3;
            MEMORY_BASIC_INFORMATION page{};
            // Only owned fixture image pages are inspected. Preserve a real
            // shared pin already installed by an earlier fixture, while making
            // dispatch-only unused targets executable without invoking them.
            if (VirtualQuery(reinterpret_cast<void*>(target), &page, sizeof(page)) == sizeof(page) && page.State == MEM_COMMIT &&
                !(page.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                std::memcpy(&first, reinterpret_cast<void*>(target), 1);
            writer(host, slot.target, &first, 1, PAGE_EXECUTE_READ);
        }
        std::array<std::uintptr_t, 0x220 / sizeof(std::uintptr_t)> table{};
        table[0] = table[0x218 / sizeof(std::uintptr_t)] = image + RenderEventRva;
        writer(host, RenderVtableRva, table.data(), sizeof(table), PAGE_READONLY);
        const unsigned char ret = 0xc3;
        writer(host, RenderEventRva, &ret, 1, PAGE_EXECUTE_READ);
        const std::array<unsigned char, 6> type{0xb0, 5, 0xc3, 0xcc, 0xcc, 0xcc};
        writer(host, TypeRva, type.data(), type.size(), PAGE_EXECUTE_READ);
        for (const auto& typeSlot : T::Slots)
        {
            const auto destroy = image + typeSlot.deletingDestructor;
            const auto getType = image + typeSlot.getType;
            const auto simple = image + typeSlot.isSimple;
            const auto abort = image + RenderEventRva;
            writer(host, typeSlot.vtable, &destroy, sizeof(destroy), PAGE_READONLY);
            writer(host, typeSlot.vtable + 0x18, &simple, sizeof(simple), PAGE_READONLY);
            writer(host, typeSlot.vtable + 0x20, &getType, sizeof(getType), PAGE_READONLY);
            writer(host, typeSlot.vtable + 0x30, &abort, sizeof(abort), PAGE_READONLY);
        }
    }

    struct State
    {
        alignas(std::uintptr_t) std::array<std::array<unsigned char, 0x400>, 2> render{};
        alignas(std::uintptr_t) std::array<std::array<unsigned char, P::ContainerBytes>, 2> containers{};
        alignas(std::uintptr_t) std::array<unsigned char, P::HeadBytes> head{};
        // Keep the engine's 0x34-byte record view while making the fixture's
        // pointer-aligned backing storage explicit. MSVC /W4 otherwise reports
        // the implicit gap before the next aligned header as C4324.
        static constexpr std::size_t StatStorageBytes = (P::PedStatStride + alignof(std::uintptr_t) - 1) / alignof(std::uintptr_t) * alignof(std::uintptr_t);
        alignas(std::uintptr_t) std::array<unsigned char, StatStorageBytes> statRecord{};
        alignas(std::uintptr_t) std::array<unsigned char, 32> intelligenceHeader{}, taskHeader{};
        unsigned char* intelligenceObjects =
            static_cast<unsigned char*>(VirtualAlloc(nullptr, I::NativeCapacity* I::ObjectStride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        unsigned char* intelligenceFlags = static_cast<unsigned char*>(VirtualAlloc(nullptr, I::NativeCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        unsigned char* taskObjects =
            static_cast<unsigned char*>(VirtualAlloc(nullptr, T::NativeCapacity* T::ObjectStride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        unsigned char* taskFlags = static_cast<unsigned char*>(VirtualAlloc(nullptr, T::NativeCapacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        unsigned       active = 0;
        // Explicit tail storage keeps the enclosing pointer-aligned fixture
        // free of compiler-inserted padding; this is no native record field.
        std::uint32_t reserved = 0;
        State() = default;
        State(const State&) = delete;
        State& operator=(const State&) = delete;
        ~State()
        {
            for (auto* pointer : {intelligenceObjects, intelligenceFlags, taskObjects, taskFlags})
                if (pointer)
                    VirtualFree(pointer, 0, MEM_RELEASE);
        }
        bool           Ready() const noexcept { return intelligenceObjects && intelligenceFlags && taskObjects && taskFlags; }
        std::uintptr_t Render() const noexcept { return reinterpret_cast<std::uintptr_t>(render[active].data()); }
        std::uintptr_t Container() const noexcept { return reinterpret_cast<std::uintptr_t>(containers[active].data()); }
        std::uintptr_t Head() const noexcept { return reinterpret_cast<std::uintptr_t>(head.data()); }
        void           Recreate(std::uintptr_t image, std::uintptr_t ped)
        {
            active = 1 - active;
            render[active].fill(0);
            containers[active].fill(0);
            Store(render[active].data(), 0, image + RenderVtableRva);
            // Deliberately no UObject registry index/serial is fabricated.
            // The production primitive borrows the proved native model owner
            // and concrete container/head relationship, not guessed headers.
            Store(render[active].data(), P::ContainerOffset, Container());
            Store(containers[active].data(), 0, Head());
            Store(containers[active].data(), P::ContainerOwnerOffset, ped + P::PedOwnerFieldOffset);
            Store(head.data(), P::HeadBacklinkOffset, Container());
            Store(reinterpret_cast<void*>(ped), P::RenderOffset, Render());
        }
        // The live registry argument preserves one fixture call shape with
        // existing shared graphs. These native render objects deliberately do
        // not register invented UObject identities in that unrelated array.
        void Reset(std::uintptr_t image, std::uintptr_t ped, std::uintptr_t modelInfo, std::uintptr_t, Writer writer, void* host)
        {
            Install(image, writer, host);
            active = 1;
            render[0].fill(0);
            render[1].fill(0);
            containers[0].fill(0);
            containers[1].fill(0);
            head.fill(0);
            statRecord.fill(0);
            intelligenceHeader.fill(0);
            taskHeader.fill(0);
            std::memset(intelligenceObjects, 0, I::NativeCapacity * I::ObjectStride);
            std::memset(intelligenceFlags, 0x80, I::NativeCapacity);
            std::memset(taskObjects, 0, T::NativeCapacity * T::ObjectStride);
            std::memset(taskFlags, 0x80, T::NativeCapacity);
            intelligenceFlags[0] = 1;
            Store(intelligenceHeader.data(), 0, reinterpret_cast<std::uintptr_t>(intelligenceObjects));
            Store(intelligenceHeader.data(), 8, reinterpret_cast<std::uintptr_t>(intelligenceFlags));
            Store(intelligenceHeader.data(), 0x10, I::NativeCapacity);
            Store(intelligenceHeader.data(), 0x14, std::int32_t{-1});
            Store(intelligenceHeader.data(), 0x18, static_cast<unsigned char>(1));
            Store(intelligenceObjects, 0, ped);
            Store(intelligenceObjects, 0x60, ped);
            Store(reinterpret_cast<void*>(ped), I::IntelligenceOffset, reinterpret_cast<std::uintptr_t>(intelligenceObjects));
            const auto intelligence = reinterpret_cast<std::uintptr_t>(intelligenceHeader.data());
            writer(host, I::RootRva, &intelligence, sizeof(intelligence), PAGE_READWRITE);
            Store(taskHeader.data(), 0, reinterpret_cast<std::uintptr_t>(taskObjects));
            Store(taskHeader.data(), 8, reinterpret_cast<std::uintptr_t>(taskFlags));
            Store(taskHeader.data(), 0x10, static_cast<std::int32_t>(T::NativeCapacity));
            Store(taskHeader.data(), 0x14, std::int32_t{-1});
            Store(taskHeader.data(), 0x18, static_cast<unsigned char>(1));
            const auto taskPool = reinterpret_cast<std::uintptr_t>(taskHeader.data());
            writer(host, T::RootRva, &taskPool, sizeof(taskPool), PAGE_READWRITE);
            Store(reinterpret_cast<void*>(modelInfo), P::ModelReferenceOffset, std::uint16_t{1});
            Store(reinterpret_cast<void*>(modelInfo), P::ModelPedStatOffset, std::int32_t{0});
            const auto bank = reinterpret_cast<std::uintptr_t>(statRecord.data());
            writer(host, P::PedStatsBankRva, &bank, sizeof(bank), PAGE_READWRITE);
            Store(reinterpret_cast<void*>(ped), P::PedStatOffset, bank);
            Store(reinterpret_cast<void*>(ped), P::RetainedObjectOffset, std::uintptr_t{0});
            Store(reinterpret_cast<void*>(ped), mta::game_de::NativePedMetadataProfile::ModelOffset, std::uint16_t{0});
            const float stat = 25.0f;
            writer(host, P::FirstStatRva, &stat, sizeof(stat), PAGE_READWRITE);
            writer(host, P::SecondStatRva, &stat, sizeof(stat), PAGE_READWRITE);
            std::array<unsigned char, P::ClothesBytes> global{};
            writer(host, P::GlobalClothesRva, global.data(), global.size(), PAGE_READWRITE);
            const std::array<unsigned char, 2> loaded{0x40, 1};
            writer(host, mta::game_de::NativeModelLoadedProfile::ArrayRva + mta::game_de::NativeModelLoadedProfile::FlagsOffset, loaded.data(), loaded.size(),
                   PAGE_READWRITE);
            writer(host, mta::game_de::NativeModelBankProfile::BankRva, &modelInfo, sizeof(modelInfo), PAGE_READWRITE);
            const auto type = image + TypeRva;
            writer(host, 0x4301000 + mta::game_de::NativeStreamingRequestProfile::TypeMethodSlot, &type, sizeof(type), PAGE_READONLY);
            Recreate(image, ped);
        }
        void Secondary(std::uintptr_t image, std::size_t type = 0)
        {
            taskFlags[0] = 1;
            Store(taskObjects, 0, image + T::Slots[type].vtable);
            Store(intelligenceObjects, 0x58, reinterpret_cast<std::uintptr_t>(taskObjects));
        }
    };
}
