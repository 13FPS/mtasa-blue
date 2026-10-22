/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned native-shaped typed world graph shared by primitive/owner fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeStreamingLoadAllProfile.h"
#include "../../Client/game_de/NativeStartupOwnerProfile.h"
#include "../../Client/game_de/NativeStartupWorldProfile.h"
#include <array>
#include <cstring>
namespace NativeStreamingLoadAllFixture
{
    struct Graph
    {
        using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
        alignas(std::uintptr_t) std::array<unsigned char, 0x2c8> game{};
        alignas(std::uintptr_t) std::array<unsigned char, 0xd8> level{};
        alignas(std::uintptr_t) std::array<unsigned char, 0x5e8> world{};
        alignas(std::uintptr_t) std::array<std::array<unsigned char, 0x40>, 7> classes{};
        alignas(std::uintptr_t) std::array<unsigned char, 11 * 24> items{};
        std::array<std::uintptr_t, 1> chunks{};
        std::array<std::uintptr_t, 3> gameChain{}, levelChain{};
        std::uintptr_t                image = 0, root = 0;
        Writer                        writer = nullptr;
        void*                         host = nullptr;
        Graph(std::uintptr_t selectedImage, std::uintptr_t selectedRoot, Writer selectedWriter, void* selectedHost)
            : image(selectedImage), root(selectedRoot), writer(selectedWriter), host(selectedHost)
        {
            Reset();
        }
        template <class T>
        static void Put(void* object, std::size_t offset, const T& value)
        {
            std::memcpy(static_cast<unsigned char*>(object) + offset, &value, sizeof(value));
        }
        template <class T>
        void Global(std::uintptr_t rva, const T& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        std::uintptr_t Game() const { return reinterpret_cast<std::uintptr_t>(game.data()); }
        std::uintptr_t Level() const { return reinterpret_cast<std::uintptr_t>(level.data()); }
        std::uintptr_t World() const { return reinterpret_cast<std::uintptr_t>(world.data()); }
        std::uintptr_t Class(std::size_t index) const { return reinterpret_cast<std::uintptr_t>(classes[index].data()); }
        void           Reset()
        {
            game.fill(0);
            level.fill(0);
            world.fill(0);
            items.fill(0);
            for (auto& type : classes)
                type.fill(0);
            const std::array<std::uintptr_t, 11> objects{root, Game(), Level(), World(), Class(0), Class(1), Class(2), Class(3), Class(4), Class(5), Class(6)};
            for (std::size_t i = 0; i != objects.size(); ++i)
            {
                auto* object = reinterpret_cast<void*>(objects[i]);
                Put(object, 0,
                    image + (i == 0 ? mta::game_de::NativeStreamingLoadAllProfile::RootVtableRva : mta::game_de::NativeStartupWorldProfile::ownerVtableRva));
                Put(object, 8, std::uint32_t{0});
                Put(object, 0xc, static_cast<std::int32_t>(i));
                Put(object, 0x10, Class(0));
                Put(object, 0x20, std::uintptr_t{0});
                Put(items.data(), i * 24, objects[i]);
                Put(items.data(), i * 24 + 0x10, static_cast<std::uint32_t>(i + 100));
            }
            Put(reinterpret_cast<void*>(root), 0x10, Class(1));
            Put(reinterpret_cast<void*>(root), 0x20, Game());
            Put(game.data(), 0x10, Class(3));
            Put(game.data(), 0x20, Level());
            Put(game.data(), 0x2c0, root);
            Put(level.data(), 0x10, Class(5));
            Put(level.data(), 0xd0, World());
            Put(world.data(), 0x10, Class(6));
            gameChain = {0, Class(2) + 0x30, Class(3) + 0x30};
            levelChain = {0, Class(4) + 0x30, Class(5) + 0x30};
            Put(classes[2].data(), 0x38, std::int32_t{1});
            Put(classes[4].data(), 0x38, std::int32_t{1});
            Put(classes[3].data(), 0x30, reinterpret_cast<std::uintptr_t>(gameChain.data()));
            Put(classes[3].data(), 0x38, std::int32_t{2});
            Put(classes[5].data(), 0x30, reinterpret_cast<std::uintptr_t>(levelChain.data()));
            Put(classes[5].data(), 0x38, std::int32_t{2});
            // Raw UClass membership intentionally admits flags0x10; the typed
            // subtype helper consumes its own +30/+38 metadata separately.
            Put(classes[2].data(), 8, std::uint32_t{0x10});
            Put(classes[4].data(), 8, std::uint32_t{0x10});
            chunks[0] = reinterpret_cast<std::uintptr_t>(items.data());
            Global(mta::game_de::NativeStreamingLoadAllProfile::RootRva, root);
            Global(mta::game_de::NativeStartupOwnerProfile::gameModeClassCacheRva, Class(2));
            Global(mta::game_de::NativeStartupWorldProfile::levelClassCacheRva, Class(4));
            Global(mta::game_de::NativeStartupWorldProfile::objectChunksRva, reinterpret_cast<std::uintptr_t>(chunks.data()));
            Global(mta::game_de::NativeStartupWorldProfile::objectCountRva, std::int32_t{11});
            Global(mta::game_de::NativeStreamingLoadAllProfile::RootWorldSlotRva, image + mta::game_de::NativeStreamingLoadAllProfile::RootWorldRva,
                   PAGE_READONLY);
            Global(mta::game_de::NativeStartupWorldProfile::ownerGetWorldSlotRva, image + mta::game_de::NativeStartupWorldProfile::getterRva, PAGE_READONLY);
        }
    };
}
