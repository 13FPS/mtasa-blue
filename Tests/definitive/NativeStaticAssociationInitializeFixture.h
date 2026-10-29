/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared owned clump/frame/map inputs for the actual initializer boundary
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeStaticAssociationInitializeCalls.h"
#include "../../Client/game_de/NativeStaticAssociationInitializeProfile.h"
#include "../../Client/game_de/NativeCustomAnimSequenceStorageProfile.h"
#include "../../Client/game_de/NativeModelReferenceTransactionProfile.h"
#include "../../Client/game_de/NativeAnimationHierarchyReadProfile.h"
#include "../../Client/game_de/NativeAnimationManagerProfile.h"
#include "../../Client/game_de/NativePedMetadataProfile.h"
#include "../../Client/game_de/NativePedRenderProfile.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include "../../Client/game_de/NativeStartupProvidersProfile.h"
#include <windows.h>
#include <array>
#include <cstring>
namespace NativeStaticAssociationInitializeFixture
{
    namespace P = mta::game_de::NativeStaticAssociationInitializeProfile;
    namespace S = mta::game_de::NativeCustomAnimSequenceStorageProfile;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    inline constexpr std::uintptr_t RenderVtableRva = 0x44ff000, EventRva = 0x1100030, ModelTypeRva = 0x1100010;
    template <class T>
    inline void Store(void* data, std::size_t offset, const T& value)
    {
        std::memcpy(static_cast<unsigned char*>(data) + offset, &value, sizeof(value));
    }
    template <class T>
    inline T Load(const void* data, std::size_t offset)
    {
        T value{};
        std::memcpy(&value, static_cast<const unsigned char*>(data) + offset, sizeof(value));
        return value;
    }
    inline void Install(std::uintptr_t image, Writer writer, void* host)
    {
        for (const auto& pin : P::Pins)
            writer(host, pin.rva, pin.bytes, pin.size, pin.code || pin.rva < 0x3f40000 ? PAGE_EXECUTE_READ : PAGE_READONLY);
        for (const auto& pin : S::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        // The inherited allocation fixture publishes reallocate/quantize and
        // malloc slots. Original static Init additionally owns native node
        // release, so its positive fixture must publish both real free slots.
        const auto authorityFree = image + S::AuthorityFreeRva, proxyFree = image + S::ProxyFreeRva;
        writer(host, mta::game_de::NativeModelReferenceTransactionProfile::HeapVtableRva + 0x30, &authorityFree, sizeof(authorityFree), PAGE_READONLY);
        writer(host, mta::game_de::NativeModelReferenceTransactionProfile::ProxyVtableRva + 0x30, &proxyFree, sizeof(proxyFree), PAGE_READONLY);
        for (const auto& pin : mta::game_de::NativeAnimationHierarchyReadProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeAnimationManagerProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePedMetadataProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativePedRenderProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        for (const auto& pin : mta::game_de::NativeModelLoadedProfile::Code)
            writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
        const auto& fold = mta::game_de::NativeAnimationManagerProfile::NameFold;
        writer(host, mta::game_de::NativeAnimationManagerProfile::NameFoldRva, fold.data(), fold.size(), PAGE_READONLY);
        std::array<std::uintptr_t, 0x6d0 / sizeof(std::uintptr_t)> table{};
        table[0] = table[0x218 / sizeof(std::uintptr_t)] = image + EventRva;
        table[0x698 / sizeof(std::uintptr_t)] = image + P::NameProviderRva;
        table[0x6c8 / sizeof(std::uintptr_t)] = image + P::SkeletonProviderRva;
        writer(host, RenderVtableRva, table.data(), sizeof(table), PAGE_READONLY);
        const unsigned char ret = 0xc3;
        writer(host, EventRva, &ret, 1, PAGE_EXECUTE_READ);
        const std::array<unsigned char, 6> type{0xb0, 5, 0xc3, 0xcc, 0xcc, 0xcc};
        writer(host, ModelTypeRva, type.data(), type.size(), PAGE_EXECUTE_READ);
        const auto imported = reinterpret_cast<std::uintptr_t>(GetProcAddress(GetModuleHandleW(L"VCRUNTIME140.dll"), "strstr"));
        writer(host, P::StrstrImportRva, &imported, sizeof(imported), PAGE_READONLY);
        const std::uintptr_t zero = 0;
        writer(host, P::NameFallbackControlRva, &zero, sizeof(zero), PAGE_READWRITE);
    }
    struct State
    {
        alignas(std::uintptr_t) std::array<unsigned char, 0x310> render{};
        alignas(std::uintptr_t) std::array<unsigned char, P::ContainerBytes> container{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x40> actorClass{}, renderClass{};
        std::array<std::uintptr_t, 2> classRows{};
        alignas(std::uintptr_t) std::array<unsigned char, 8 + 2 * S::RowBytes> sequences{};
        std::array<std::uintptr_t, 2> nodes{};
        std::array<std::int32_t, 4>   mapRow{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x480> component{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x40> skeleton{};
        alignas(std::uintptr_t) std::array<unsigned char, 256 * 24> registryItems{};
        std::array<std::uintptr_t, 1> registryChunks{};
        std::size_t                   actorClassIndex = 0, renderClassIndex = 0, renderIndex = 0, componentIndex = 0, skeletonIndex = 0;
        std::uintptr_t                image = 0, ped = 0, association = 0, hierarchy = 0;
        std::uintptr_t                Clump() const noexcept { return reinterpret_cast<std::uintptr_t>(render.data()); }
        std::uintptr_t                Container() const noexcept { return reinterpret_cast<std::uintptr_t>(container.data()); }
        std::uintptr_t Sequence(std::size_t index) const noexcept { return reinterpret_cast<std::uintptr_t>(sequences.data()) + 8 + index * S::RowBytes; }
        std::uintptr_t Nodes() const noexcept { return reinterpret_cast<std::uintptr_t>(nodes.data()); }
        // The host supplies already-qualified same-bank physical row/H and its
        // actual block index. This helper does not invent their registration,
        // loaded range, a native owner, or a hidden hierarchy lifetime counter.
        void Reset(std::uintptr_t currentImage, std::uintptr_t currentPed, std::uintptr_t currentAssociation, std::uintptr_t currentHierarchy, Writer writer,
                   void* host, std::uint32_t blockId = 1)
        {
            image = currentImage;
            ped = currentPed;
            association = currentAssociation;
            hierarchy = currentHierarchy;
            render.fill(0);
            container.fill(0);
            actorClass.fill(0);
            renderClass.fill(0);
            sequences.fill(0);
            nodes.fill(0);
            component.fill(0);
            skeleton.fill(0);
            registryItems.fill(0);
            registryChunks.fill(0);
            actorClassIndex = renderClassIndex = renderIndex = componentIndex = skeletonIndex = 0;
            Install(image, writer, host);
            const auto baseClass = reinterpret_cast<std::uintptr_t>(actorClass.data());
            classRows = {baseClass + 0x30, reinterpret_cast<std::uintptr_t>(renderClass.data()) + 0x30};
            Store(actorClass.data(), 0x38, std::int32_t{0});
            Store(renderClass.data(), 0x30, reinterpret_cast<std::uintptr_t>(classRows.data()));
            Store(renderClass.data(), 0x38, std::int32_t{1});
            writer(host, P::ActorClassSlotRva, &baseClass, sizeof(baseClass), PAGE_READWRITE);
            Store(render.data(), 0, image + RenderVtableRva);
            Store(render.data(), 0x10, reinterpret_cast<std::uintptr_t>(renderClass.data()));
            Store(render.data(), 0x300, Container());
            Store(render.data(), 0x308, image + P::CleanupRva);
            Store(reinterpret_cast<void*>(ped), 0x20, Clump());
            Store(container.data(), 0x10, std::int32_t{2});
            Store(container.data(), 0x18, Clump());
            Store(container.data(), 0x20, ped + 0x6e4);
            Store(container.data(), 0x988, std::int32_t{2});
            Store(container.data(), 0x98c, std::int32_t{75});
            Store(container.data(), 0x40, std::uint32_t{0});
            Store(container.data(), 0x60, std::uint32_t{1});
            Store(container.data(), 0x11cc, std::int32_t{0});
            Store(container.data(), 0x11d0, std::int32_t{1});
            // One real-shaped sparse row maps signed bone7 to frame0. Name
            // layout occupies the other sequence. No matcher runs in this fixture.
            mapRow = {7, 0, -1, 0};
            Store(container.data(), 0x1438, reinterpret_cast<std::uintptr_t>(mapRow.data()));
            Store(container.data(), 0x1440, std::int32_t{1});
            Store(container.data(), 0x1444, std::int32_t{1});
            Store(container.data(), 0x1448, std::uint32_t{1});
            Store(container.data(), 0x1460, std::int32_t{1});
            Store(container.data(), 0x1464, std::int32_t{128});
            Store(container.data(), 0x1468, std::int32_t{-1});
            Store(container.data(), 0x1470, std::int32_t{0});
            Store(container.data(), 0x1480, std::int32_t{1});
            Store(sequences.data(), 0, std::uint64_t{2});
            Store(reinterpret_cast<void*>(Sequence(0)), S::BoneOffset, std::int16_t{7});
            Store(reinterpret_cast<void*>(Sequence(0)), S::FlagsOffset, std::uint16_t{0x10});
            Store(reinterpret_cast<void*>(Sequence(0)), S::FrameCountOffset, std::int16_t{1});
            const char name[] = "original_dummy_event";
            std::memcpy(reinterpret_cast<void*>(Sequence(1)), name, sizeof(name));
            Store(reinterpret_cast<void*>(Sequence(1)), S::FrameCountOffset, std::int16_t{1});
            std::array<unsigned char, 0x28> hierarchyRow{};
            Store(hierarchyRow.data(), 8, Sequence(0));
            Store(hierarchyRow.data(), 0x10, std::uint16_t{2});
            Store(hierarchyRow.data(), 0x14, blockId);
            writer(host, hierarchy - image, hierarchyRow.data(), hierarchyRow.size(), PAGE_READWRITE);
            std::array<unsigned char, 32> row{};
            Store(row.data(), 0, image + P::StaticVtableRva);
            Store(row.data(), 8, std::uint16_t{0xcafe});
            Store(row.data(), 0x0a, std::int16_t{17});
            Store(row.data(), 0x0c, std::int16_t{1});
            Store(row.data(), 0x0e, std::int16_t{0x24});
            Store(row.data(), 0x18, hierarchy);
            std::memcpy(reinterpret_cast<void*>(association), row.data(), row.size());
        }
        void EnableSkeletonInputs() noexcept
        {
            Store(render.data(), 0x298, reinterpret_cast<std::uintptr_t>(component.data()));
            Store(component.data(), 0x478, reinterpret_cast<std::uintptr_t>(skeleton.data()));
        }
        // Append to the host's one real-shaped registry rather than publish a
        // competing authority. Existing world/model/provider members keep their
        // exact indexes/items. These fixture headers grant no real engine lease.
        bool AttachRegistry(const void* existingItems, std::size_t existingCount, std::uintptr_t metaClass, Writer writer, void* host)
        {
            const bool        includeSkeleton = Load<std::uintptr_t>(render.data(), 0x298) != 0;
            const std::size_t extra = includeSkeleton ? 5 : 3;
            if (!existingItems || !metaClass || existingCount > 256 - extra)
                return false;
            registryItems.fill(0);
            std::memcpy(registryItems.data(), existingItems, existingCount * 24);
            const auto metaVtable = Load<std::uintptr_t>(reinterpret_cast<void*>(metaClass), 0);
            actorClassIndex = existingCount;
            renderClassIndex = existingCount + 1;
            renderIndex = existingCount + 2;
            const auto add = [&](void* object, std::size_t index, std::uintptr_t objectClass, bool isRender)
            {
                const auto address = reinterpret_cast<std::uintptr_t>(object);
                Store(object, 0, isRender ? image + RenderVtableRva : metaVtable);
                Store(object, 8, std::uint32_t{0});
                Store(object, 0x0c, static_cast<std::int32_t>(index));
                Store(object, 0x10, objectClass);
                Store(object, 0x20, std::uintptr_t{0});
                Store(registryItems.data(), index * 24, address);
                Store(registryItems.data(), index * 24 + 0x10, static_cast<std::uint32_t>(1000 + index));
            };
            add(actorClass.data(), actorClassIndex, metaClass, false);
            add(renderClass.data(), renderClassIndex, metaClass, false);
            add(render.data(), renderIndex, reinterpret_cast<std::uintptr_t>(renderClass.data()), true);
            if (includeSkeleton)
            {
                componentIndex = existingCount + 3;
                skeletonIndex = existingCount + 4;
                add(component.data(), componentIndex, metaClass, false);
                add(skeleton.data(), skeletonIndex, metaClass, false);
            }
            registryChunks[0] = reinterpret_cast<std::uintptr_t>(registryItems.data());
            const auto chunks = reinterpret_cast<std::uintptr_t>(registryChunks.data());
            const auto count = static_cast<std::int32_t>(existingCount + extra);
            writer(host, mta::game_de::NativeStartupProvidersProfile::objectChunksRva, &chunks, sizeof(chunks), PAGE_READWRITE);
            writer(host, mta::game_de::NativeStartupProvidersProfile::objectCountRva, &count, sizeof(count), PAGE_READWRITE);
            return true;
        }
        unsigned char* RenderItem() noexcept { return registryItems.data() + renderIndex * 24; }
        unsigned char* RenderClassItem() noexcept { return registryItems.data() + renderClassIndex * 24; }
        unsigned char* ComponentItem() noexcept { return registryItems.data() + componentIndex * 24; }
        unsigned char* SkeletonItem() noexcept { return registryItems.data() + skeletonIndex * 24; }
        // Stand-in effects belong solely to injected Invoke. The production
        // boundary still qualifies real bytes/readers and never chooses nodes.
        void AcceptInjectedResult() noexcept
        {
            nodes = {Sequence(0), Sequence(1)};
            Store(reinterpret_cast<void*>(association), 8, std::uint16_t{2});
            Store(reinterpret_cast<void*>(association), 0x10, Nodes());
        }
    };
}
