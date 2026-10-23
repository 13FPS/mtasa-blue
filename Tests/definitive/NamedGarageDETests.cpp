/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK named garage intent, identity and accepted failure effects
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CGarageNamedDE.h"
#include <game/CGarageDoorStatePolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* why)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(why);
    }
    class Garage final : public CGarageNamedDE
    {
    public:
        Garage(NativeNamedGarageAccess& access, const NativeGarageName& name) : CGarageNamedDE(access, name) {}
    };
    struct Owner final : NativeNamedGarageAccess
    {
        std::array<unsigned, 4> calls{};
        unsigned                geometryReads = 0;
        NativeGarageGeometry    geometry;
        NativeGarageName        selected{};
        std::uint8_t            type = 0, door = 0;
        ClockResult             result = ClockResult::Ok;
        ClockResult             ReadNamedGarageType(const NativeGarageName& name, std::uint8_t& output) noexcept override
        {
            ++calls[0];
            selected = name;
            // Exercise both a failing owner that leaves output untouched and
            // one that has produced a value before its final refusal fence.
            if (result != ClockResult::NotInitialized)
                output = type;
            return result;
        }
        ClockResult SetNamedGarageType(const NativeGarageName& name, std::uint8_t value) noexcept override
        {
            ++calls[1];
            selected = name;
            type = value;
            return result;
        }
        ClockResult ReadNamedGarageOpen(const NativeGarageName& name, bool& output) noexcept override
        {
            ++calls[2];
            selected = name;
            if (result != ClockResult::NotInitialized)
                output = MtaGame::CGarageDoorStatePolicy::IsOpen(door);
            return result;
        }
        ClockResult ReadNamedGarageGeometry(const NativeGarageName& name, NativeGarageGeometry& output) noexcept override
        {
            ++geometryReads;
            selected = name;
            if (result != ClockResult::NotInitialized)
                output = geometry;
            return result;
        }
        ClockResult SetNamedGarageOpen(const NativeGarageName& name, bool value) noexcept override
        {
            ++calls[3];
            selected = name;
            door = MtaGame::CGarageDoorStatePolicy::StateForOpen(value);
            return result;
        }
    };
    template <class Action>
    void Refuses(Action action, const char* operation)
    {
        bool rejected = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            rejected = std::string(error.what()).find(operation) != std::string::npos;
        }
        Check(rejected, "SDK owner refusal must throw with exact operation instead of returning cached/false/default state");
    }
    std::uint32_t Word(const float& value)
    {
        std::uint32_t word;
        std::memcpy(&word, &value, sizeof(word));
        return word;
    }
    void PutWord(float& output, std::uint32_t word)
    {
        std::memcpy(&output, &word, sizeof(word));
    }
    void GeometryChecks(Owner& owner, CGarage& sdk, const NativeGarageName& identity)
    {
        const CGarage&                          constant = sdk;
        const auto                              properties = owner.calls;
        constexpr std::array<std::uint32_t, 14> patterns = {0x00000000, 0x80000000, 0x3f800001, 0xc1200000, 0x7f7fffff, 0xff7fffff, 0x7f800000,
                                                            0xff800000, 0x7fc12345, 0xffc07654, 0x7fa54321, 0x00000001, 0x80000001, 0x00800000};
        owner.result = ClockResult::Ok;
        for (std::size_t rotation = 0; rotation != patterns.size(); ++rotation)
        {
            for (std::size_t index = 0; index != patterns.size(); ++index)
                owner.geometry.words[index] = patterns[(index + rotation) % patterns.size()];
            const auto expected = owner.geometry.words;
            auto       before = owner.geometryReads;
            CVector    position;
            constant.GetPosition(position);
            Check(owner.geometryReads == before + 1 && owner.selected == identity,
                  "GetPosition takes one fresh same-name owner observation instead of cached current garage or per-component queries");
            Check(Word(position.fX) == expected[0] && Word(position.fY) == expected[1] && Word(position.fZ) == expected[2],
                  "actual SDK origin preserves every binary32 sign/NaN/infinity/subnormal payload without arithmetic");
            std::array<float, 4> output{};
            before = owner.geometryReads;
            constant.GetDirection(output[0], output[1], output[2], output[3]);
            Check(owner.geometryReads == before + 1 && owner.selected == identity,
                  "GetDirection reads one coherent same-name tuple before assigning four outputs");
            for (std::size_t index = 0; index != output.size(); ++index)
                Check(Word(output[index]) == expected[3 + index], "SDK direction order is Axy then Bxy with exact raw bits");
            before = owner.geometryReads;
            constant.GetSize(output[0], output[1], output[2]);
            Check(owner.geometryReads == before + 1 && owner.selected == identity, "GetSize reads one current named geometry observation");
            for (std::size_t index = 0; index != 3; ++index)
                Check(Word(output[index]) == expected[7 + index], "SDK Height/Width/Depth are raw upper-plane/A-length/B-length; no origin subtraction");
            before = owner.geometryReads;
            constant.GetBoundingBox(output[0], output[1], output[2], output[3]);
            Check(owner.geometryReads == before + 1 && owner.selected == identity, "GetBoundingBox takes one fresh same-name read");
            for (std::size_t index = 0; index != output.size(); ++index)
                Check(Word(output[index]) == expected[10 + index], "SDK bound order is minX,maxX,minY,maxY without normalization");
        }
        float alias = 0;
        auto  before = owner.geometryReads;
        constant.GetDirection(alias, alias, alias, alias);
        Check(Word(alias) == owner.geometry.words[6] && owner.geometryReads == before + 1,
              "aliased direction outputs preserve actual classic sequential assignment order");
        before = owner.geometryReads;
        constant.GetSize(alias, alias, alias);
        Check(Word(alias) == owner.geometry.words[9] && owner.geometryReads == before + 1,
              "aliased size outputs finish with depth rather than reordered/cached values");
        before = owner.geometryReads;
        constant.GetBoundingBox(alias, alias, alias, alias);
        Check(Word(alias) == owner.geometry.words[13] && owner.geometryReads == before + 1,
              "aliased bound outputs finish with back as in actual classic assignment order");
        for (const auto result : {ClockResult::NotInitialized, ClockResult::InvalidArgument, ClockResult::BindingChanged, ClockResult::ReadFailure,
                                  ClockResult::Busy, ClockResult::OriginalException, ClockResult::Expired, ClockResult::Stopped})
        {
            owner.result = result;
            CVector position;
            PutWord(position.fX, 0x7fa12345);
            PutWord(position.fY, 0x80000000);
            PutWord(position.fZ, 0xffc54321);
            std::array<float, 4> output{};
            for (std::size_t index = 0; index != output.size(); ++index)
                PutWord(output[index], patterns[index]);
            before = owner.geometryReads;
            Refuses([&] { constant.GetPosition(position); }, "GetPosition");
            Check(Word(position.fX) == 0x7fa12345 && Word(position.fY) == 0x80000000 && Word(position.fZ) == 0xffc54321,
                  "failed position observation preserves all caller outputs even if owner produced a tuple before final refusal");
            Refuses([&] { constant.GetDirection(output[0], output[1], output[2], output[3]); }, "GetDirection");
            Refuses([&] { constant.GetSize(output[0], output[1], output[2]); }, "GetSize");
            Refuses([&] { constant.GetBoundingBox(output[0], output[1], output[2], output[3]); }, "GetBoundingBox");
            for (std::size_t index = 0; index != output.size(); ++index)
                Check(Word(output[index]) == patterns[index], "every refused tuple getter preserves caller output references");
            Check(owner.geometryReads == before + 4 && owner.selected == identity,
                  "four refused getters each dispatch once; none retries, falls back or reuses a successful tuple");
        }
        NativeNamedGarageAccess absent;
        Garage                  missing(absent, identity);
        CVector                 position;
        std::array<float, 4>    output{};
        Refuses([&] { missing.GetPosition(position); }, "GetPosition");
        Refuses([&] { missing.GetDirection(output[0], output[1], output[2], output[3]); }, "GetDirection");
        Refuses([&] { missing.GetSize(output[0], output[1], output[2]); }, "GetSize");
        Refuses([&] { missing.GetBoundingBox(output[0], output[1], output[2], output[3]); }, "GetBoundingBox");
        Check(owner.calls == properties, "all geometry observations preserve unrelated type/open operations and accepted effects");
        owner.result = ClockResult::Ok;
    }

}
int main()
{
    try
    {
        static_assert(!std::is_abstract_v<CGarageNamedDE> && !std::is_abstract_v<Garage>);
        static_assert(!std::is_constructible_v<CGarageNamedDE, NativeNamedGarageAccess&, const NativeGarageName&>);
        Owner            owner;
        NativeGarageName source{{'M', 'i', 'X', 0, 0xa1, 0xb2, 0xc3, 0xd4}};
        const auto       identity = source;
        Garage           wrapper(owner, source);
        source.fill('Z');
        CGarage*       sdk = &wrapper;
        const CGarage* constant = &wrapper;
        Check(owner.calls == std::array<unsigned, 4>{}, "constructor copies CPU identity without native query/write or geometry access");
        for (unsigned type = 0; type != 256; ++type)
        {
            const auto before = owner.calls;
            sdk->SetType(static_cast<BYTE>(type));
            Check(static_cast<unsigned>(owner.type) == type && owner.calls[1] == before[1] + 1 && owner.selected == identity,
                  "actual SDK setter preserves every BYTE and the exact copied native lookup name once");
            Check(static_cast<unsigned>(constant->GetType()) == type && owner.calls[0] == before[0] + 1 && owner.selected == identity,
                  "const SDK getter reads selected named owner rather than stale wrapper cache");
            Check(owner.calls[2] == before[2] && owner.calls[3] == before[3], "type operations do not invent door or geometry side effects");
        }
        for (unsigned state = 0; state != 256; ++state)
        {
            owner.door = static_cast<std::uint8_t>(state);
            const auto before = owner.calls;
            Check(constant->IsOpen() == (state == 1 || state == 3) && owner.calls[2] == before[2] + 1 && owner.selected == identity,
                  "actual SDK reads genuine MTA1/3 door policy, including native script's differing4 state");
            Check(owner.calls[0] == before[0] && owner.calls[1] == before[1] && owner.calls[3] == before[3], "IsOpen has no type query or write");
        }
        for (const bool open : {false, true})
        {
            const auto before = owner.calls;
            sdk->SetOpen(open);
            Check(owner.door == (open ? 3 : 2) && owner.calls[3] == before[3] + 1 && owner.selected == identity,
                  "SetOpen forwards the same bool once through actual MTA3/2 policy");
            Check(owner.calls[0] == before[0] && owner.calls[1] == before[1] && owner.calls[2] == before[2], "SetOpen does not query, replay or change type");
        }
        for (const auto status : {ClockResult::NotInitialized, ClockResult::InvalidArgument, ClockResult::BindingChanged, ClockResult::Busy,
                                  ClockResult::ReadFailure, ClockResult::WriteFailure, ClockResult::OriginalException, ClockResult::Expired})
        {
            owner.result = status;
            const auto before = owner.calls;
            Refuses([&] { (void)constant->GetType(); }, "GetType");
            Refuses([&] { sdk->SetType(0xed); }, "SetType");
            Refuses([&] { (void)constant->IsOpen(); }, "IsOpen");
            Refuses([&] { sdk->SetOpen(true); }, "SetOpen");
            for (std::size_t operation = 0; operation != before.size(); ++operation)
                Check(owner.calls[operation] == before[operation] + 1, "each refusal dispatches once without fallback/readback/retry");
            Check(owner.type == 0xed && owner.door == 3 && owner.selected == identity,
                  "effects accepted before final failure persist without rollback or rewritten identity");
        }
        const auto beforeInvalid = owner.calls;
        bool       malformed = false;
        try
        {
            Garage invalid(owner, source);
        }
        catch (const std::invalid_argument&)
        {
            malformed = true;
        }
        Check(malformed && owner.calls == beforeInvalid, "all8 nonzero bytes refuse before any native call");
        NativeNamedGarageAccess absent;
        Garage                  absentWrapper(absent, identity);
        Refuses([&] { (void)absentWrapper.GetType(); }, "GetType");
        Refuses([&] { absentWrapper.SetType(1); }, "SetType");
        Refuses([&] { (void)absentWrapper.IsOpen(); }, "IsOpen");
        Refuses([&] { absentWrapper.SetOpen(false); }, "SetOpen");
        owner.result = ClockResult::Ok;
        Owner independent;
        for (std::size_t length = 0; length != 8; ++length)
        {
            NativeGarageName name{};
            for (std::size_t index = 0; index != length; ++index)
                name[index] = static_cast<unsigned char>(0x80 + index);
            Garage selected(independent, name);
            selected.SetType(static_cast<BYTE>(length));
            Check(independent.selected == name && static_cast<std::size_t>(selected.GetType()) == length,
                  "bounded0..7-byte identity preserves non-ASCII bytes and independent owner selection without local normalization");
        }
        Check(owner.calls == beforeInvalid && owner.type == 0xed && owner.selected == identity,
              "construction and operations on another owner never mutate prior wrapper state");
        GeometryChecks(owner, *sdk, identity);
        std::puts("Production actual named garage geometry SDK fixture completed");
        std::puts("Production actual named garage SDK fixture completed");
        std::printf("Passed %u actual named garage SDK checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Named garage SDK fixture failed: %s\n", error.what());
        return 1;
    }
}
