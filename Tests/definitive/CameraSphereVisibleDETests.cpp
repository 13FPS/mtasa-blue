/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual const camera SDK delegates copied sphere inputs unchanged
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include "../../Client/game_de/CCameraSphereVisibleDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace camera_sphere_visible_fixture
{
    [[noreturn]] inline void Unqualified(const char* method) noexcept
    {
        std::fprintf(stderr, "Unqualified camera sphere fixture SDK call: %s\n", method);
        std::abort();
    }
}
#include "CameraSphereVisibleFixture.generated.h"

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    float FromBits(std::uint32_t bits) noexcept
    {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::uint32_t Bits(float value) noexcept
    {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    struct Owner final : NativeCameraSphereVisibleAccess
    {
        unsigned                     calls = 0;
        ClockResult                  result = ClockResult::Ok;
        bool                         value = false;
        const CVector*               caller = nullptr;
        bool                         alias = false;
        std::array<std::uint32_t, 4> received{};
        std::function<void()>        afterRead;
        ClockResult                  ReadCameraSphereVisible(const CVector& center, float radius, bool& output) noexcept override
        {
            ++calls;
            alias = alias || &center == caller;
            received = {Bits(center.fX), Bits(center.fY), Bits(center.fZ), Bits(radius)};
            // The owner may have staged a value before refusing. Actual SDK
            // dispatch must not publish it, retry or synthesize a frustum.
            output = value;
            if (afterRead)
                afterRead();
            return result;
        }
    };
    struct Unavailable final : NativeCameraSphereVisibleAccess
    {
    };
}

static_assert(std::is_abstract_v<mta::game_de::CCameraSphereVisibleDE>);
static_assert(std::is_base_of_v<CCamera, mta::game_de::CCameraSphereVisibleDE>);
static_assert(
    std::is_same_v<decltype(&mta::game_de::CCameraSphereVisibleDE::IsSphereVisible), bool (mta::game_de::CCameraSphereVisibleDE::*)(CVector*, float) const>);

int main()
{
    try
    {
        Owner                                                           first, second;
        NativeCameraSphereVisibleAccess*                                selected = &first;
        const camera_sphere_visible_fixture::CameraSphereVisibleFixture camera(*selected);
        selected = &second;
        const CCamera& sdk = camera;
        Check(!first.calls && !second.calls, "const camera construction borrows one owner without native or other camera operations");
        CVector center(1.25f, -2.5f, 3.75f);
        first.caller = &center;
        for (const auto bits : {0u, 0x80000000u, 1u, 0x80000001u, 0x3f800000u, 0xbf800000u, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u,
                                0xffcabcdeu, 0x7f812345u, 0xff812345u})
        {
            for (const bool visible : {false, true})
            {
                first.value = visible;
                const auto before = first.calls;
                Check(sdk.IsSphereVisible(&center, FromBits(bits)) == visible && first.calls == before + 1 &&
                          first.received == std::array<std::uint32_t, 4>{Bits(center.fX), Bits(center.fY), Bits(center.fZ), bits} && !first.alias &&
                          !second.calls,
                      "actual const CCamera dispatch copies center and forwards each raw radius bit pattern once, preserving either native boolean without "
                      "clamps/cache/CPU frustum");
            }
        }
        const auto beforeDirect = first.calls;
        Check(static_cast<const CCameraSphereVisibleDE&>(camera).IsSphereVisible(&center, -7.0f) && first.calls == beforeDirect + 1,
              "direct const production override and common SDK select the same borrowed owner once");
        const std::array<std::uint32_t, 3> coordinateBits{0x80000000u, 0x7f800000u, 0x7fcabcdeu};
        center = CVector(FromBits(coordinateBits[0]), FromBits(coordinateBits[1]), FromBits(coordinateBits[2]));
        first.afterRead = [&] { center = CVector(9, 10, 11); };
        const auto beforeCopy = first.calls;
        Check(sdk.IsSphereVisible(&center, FromBits(0x80000000u)) && first.calls == beforeCopy + 1 &&
                  first.received == std::array<std::uint32_t, 4>{coordinateBits[0], coordinateBits[1], coordinateBits[2], 0x80000000u} && center.fX == 9 &&
                  !first.alias,
              "nonfinite and signed-zero coordinates are snapshotted before callback mutation without normalization, retention or aliasing");
        first.afterRead = {};
        const auto beforeNull = first.calls;
        bool       nullRefused = false;
        try
        {
            (void)sdk.IsSphereVisible(nullptr, 1);
        }
        catch (const std::invalid_argument& error)
        {
            nullRefused = std::string(error.what()).find("IsSphereVisible") != std::string::npos;
        }
        Check(nullRefused && first.calls == beforeNull && !second.calls,
              "null SDK center explicitly refuses by method name before borrowing any native access");
        for (const auto result :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            first.result = result;
            first.value = false;
            bool       output = true, refused = false;
            const auto before = first.calls;
            try
            {
                output = sdk.IsSphereVisible(&center, 4);
            }
            catch (const std::runtime_error& error)
            {
                const std::string text(error.what());
                refused = text.find("IsSphereVisible") != std::string::npos && text.find(std::to_string(static_cast<unsigned>(result))) != std::string::npos;
            }
            Check(refused && output && first.calls == before + 1 && !second.calls,
                  "all explicit owner refusals preserve caller output, name the numeric status and make one call without fake false, retry or camera fallback");
        }
        first.result = ClockResult::Ok;
        first.value = true;
        first.afterRead = [&]
        {
            first.result = ClockResult::BindingChanged;
            first.value = false;
        };
        bool       output = false, callbackRefused = false;
        const auto beforeCallback = first.calls;
        try
        {
            output = sdk.IsSphereVisible(&center, 4);
        }
        catch (const std::runtime_error& error)
        {
            callbackRefused = std::string(error.what()).find("IsSphereVisible") != std::string::npos;
        }
        Check(callbackRefused && !output && first.calls == beforeCallback + 1,
              "callback owner loss after staging does not leak its boolean or replay the native operation");
        first.afterRead = {};
        first.result = ClockResult::Ok;
        Check(!sdk.IsSphereVisible(&center, 4), "a later accepted false is fresh rather than the earlier staged/refused true");
        second.value = true;
        const camera_sphere_visible_fixture::CameraSphereVisibleFixture another(*selected);
        const auto                                                      beforeSecond = first.calls;
        Check(static_cast<const CCamera&>(another).IsSphereVisible(&center, 8) && second.calls == 1 && first.calls == beforeSecond,
              "another camera keeps its own exact borrowed access instead of global owner selection");
        Unavailable                                                     unavailable;
        const camera_sphere_visible_fixture::CameraSphereVisibleFixture missing(unavailable);
        output = true;
        bool unavailableRefused = false;
        try
        {
            output = static_cast<const CCamera&>(missing).IsSphereVisible(&center, 2);
        }
        catch (const std::runtime_error& error)
        {
            unavailableRefused = std::string(error.what()).find("IsSphereVisible") != std::string::npos;
        }
        Check(unavailableRefused && output, "default owner refuses explicitly rather than manufacturing an invisible sphere or complete camera");
        std::puts("Actual camera sphere-visible SDK fixture completed");
        std::printf("Passed %u actual camera sphere-visible SDK checks (%zu-bit; no native engine code executed)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Camera sphere-visible SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
