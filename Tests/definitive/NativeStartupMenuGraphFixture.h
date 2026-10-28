/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned registered widget graph for actual startup menu controllers
 ****************************************************************************/
#pragma once
#include "NativeStreamingLoadAllGraphFixture.h"
#include "../../Client/game_de/NativeStartupMenuProfile.h"
#include <array>
#include <cstring>

namespace NativeStartupMenuFixture
{
    namespace P = mta::game_de::NativeStartupMenuProfile;
    namespace M = mta::game_de::NativeFrontendMenuProfile;
    struct Graph
    {
        NativeStreamingLoadAllFixture::Graph& world;
        alignas(std::uintptr_t) std::array<unsigned char, 0x40> widget{};
        alignas(std::uintptr_t) std::array<unsigned char, 12 * 24> items{};
        std::array<std::uintptr_t, 1>   chunks{};
        static constexpr std::uintptr_t WidgetVtableRva = 0x4302000;
        explicit Graph(NativeStreamingLoadAllFixture::Graph& world) : world(world) { Reset(); }
        template <class T>
        static void Put(void* object, std::size_t offset, const T& value)
        {
            NativeStreamingLoadAllFixture::Graph::Put(object, offset, value);
        }
        template <class T>
        T Root(std::size_t offset) const
        {
            T value{};
            std::memcpy(&value, reinterpret_cast<const unsigned char*>(world.root) + offset, sizeof(value));
            return value;
        }
        template <class T>
        void Root(std::size_t offset, const T& value)
        {
            Put(reinterpret_cast<void*>(world.root), offset, value);
        }
        std::uintptr_t Widget() const { return reinterpret_cast<std::uintptr_t>(widget.data()); }
        void           InitializePins()
        {
            for (const auto& pin : M::pins)
                world.writer(world.host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            const unsigned char ret = 0xc3;
            for (const auto& target : M::vtableTargets)
            {
                const auto pointer = world.image + target.targetRva;
                world.writer(world.host, target.targetRva, &ret, 1, PAGE_EXECUTE_READ);
                world.writer(world.host, target.slotRva, &pointer, sizeof(pointer), PAGE_READONLY);
            }
            for (const auto& target : M::imagePointers)
            {
                const auto pointer = world.image + target.targetRva;
                world.writer(world.host, target.slotRva, &pointer, sizeof(pointer), PAGE_READONLY);
            }
            for (const auto& pin : P::Pins)
                world.writer(world.host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            const auto target = world.image + P::EntryRva;
            world.Global(WidgetVtableRva + 0x288, target, PAGE_READONLY);
            world.Global(mta::game_de::NativeStartupWorldProfile::ownerVtableRva + 0x690, target, PAGE_READONLY);
        }
        void Reset(bool present = true)
        {
            world.Reset();
            widget.fill(0);
            items.fill(0);
            std::memcpy(items.data(), world.items.data(), world.items.size());
            Put(widget.data(), 0, world.image + WidgetVtableRva);
            Put(widget.data(), 0xc, std::int32_t{11});
            Put(widget.data(), 0x10, world.Class(0));
            Put(widget.data(), 0x20, world.root);
            Put(items.data(), 11 * 24, Widget());
            Put(items.data(), 11 * 24 + 0x10, std::uint32_t{111});
            chunks[0] = reinterpret_cast<std::uintptr_t>(items.data());
            world.Global(mta::game_de::NativeStartupWorldProfile::objectChunksRva, reinterpret_cast<std::uintptr_t>(chunks.data()));
            world.Global(mta::game_de::NativeStartupWorldProfile::objectCountRva, std::int32_t{12});
            Root(0x128, world.Game());
            Root(0x348, present ? Widget() : std::uintptr_t{0});
            Root(0x350, std::uint32_t{79});
            world.Global(P::FlagRva, static_cast<unsigned char>(1));
            const std::array<unsigned char, 24> empty{};
            world.writer(world.host, M::menuHeaderRva, empty.data(), empty.size(), PAGE_READWRITE);
        }
        void Effect(bool& result)
        {
            result = Root<std::uintptr_t>(0x348) != 0;
            world.Global(P::FlagRva, static_cast<unsigned char>(0));
            if (result)
            {
                Root(0x348, std::uintptr_t{0});
                Root(0x350, std::uint32_t{0});
            }
        }
        void RetireWidget()
        {
            // A genuine close may already destroy its detached widget. The
            // controller must reacquire only the surviving enclosing graph.
            Put(items.data(), 11 * 24, std::uintptr_t{0});
        }
    };
}
