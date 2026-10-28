/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Shared owned default-collision seed for reader and creator fixtures
 ****************************************************************************/
#pragma once
#include "../../Client/game_de/NativePedModelDefaultCollision.h"
#include "../../Client/game_de/NativePedModelMetadataProfile.h"

// Include the existing SpecialFixture definitions with its main renamed first.
// This helper adds only the completed default's owned image/private snapshots;
// it never calls a historical main or executes copied native instructions.
namespace NativePedModelDefaultCollisionFixture
{
    namespace Profile = mta::game_de::NativePedModelMetadataProfile;
    namespace Calls = mta::game_de::NativePedModelDefaultCollision;
    struct Fixture : SpecialFixture
    {
        unsigned char* block = nullptr;
        unsigned char* data = nullptr;
        unsigned char* spheres = nullptr;
        unsigned char* segments = nullptr;
        explicit Fixture(const std::filesystem::path& path) : SpecialFixture(path)
        {
            block = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x3000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Check(block != nullptr, "own only the three private default-collision fixture pages");
            ResetDefaultCollision();
        }
        std::uintptr_t Image() const { return transform.image.image; }
        std::uintptr_t Canonical() const { return Image() + Profile::DefaultCollisionRva; }
        void           SeedDefaultCollision(std::size_t skew = 0)
        {
            Check(skew <= 1, "the only optional seed skew tests safe unaligned raw observation");
            DWORD previous = 0;
            Check(VirtualProtect(block, 0x3000, PAGE_READWRITE, &previous) != FALSE, "restore only owned private seed pages");
            std::memset(block, 0xcd, 0x3000);
            data = block + skew;
            spheres = block + 0x1000 + skew;
            segments = block + 0x2000 + skew;
            for (const auto& pin : Profile::Code)
                transform.image.Patch(pin.rva, pin.bytes, pin.size, PAGE_EXECUTE_READ);
            // The captured producer proves equality to these immutable sources,
            // not their numeric stock values. Deliberately seed distinctive
            // raw bytes so a hard-coded guessed bound cannot pass this fixture.
            const std::array<unsigned char, 16> bounds = {0x31, 0x32, 0x33, 0x34, 0x41, 0x42, 0x43, 0x44, 0x51, 0x52, 0x53, 0x54, 0x61, 0x62, 0x63, 0x64};
            const std::array<unsigned char, 4>  maxY = {0x71, 0x72, 0x73, 0x74};
            transform.image.Patch(Profile::BoundsLiteralRva, bounds.data(), bounds.size(), PAGE_READONLY);
            transform.image.Patch(Profile::MaxYLiteralRva, maxY.data(), maxY.size(), PAGE_READONLY);
            std::array<unsigned char, 0x38> outer;
            outer.fill(0x9b);
            std::memcpy(outer.data(), bounds.data(), bounds.size());
            std::memcpy(outer.data() + 0x10, maxY.data(), maxY.size());
            Put(outer.data(), 0x14, std::uint32_t{0x3f733333});
            Put(outer.data(), 0x18, std::uint64_t{0});
            Put(outer.data(), 0x20, std::uint32_t{0});
            Put(outer.data(), 0x24, std::uint32_t{0x3f800000});
            outer[0x28] = 0;
            Put(outer.data(), 0x30, Address(data));
            transform.image.Patch(Profile::DefaultCollisionRva, outer.data(), outer.size(), PAGE_READWRITE);
            data[6] = 0;
            Put(data, 8, Address(spheres));
            Put(data, 0x18, Address(segments));
            constexpr std::array<std::uint32_t, 3> z = {0xbe4ccccc, 0x3e4cccd0, 0x3f19999a};
            for (std::size_t index = 0; index != z.size(); ++index)
            {
                auto* row = spheres + index * 0x14;
                Put(row, 0, std::uint64_t{0});
                Put(row, 8, z[index]);
                Put(row, 0xc, std::uint32_t{0x3eb33333});
                Put(row, 0x10, static_cast<std::uint16_t>(0x3e + index * 0x100));
                row[0x12] = 0xff;
            }
            for (std::size_t index = 0; index != 2; ++index)
            {
                auto* row = segments + index * 0x20;
                Put(row, 0, std::uint64_t{0});
                Put(row, 8, std::uint32_t{0});
                Put(row, 0x10, std::uint64_t{0});
                Put(row, 0x18, index ? std::uint32_t{0x3f800000} : std::uint32_t{0xbf800000});
            }
        }
        void ResetDefaultCollision()
        {
            SpecialFixture::Reset();
            SeedDefaultCollision();
            PoolTestHost::image = Image();
            PoolTestHost::ownerResult = mta::game_de::ClockResult::Ok;
            PoolTestHost::reads = PoolTestHost::failAt = PoolTestHost::ownerCalls = PoolTestHost::revokeAt = 0;
            PoolTestHost::watched = 0;
            PoolTestHost::after = nullptr;
        }
        ~Fixture()
        {
            if (block)
                VirtualFree(block, 0, MEM_RELEASE);
        }
    };
}
