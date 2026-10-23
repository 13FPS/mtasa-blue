/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: MTA aircraft limit state, float math and stable consumer addresses
 ****************************************************************************/
#include <game/CAircraftLimitsPolicy.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace
{
    using Policy = MtaGame::CAircraftLimitsPolicy;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    float FromBits(std::uint32_t bits) noexcept
    {
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    std::uint32_t Bits(const float* value) noexcept
    {
        std::uint32_t result;
        std::memcpy(&result, value, sizeof(result));
        return result;
    }
    bool GetterMatches(float value, std::uint32_t bits) noexcept
    {
        // The old getters return float under the real target compiler. NaN
        // arithmetic/payload handling belongs to that ABI, not a new clamp.
        return (bits & 0x7fffffffu) > 0x7f800000u ? std::isnan(value) : Bits(&value) == bits;
    }
}
int main()
{
    try
    {
        struct Enclosed
        {
            std::uint32_t before;
            Policy::State limits;
            std::uint32_t after;
        } owner{0x12345678u, {}, 0x87654321u};
        auto&        state = owner.limits;
        float* const height = Policy::HeightAddress(state);
        float* const velocity = Policy::VelocityAddress(state);
        float* const square = Policy::SquaredVelocityAddress(state);
        Policy::ResetToDefaults(state);
        Check(Policy::GetMaxHeight(state) == 800.0f && Policy::GetMaxVelocity(state) == 1.5f && *square == 2.25f,
              "actual MTA defaults preserve height, raw velocity and its initialized float square");
        Check(reinterpret_cast<std::uintptr_t>(height) == reinterpret_cast<std::uintptr_t>(&state) &&
                  reinterpret_cast<std::uintptr_t>(velocity) - reinterpret_cast<std::uintptr_t>(height) == 4 &&
                  reinterpret_cast<std::uintptr_t>(square) - reinterpret_cast<std::uintptr_t>(height) == 8,
              "consumer targets retain the exact three adjacent float cells and original offsets");
        for (const std::uint32_t raw : {0u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x42c80000u, 0xc1a00000u, 0x44480000u, 1u, 0x007fffffu, 0x00800000u,
                                        0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc23456u})
        {
            const auto previousVelocity = Bits(velocity), previousSquare = Bits(square);
            Policy::SetMaxHeight(state, FromBits(raw));
            Check(Bits(height) == raw && GetterMatches(Policy::GetMaxHeight(state), raw),
                  "height preserves the raw supplied float without Lua bounds, finite checks or normalization");
            Check(Bits(velocity) == previousVelocity && Bits(square) == previousSquare, "height updates leave both actual velocity consumer cells unchanged");
        }
        struct Case
        {
            std::uint32_t value, squared;
            bool          squaredNan;
        };
        for (const Case example :
             {Case{0u, 0u, false}, Case{0x80000000u, 0u, false}, Case{0x3f800000u, 0x3f800000u, false}, Case{0xbf800000u, 0x3f800000u, false},
              Case{0x3fc00000u, 0x40100000u, false}, Case{0xbfc00000u, 0x40100000u, false}, Case{0x40000000u, 0x40800000u, false},
              Case{0xc0000000u, 0x40800000u, false}, Case{0x40400000u, 0x41100000u, false}, Case{0xc0400000u, 0x41100000u, false},
              Case{0x3f800001u, 0x3f800002u, false}, Case{1u, 0u, false}, Case{0x7f7fffffu, 0x7f800000u, false}, Case{0xff7fffffu, 0x7f800000u, false},
              Case{0x7f800000u, 0x7f800000u, false}, Case{0xff800000u, 0x7f800000u, false}, Case{0x7fc12345u, 0u, true}, Case{0xffc23456u, 0u, true}})
        {
            const auto previousHeight = Bits(height);
            Policy::SetMaxVelocity(state, FromBits(example.value));
            Check(Bits(velocity) == example.value && GetterMatches(Policy::GetMaxVelocity(state), example.value),
                  "raw velocity keeps its sign and supplied bits while its getter follows the target float ABI");
            Check(example.squaredNan ? std::isnan(*square) : Bits(square) == example.squared,
                  "the actual float product preserves sign-zero, exact products, rounding, overflow, underflow and NaN behavior");
            Check(Bits(height) == previousHeight && height == Policy::HeightAddress(state) && velocity == Policy::VelocityAddress(state) &&
                      square == Policy::SquaredVelocityAddress(state),
                  "velocity updates preserve height and all borrowed native operand addresses");
        }
        Check(owner.before == 0x12345678u && owner.after == 0x87654321u, "policy updates stay inside the original three-float owner storage");
        Policy::State second;
        Policy::ResetToDefaults(second);
        Policy::SetMaxHeight(second, -1.0f);
        Policy::SetMaxVelocity(second, -2.0f);
        Check(Policy::GetMaxHeight(second) == -1.0f && Policy::GetMaxVelocity(second) == -2.0f && *Policy::SquaredVelocityAddress(second) == 4.0f &&
                  Policy::GetMaxHeight(state) != -1.0f,
              "independent MTA owners keep independent raw limits without a global state or imposed positive range");
        Policy::ResetToDefaults(state);
        const Policy::State& readOnly = state;
        Check(Policy::GetMaxHeight(readOnly) == 800.0f && Policy::GetMaxVelocity(readOnly) == 1.5f && *square == 2.25f &&
                  height == Policy::HeightAddress(state) && velocity == Policy::VelocityAddress(state) && square == Policy::SquaredVelocityAddress(state),
              "reset restores the same MTA defaults in place without invalidating installed consumer addresses");
        Check(Policy::GetMaxHeight(second) == -1.0f && Policy::GetMaxVelocity(second) == -2.0f, "resetting one owner does not change another owner's values");
        std::puts("Production MTA aircraft limits policy fixture completed");
        std::printf("Passed %u actual MTA aircraft limits policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Aircraft limits policy failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
