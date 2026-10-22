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
        void GetPosition(CVector&) const override { std::abort(); }
        void GetDirection(float&, float&, float&, float&) const override { std::abort(); }
        void GetSize(float&, float&, float&) const override { std::abort(); }
        void GetBoundingBox(float&, float&, float&, float&) const override { std::abort(); }
    };
    struct Owner final : NativeNamedGarageAccess
    {
        std::array<unsigned, 4> calls{};
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
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CGarageNamedDE> && !std::is_abstract_v<Garage>);
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
