/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Execute real fixed-ID adapters and the inherited MTA garage policy
 ****************************************************************************/
#include <SharedUtil.Defines.h>
#include <CVector.h>
#include "../../Client/game_de/CGaragesStartupDE.h"
#include "../../Client/game_de/CGameNumericGaragesDE.h"
#include <game/CGarageDoorStatePolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
    using namespace mta::game_de;
    constexpr std::array<std::uint32_t, 16> Expected{7, 8, 10, 11, 12, 15, 18, 19, 24, 27, 32, 33, 36, 40, 41, 47};
    unsigned                                checks = 0;
    void                                    Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    std::uint32_t Word(const float& value)
    {
        std::uint32_t result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }
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
        Check(rejected, "one explicit unavailable/refused operation must propagate rather than return a cached/default value");
    }
    struct Event
    {
        std::uint32_t id;
        std::uint8_t  type;
    };
    struct Owner final : NativeNumericGarageAccess, NativeNamedGarageAccess
    {
        std::array<unsigned, 5>                       calls{};
        unsigned                                      readinessCalls = 0, namedCalls = 0;
        std::uint32_t                                 selected = MAX_GARAGES, failType = MAX_GARAGES;
        ClockResult                                   result = ClockResult::Ok, readinessResult = ClockResult::Ok;
        std::array<NativeGarageGeometry, MAX_GARAGES> geometry{};
        std::array<std::uint8_t, MAX_GARAGES>         type{};
        std::array<bool, MAX_GARAGES>                 opened{};
        std::vector<Event>                            effects;
        Owner()
        {
            for (const auto& descriptor : NativeNumericGarageProfile::Descriptors)
                geometry[descriptor.sdkId] = descriptor.geometry;
            type.fill(200);
        }
        ClockResult RequireStartupNumericGaragesReady() noexcept override
        {
            ++readinessCalls;
            return readinessResult;
        }
        ClockResult ReadNumericGarageGeometry(std::uint32_t id, NativeGarageGeometry& output) noexcept override
        {
            ++calls[0];
            selected = id;
            output = geometry.at(id);
            return result;
        }
        ClockResult ReadNumericGarageType(std::uint32_t id, std::uint8_t& output) noexcept override
        {
            ++calls[1];
            selected = id;
            output = type.at(id);
            return result;
        }
        ClockResult SetNumericGarageType(std::uint32_t id, std::uint8_t value) noexcept override
        {
            ++calls[2];
            selected = id;
            type.at(id) = value;
            effects.push_back({id, value});
            return id == failType ? ClockResult::OwnerConflict : result;
        }
        ClockResult ReadNumericGarageOpen(std::uint32_t id, bool& output) noexcept override
        {
            ++calls[3];
            selected = id;
            output = opened.at(id);
            return result;
        }
        ClockResult SetNumericGarageOpen(std::uint32_t id, bool value) noexcept override
        {
            ++calls[4];
            selected = id;
            opened.at(id) = value;
            return result;
        }
        ClockResult ReadNamedGarageGeometry(const NativeGarageName&, NativeGarageGeometry&) noexcept override
        {
            ++namedCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadNamedGarageType(const NativeGarageName&, std::uint8_t&) noexcept override
        {
            ++namedCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult SetNamedGarageType(const NativeGarageName&, std::uint8_t) noexcept override
        {
            ++namedCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadNamedGarageOpen(const NativeGarageName&, bool&) noexcept override
        {
            ++namedCalls;
            return ClockResult::NotInitialized;
        }
        ClockResult SetNamedGarageOpen(const NativeGarageName&, bool) noexcept override
        {
            ++namedCalls;
            return ClockResult::NotInitialized;
        }
    };

    void Properties(Owner& owner, CGaragesStartupDE& garages)
    {
        // Property availability spans the qualified SDK range. Keep this
        // independent of the narrower inherited initialization itinerary.
        static_assert(MAX_GARAGES == 50 && NativeNumericGarageProfile::Descriptors.size() == MAX_GARAGES);
        for (std::uint32_t id = 0; id != MAX_GARAGES; ++id)
        {
            auto* sdk = garages.GetGarage(id);
            Check(sdk && sdk == garages.GetGarage(id), "mapped SDK pointer is stable and owned by this concrete receiver");
            const auto expected = owner.geometry[id].words;
            const auto before = owner.calls[0];
            CVector    position;
            sdk->GetPosition(position);
            Check(owner.selected == id && owner.calls[0] == before + 1 && Word(position.fX) == expected[0] && Word(position.fY) == expected[1] &&
                      Word(position.fZ) == expected[2],
                  "actual inherited origin getter uses one numeric owner observation and all raw position words");
            std::array<float, 4> output{};
            sdk->GetDirection(output[0], output[1], output[2], output[3]);
            Check(owner.selected == id && owner.calls[0] == before + 2, "inherited direction getter dispatches once to the fixed SDK identity");
            for (std::size_t word = 0; word != 4; ++word)
                Check(Word(output[word]) == expected[3 + word], "actual inherited direction order preserves its raw storage words");
            sdk->GetSize(output[0], output[1], output[2]);
            Check(owner.selected == id && owner.calls[0] == before + 3, "inherited size getter dispatches once to the fixed SDK identity");
            for (std::size_t word = 0; word != 3; ++word)
                Check(Word(output[word]) == expected[7 + word], "SDK size remains raw upperZ,width,depth; Lua convention normalization is not an SDK change");
            sdk->GetBoundingBox(output[0], output[1], output[2], output[3]);
            Check(owner.selected == id && owner.calls[0] == before + 4, "inherited bounds getter dispatches once to the fixed SDK identity");
            for (std::size_t word = 0; word != 4; ++word)
                Check(Word(output[word]) == expected[10 + word], "actual inherited bounds preserve left,right,front,back");
            const auto typeReads = owner.calls[1];
            Check(sdk->GetType() == owner.type[id] && owner.selected == id && owner.calls[1] == typeReads + 1,
                  "type query reaches only its fixed numeric owner once");
            const auto typeWrites = owner.calls[2];
            sdk->SetType(static_cast<BYTE>(id));
            Check(owner.type[id] == static_cast<std::uint8_t>(id) && owner.selected == id && owner.calls[2] == typeWrites + 1,
                  "type intent is dispatched once without named lookup");
            for (const bool opened : {true, false})
            {
                const auto writes = owner.calls[4], reads = owner.calls[3];
                sdk->SetOpen(opened);
                Check(owner.opened[id] == opened && owner.selected == id && owner.calls[4] == writes + 1, "door intent stays with the fixed numeric identity");
                Check(sdk->IsOpen() == opened && owner.selected == id && owner.calls[3] == reads + 1, "door query uses one current numeric observation");
            }
        }
        Check(owner.namedCalls == 0 && garages.GetGarage(8) != garages.GetGarage(11),
              "same-name sprLAe objects keep distinct stable numeric dispatch, without native first-match calls");
        Check(NativeNumericGarageProfile::Descriptors[1].name == NativeNumericGarageProfile::Descriptors[3].name,
              "the independently qualified descriptor set actually exercises the duplicate sprLAe name");
        auto* eight = garages.GetGarage(8);
        auto* eleven = garages.GetGarage(11);
        eight->SetType(71);
        eleven->SetType(83);
        Check(owner.type[8] == 71 && owner.type[11] == 83, "same-name SDK8 and11 cannot overwrite each other's type intent");
        owner.result = ClockResult::BindingChanged;
        const auto before = owner.calls[0];
        CVector    unchanged(11.0f, 12.0f, 13.0f);
        Refuses([&] { eight->GetPosition(unchanged); }, "GetPosition");
        Check(owner.calls[0] == before + 1 && unchanged.fX == 11.0f && unchanged.fY == 12.0f && unchanged.fZ == 13.0f,
              "a failing owner may fill its local snapshot, but inherited SDK output remains whole and unchanged");
        const auto writes = owner.calls[2];
        Refuses([&] { eight->SetType(93); }, "SetType");
        Check(owner.type[8] == 93 && owner.calls[2] == writes + 1, "accepted failure effect persists without wrapper retry or rollback");
        owner.result = ClockResult::Ok;
    }
    void AdapterGuards(Owner& owner)
    {
        NumericGarageSdkDetail::BoundAccess adapter(owner, NativeNumericGarageProfile::Descriptors[1]);
        auto                                wrong = NativeNumericGarageProfile::Descriptors[1].name;
        wrong.back() = static_cast<unsigned char>(wrong.back() ^ 0x7f);
        NativeGarageGeometry geometry;
        geometry.words.fill(0xa5a5a5a5);
        std::uint8_t type = 0x5a;
        bool         opened = true;
        const auto   before = owner.calls;
        Check(adapter.ReadNamedGarageGeometry(wrong, geometry) == ClockResult::InvalidArgument &&
                  adapter.ReadNamedGarageType(wrong, type) == ClockResult::InvalidArgument &&
                  adapter.SetNamedGarageType(wrong, 99) == ClockResult::InvalidArgument &&
                  adapter.ReadNamedGarageOpen(wrong, opened) == ClockResult::InvalidArgument &&
                  adapter.SetNamedGarageOpen(wrong, false) == ClockResult::InvalidArgument,
              "the private numeric adapter refuses a different raw name, including a changed suffix after the logical terminator");
        Check(owner.calls == before && type == 0x5a && opened && geometry.words[0] == 0xa5a5a5a5,
              "invalid adapter identity never reaches the owner or partially changes caller output");
    }
    void Initialize(Owner& owner, CGaragesStartupDE& garages)
    {
        for (std::size_t failing = 0; failing != Expected.size(); ++failing)
        {
            owner.effects.clear();
            owner.type.fill(200);
            owner.failType = Expected[failing];
            Refuses([&] { garages.Initialize(); }, "SetType");
            Check(owner.effects.size() == failing + 1, "real inherited MTA itinerary stops at the entered failing ID");
            for (std::size_t index = 0; index != owner.effects.size(); ++index)
                Check(owner.effects[index].id == Expected[index] && owner.effects[index].type == 1,
                      "the exact original sixteen SDK identities/order/BYTE1 intent are reused");
            for (std::size_t index = 0; index != Expected.size(); ++index)
                Check(owner.type[Expected[index]] == (index <= failing ? 1 : 200),
                      "accepted prefix including uncertain final intent survives, following IDs remain untouched");
            // The complete service must not broaden MTA's original startup
            // writes to the newly exposed property-only garage identities.
            for (std::uint32_t id = 0; id != MAX_GARAGES; ++id)
            {
                bool entered = false;
                for (std::size_t index = 0; index <= failing; ++index)
                    entered = entered || id == Expected[index];
                Check(owner.type[id] == (entered ? 1 : 200), "a failed Initialize leaves every garage outside the entered sixteen-ID prefix untouched");
            }
        }
        owner.failType = MAX_GARAGES;
        owner.effects.clear();
        owner.type.fill(200);
        garages.Initialize();
        garages.Initialize();
        Check(owner.effects.size() == Expected.size() * 2, "explicit repeated initialization reuses the source itinerary without a success latch");
        for (std::size_t index = 0; index != owner.effects.size(); ++index)
            Check(owner.effects[index].id == Expected[index % Expected.size()] && owner.effects[index].type == 1,
                  "each explicit initialization preserves the complete source schedule");
        for (std::uint32_t id = 0; id != MAX_GARAGES; ++id)
        {
            bool selected = false;
            for (const auto startupId : Expected)
                selected = selected || id == startupId;
            Check(owner.type[id] == (selected ? 1 : 200),
                  "repeated Initialize changes only the original sixteen types and leaves the other thirty-four intact");
        }
    }
}
int main()
{
    try
    {
        static_assert(std::is_abstract_v<CGaragesDE> && std::is_abstract_v<CGameNumericGaragesDE>);
        static_assert(!std::has_virtual_destructor_v<CGarage> && !std::has_virtual_destructor_v<CGarages>);
        static_assert(std::is_same_v<decltype(&CGameNumericGaragesDE::GetGarages), CGarages* (CGameNumericGaragesDE::*)()>);
        Owner             owner;
        CGaragesStartupDE garages(owner);
        Check(owner.calls == std::array<unsigned, 5>{} && owner.readinessCalls == 0 && owner.namedCalls == 0,
              "fixed CPU construction performs no native probe, getter, write or readiness query");
        for (const auto status : {ClockResult::Ok, ClockResult::NotInitialized, ClockResult::Busy, ClockResult::BindingChanged})
        {
            const auto before = owner.readinessCalls;
            owner.readinessResult = status;
            Check(garages.RequireReady() == status && owner.readinessCalls == before + 1 && owner.calls == std::array<unsigned, 5>{},
                  "each service readiness request forwards freshly and preserves all property effects");
        }
        Check(!garages.GetGarage(MAX_GARAGES) && !garages.GetGarage(0xffffffffu), "SDK out-of-range IDs retain ordinary null absence");
        const auto before = owner.calls;
        for (std::uint32_t id = 0; id != MAX_GARAGES; ++id)
            Check(garages.GetGarage(id) && garages.GetGarage(id) == garages.GetGarage(id), "every qualified SDK ID has one stable fixed-storage wrapper");
        Check(owner.calls == before, "borrowing all fifty wrappers performs no native property observation or effect");
        Properties(owner, garages);
        AdapterGuards(owner);
        Initialize(owner, garages);
        Check(owner.namedCalls == 0, "no actual numeric SDK method dispatched through the native named API");
        std::puts("Production actual fifty-ID numeric garage SDK properties fixture completed");
        std::puts("Production actual numeric startup garage SDK fixture completed");
        std::printf("Passed %u numeric startup garage SDK checks (%u-bit)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Numeric garage SDK failure after %u checks: %s\n", checks, error.what());
        return EXIT_FAILURE;
    }
}

static_assert(!std::is_copy_constructible_v<mta::game_de::NumericGarageSdkDetail::Garage> &&
                  !std::is_copy_assignable_v<mta::game_de::NumericGarageSdkDetail::Garage> &&
                  !std::is_move_constructible_v<mta::game_de::NumericGarageSdkDetail::Garage> &&
                  !std::is_move_assignable_v<mta::game_de::NumericGarageSdkDetail::Garage>,
              "A fixed numeric property object must never retain another object's adapter reference");
static_assert(!std::is_copy_constructible_v<mta::game_de::CGaragesStartupDE> && !std::is_copy_assignable_v<mta::game_de::CGaragesStartupDE> &&
                  !std::is_move_constructible_v<mta::game_de::CGaragesStartupDE> && !std::is_move_assignable_v<mta::game_de::CGaragesStartupDE>,
              "The owning service preserves all borrowed SDK addresses and adapter lifetimes");
