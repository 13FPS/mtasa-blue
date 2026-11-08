/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Three proven model families in an owned image, with raw read fences.
 *****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeModelDynamicMemory.h"
#include "../../Client/game_de/NativeModelObjectGroupProfile.h"
#include "../../Client/game_de/NativeModelTimeProfile.h"
#include "../../Client/game_de/NativeModelWheelProfile.h"
#include "../../Client/game_de/NativeModelBankProfile.h"
#include "../../Client/game_de/CModelInfoDE.h"
#include <array>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace NativeModelDynamicFixture
{
    namespace A = mta::game_de::NativeModelObjectGroupProfile;
    namespace T = mta::game_de::NativeModelTimeProfile;
    namespace V = mta::game_de::NativeModelWheelProfile;
    namespace B = mta::game_de::NativeModelBankProfile;
    namespace M = mta::game_de::NativeModelDynamicMemory;
    using mta::game_de::ClockResult;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class Value>
    void Store(std::uintptr_t at, std::size_t offset, const Value& value)
    {
        std::memcpy(reinterpret_cast<void*>(at + offset), &value, sizeof(value));
    }
    struct Storage
    {
        std::uintptr_t image;
        Writer         writer;
        void*          host;
        alignas(std::uintptr_t) std::array<unsigned char, 0x100> foreign{}, otherTime{};
        Storage(std::uintptr_t selectedImage, Writer selectedWriter, void* selectedHost) : image(selectedImage), writer(selectedWriter), host(selectedHost)
        {
            const auto pins = [&](const auto& values)
            {
                for (const auto& pin : values)
                    writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            };
            pins(A::Pins);
            pins(T::Pins);
            pins(V::Pins);
            const auto table = [&](std::uintptr_t rva, const auto& values)
            {
                std::array<std::uintptr_t, 16> methods{};
                for (std::size_t index = 0; index != std::size(values); ++index)
                    methods[index] = image + values[index];
                if (std::size(values) == 8)
                {
                    methods[8] = image + A::InitializeRva;
                    methods[15] = image + 0xace850;
                }
                writer(host, rva, methods.data(), sizeof(methods), PAGE_READONLY);
            };
            table(A::VtableRva, A::VtableMethods);
            table(T::VtableRva, T::VtableMethods);
            table(V::VtableRva, V::VtableMethods);
            const std::array<unsigned char, 8> animationLeaf{0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc3};
            writer(host, 0xace850, animationLeaf.data(), animationLeaf.size(), PAGE_EXECUTE_READ);
            const auto pool = [&](std::uintptr_t rva, std::size_t count, std::size_t stride, std::uintptr_t vtable)
            {
                std::vector<unsigned char> bytes(count * stride, 0xa5);
                for (std::size_t index = 0; index != count; ++index)
                {
                    const std::uint16_t sentinel = 0xffff;
                    std::memcpy(bytes.data() + index * stride, &vtable, sizeof(vtable));
                    std::memcpy(bytes.data() + index * stride + A::FieldOffset, &sentinel, sizeof(sentinel));
                }
                writer(host, rva, bytes.data(), bytes.size(), PAGE_READWRITE);
            };
            pool(A::PoolRva, A::PoolCapacity, A::PoolStride, image + A::VtableRva);
            pool(T::PoolRva, T::PoolCapacity, T::RowBytes, image + T::VtableRva);
            pool(V::PoolRva, V::PoolCapacity, V::PoolStride, image + V::VtableRva);
            Global(A::PoolCountRva, std::int32_t{2});
            Global(T::PoolCountRva, std::int32_t{1});
            Global(V::PoolCountRva, std::int32_t{1});
            Bind(3, Row());
            Bind(4, Row(1));
            Bind(5, image + T::PoolRva);
            Bind(6, image + V::PoolRva);
            Bind(7, 0);
            Store(reinterpret_cast<std::uintptr_t>(foreign.data()), 0, image + A::VtableRva);
            Store(reinterpret_cast<std::uintptr_t>(foreign.data()), A::FieldOffset, std::uint16_t{0});
            Bind(8, reinterpret_cast<std::uintptr_t>(foreign.data()));
            std::array<std::uintptr_t, 16> methods{};
            for (std::size_t index = 0; index != std::size(T::VtableMethods); ++index)
                methods[index] = image + T::VtableMethods[index];
            methods[4] = image + 0x120a770;
            methods[8] = image + A::InitializeRva;
            methods[15] = image + 0xace850;
            writer(host, 0x4317008, methods.data(), sizeof(methods), PAGE_READONLY);
            const std::array<unsigned char, 6> otherType{0xb0, 0x03, 0xc3, 0xcc, 0xcc, 0xcc};
            writer(host, 0x120a770, otherType.data(), otherType.size(), PAGE_EXECUTE_READ);
            Store(reinterpret_cast<std::uintptr_t>(otherTime.data()), 0, image + 0x4317008);
            Bind(9, reinterpret_cast<std::uintptr_t>(otherTime.data()));
        }
        template <class Value>
        void Global(std::uintptr_t rva, const Value& value, DWORD protection = PAGE_READWRITE)
        {
            writer(host, rva, &value, sizeof(value), protection);
        }
        std::uintptr_t Row(std::size_t index = 0) const noexcept { return image + A::PoolRva + index * A::PoolStride; }
        void           Bind(std::uint32_t model, std::uintptr_t at) { Global(B::BankRva + model * B::SlotBytes, at); }
        void           Word(std::uintptr_t metadata, std::uint16_t value) { Store(metadata, A::FieldOffset, value); }
    };
    struct Gate
    {
        M::CheckOwner                    owner = nullptr, after = nullptr;
        unsigned                         owners = 0, applications = 0, mutateAt = 0;
        std::function<void()>            change;
        inline static thread_local Gate* current = nullptr;
        static ClockResult               Owner() noexcept
        {
            ++current->owners;
            return current->owner();
        }
        static ClockResult After() noexcept
        {
            const auto result = current->after();
            if (result == ClockResult::Ok && ++current->applications == current->mutateAt && current->change)
                current->change();
            return result;
        }
    };
}
