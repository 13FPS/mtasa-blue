/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Constructor-shaped typed visual pool and real occupied reference keys
 *****************************************************************************/
#pragma once
#include "../../Client/game_de/NativeModelDummiesProfile.h"
#include "../../Client/game_de/NativeModelLoadedProfile.h"
#include "../../Client/game_de/NativeModelReferenceProfile.h"
#include <windows.h>
#include <cstring>
#include <vector>

namespace NativeModelDummiesFixture
{
    namespace P = mta::game_de::NativeModelDummiesProfile;
    namespace W = mta::game_de::NativeModelWheelProfile;
    namespace B = mta::game_de::NativeModelBankProfile;
    using Writer = void (*)(void*, std::uintptr_t, const void*, std::size_t, DWORD);
    template <class T>
    void Put(std::uintptr_t object, std::size_t offset, const T& value)
    {
        std::memcpy(reinterpret_cast<void*>(object + offset), &value, sizeof(value));
    }
    class Storage
    {
    public:
        explicit Storage(std::uintptr_t image, std::uintptr_t root, Writer writer, void* host)
            : data(P::PoolCapacity * P::PoolStride), flags(P::PoolCapacity, 0x80), image(image)
        {
            const auto typed = [&](const auto& pins)
            {
                for (const auto& pin : pins)
                    writer(host, pin.rva, pin.bytes, pin.size, pin.code ? PAGE_EXECUTE_READ : PAGE_READONLY);
            };
            typed(W::Pins);
            typed(P::Pins);
            for (const auto& pin : mta::game_de::NativeModelLoadedProfile::Code)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            for (const auto& pin : mta::game_de::NativeModelReferenceProfile::Code)
                writer(host, pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            std::array<std::uintptr_t, P::VtableMethods.size()> methods{};
            for (std::size_t n = 0; n < methods.size(); ++n)
                methods[n] = image + P::VtableMethods[n];
            writer(host, W::VtableRva, methods.data(), sizeof(methods), PAGE_READONLY);
            std::vector<unsigned char> metadata(W::PoolBytes);
            for (std::uint32_t n = 0; n < 2; ++n)
            {
                auto*      row = metadata.data() + n * W::PoolStride;
                const auto table = image + W::VtableRva, visual = Visual(n);
                std::memcpy(row, &table, sizeof(table));
                std::memcpy(row + P::VisualOffset, &visual, sizeof(visual));
                flags[n] = static_cast<unsigned char>(n + 1);
                for (std::uint32_t dummy = 0; dummy < P::NativeVectorCount; ++dummy)
                {
                    const std::array<float, 3> initial{static_cast<float>(dummy + 1), static_cast<float>(dummy + 21), static_cast<float>(dummy + 41)};
                    std::memcpy(data.data() + n * P::PoolStride + dummy * P::VectorBytes, initial.data(), sizeof(initial));
                }
            }
            writer(host, W::PoolRva, metadata.data(), metadata.size(), PAGE_READWRITE);
            const std::int32_t count = 2;
            writer(host, W::PoolCountRva, &count, sizeof(count), PAGE_READWRITE);
            for (std::uint32_t n = 0; n < 2; ++n)
            {
                const auto at = Model(n);
                writer(host, B::BankRva + (3 + n) * sizeof(std::uintptr_t), &at, sizeof(at), PAGE_READWRITE);
                const std::array<unsigned char, 2> loaded{0, 1};
                writer(host, mta::game_de::NativeModelLoadedProfile::ArrayRva + (3 + n) * 16 + 12, loaded.data(), loaded.size(), PAGE_READWRITE);
            }
            const auto headerAt = reinterpret_cast<std::uintptr_t>(header.data());
            Put(headerAt, 0, reinterpret_cast<std::uintptr_t>(data.data()));
            Put(headerAt, 8, reinterpret_cast<std::uintptr_t>(flags.data()));
            Put(headerAt, 0x10, std::int32_t{50});
            Put(headerAt, 0x14, std::int32_t{1});
            Put(headerAt, 0x18, static_cast<unsigned char>(1));
            writer(host, P::PoolRootRva, &headerAt, sizeof(headerAt), PAGE_READWRITE);

            // These genuine occupied keys take the existing native CAS route;
            // no allocator or native function is replaced or executed. Model4
            // starts at zero to exercise explicit final KeepResident release.
            const auto map = root + mta::game_de::NativeModelReferenceProfile::MapOffset;
            std::memset(reinterpret_cast<void*>(map), 0, mta::game_de::NativeModelReferenceProfile::HeaderBytes);
            references = {{{3, 2, 1, 0}, {4, 0, -1, 0}}};
            Put(map, 0, reinterpret_cast<std::uintptr_t>(references.data()));
            Put(map, 8, std::int32_t{2});
            Put(map, 0xc, std::int32_t{2});
            Put(map, 0x10, std::uint32_t{3});
            Put(map, 0x28, std::int32_t{2});
            Put(map, 0x2c, std::int32_t{128});
            Put(map, 0x30, std::int32_t{-1});
            Put(map, 0x38, std::int32_t{0});
            Put(map, 0x48, std::int32_t{1});
        }
        std::uintptr_t               Model(std::uint32_t n = 0) const noexcept { return image + W::PoolRva + n * W::PoolStride; }
        std::uintptr_t               Visual(std::uint32_t n = 0) const noexcept { return reinterpret_cast<std::uintptr_t>(data.data()) + n * P::PoolStride; }
        std::uint32_t                Count(std::uint32_t n = 0) const noexcept { return references[n].count; }
        std::array<std::uint32_t, 3> Bits(std::uint32_t dummy, std::uint32_t n = 0) const noexcept
        {
            std::array<std::uint32_t, 3> value{};
            std::memcpy(value.data(), reinterpret_cast<void*>(Visual(n) + dummy * P::VectorBytes), sizeof(value));
            return value;
        }
        std::array<unsigned char, P::PoolHeaderBytes> header{};
        std::vector<unsigned char>                    data, flags;
        struct Row
        {
            std::uint32_t key, count;
            std::int32_t  next, bucket;
        };
        static_assert(sizeof(Row) == 16);
        std::array<Row, 2> references{};

    private:
        std::uintptr_t image;
    };
}
