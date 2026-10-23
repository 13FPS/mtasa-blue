/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exact MTA give-weapon callback ordering and accepted failure prefixes
 ****************************************************************************/
#include <game/CPedGiveWeaponPolicy.h>
#include <array>
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    enum class Type : std::uint32_t
    {
        Unarmed = 0,
        Ordinary = 24,
        Satchel = 39,
        Detonator = 40
    };
    enum class Skill : std::uint8_t
    {
        Poor = 0,
        Standard = 1,
        Pro = 2,
        Raw = 255
    };
    enum class Blocking : std::uint32_t
    {
        Required = 0x12345678u
    };
    enum class Step
    {
        Info,
        GetModel,
        LookupModel,
        Request,
        Custom,
        Recurse,
        Native,
        Wrapper
    };
    struct Event
    {
        Step          step;
        std::uint32_t type, value;
        bool          operator==(const Event& other) const noexcept { return step == other.step && type == other.type && value == other.value; }
    };
    struct Weapon
    {
        int slot;
    };
    struct Scenario;
    struct Info
    {
        Scenario* owner = nullptr;
        Type      type = Type::Ordinary;
        int       model = 0;
        int       GetModel();
    };
    struct Model
    {
        Scenario* owner = nullptr;
        Type      type = Type::Ordinary;
        void      Request(Blocking flags, const char* label);
        void      MakeCustomModel();
    };
    struct Scenario
    {
        std::vector<Event>     events;
        int                    failAt = -1;
        unsigned               requests = 0, customs = 0, grants = 0;
        bool                   infoPresent = true, modelPresent = true, overrideSlot = false;
        int                    slot = 7;
        Type                   latestInfo = Type::Ordinary;
        std::array<Info, 3>    infos;
        std::array<Model, 3>   models;
        std::array<Weapon, 13> weapons;
        std::uint32_t          receivedFlags = 0;
        std::string            receivedLabel;
        Scenario()
        {
            unsigned index = 0;
            for (const Type type : {Type::Ordinary, Type::Satchel, Type::Detonator})
            {
                infos[index] = {this, type, static_cast<int>(type) + 100};
                models[index] = {this, type};
                ++index;
            }
            for (unsigned i = 0; i != weapons.size(); ++i)
                weapons[i].slot = static_cast<int>(i);
        }
        static unsigned Index(Type type) { return type == Type::Satchel ? 1u : type == Type::Detonator ? 2u : 0u; }
        void            Record(Step step, Type type, std::uint32_t value = 0)
        {
            events.push_back({step, static_cast<std::uint32_t>(type), value});
            if (failAt >= 0 && events.size() == static_cast<std::size_t>(failAt + 1))
                throw std::runtime_error("accepted-prefix-" + std::to_string(failAt));
        }
        Weapon* Give(Type type, std::uint32_t ammo, Skill skill)
        {
            return MtaGame::CPedGiveWeaponPolicy::Give(
                type, ammo, skill, Type::Unarmed, Type::Satchel, Blocking::Required,
                [this](Type requested, Skill desired) -> Info*
                {
                    Record(Step::Info, requested, static_cast<std::uint8_t>(desired));
                    latestInfo = requested;
                    return infoPresent ? &infos[Index(requested)] : nullptr;
                },
                [this](std::uint32_t model) -> Model*
                {
                    Record(Step::LookupModel, latestInfo, model);
                    return modelPresent ? &models[Index(latestInfo)] : nullptr;
                },
                [this]
                {
                    Record(Step::Recurse, Type::Detonator);
                    Give(Type::Detonator, 1, Skill::Standard);
                },
                [this](Type requested, std::uint32_t amount)
                {
                    ++grants;
                    Record(Step::Native, requested, amount);
                    return overrideSlot ? slot : requested == Type::Unarmed ? 0 : requested == Type::Satchel ? 8 : requested == Type::Detonator ? 12 : 7;
                },
                [this, type](int returnedSlot) -> Weapon*
                {
                    Record(Step::Wrapper, type, static_cast<std::uint32_t>(returnedSlot));
                    return returnedSlot >= 0 && returnedSlot < static_cast<int>(weapons.size()) ? &weapons[returnedSlot] : nullptr;
                });
        }
    };
    int Info::GetModel()
    {
        owner->Record(Step::GetModel, type, static_cast<std::uint32_t>(model));
        return model;
    }
    void Model::Request(Blocking flags, const char* label)
    {
        ++owner->requests;
        owner->receivedFlags = static_cast<std::uint32_t>(flags);
        owner->receivedLabel = label;
        owner->Record(Step::Request, type, static_cast<std::uint32_t>(flags));
    }
    void Model::MakeCustomModel()
    {
        ++owner->customs;
        owner->Record(Step::Custom, type);
    }
    unsigned checks = 0;
    void     Check(bool condition, const char* reason)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(reason);
    }
    std::vector<Step> Steps(const Scenario& scenario)
    {
        std::vector<Step> result;
        for (const auto& event : scenario.events)
            result.push_back(event.step);
        return result;
    }
    unsigned Count(const std::vector<Event>& events, Step step)
    {
        unsigned count = 0;
        for (const auto& event : events)
            if (event.step == step)
                ++count;
        return count;
    }
}

int main()
{
    try
    {
        const std::vector<Step> ordinary{Step::Info, Step::GetModel, Step::LookupModel, Step::Request, Step::Custom, Step::Native, Step::Wrapper};
        Scenario                normal;
        Check(normal.Give(Type::Ordinary, 123, Skill::Pro) == &normal.weapons[7] && Steps(normal) == ordinary,
              "ordinary give prepares exact typed model then grants once and returns the existing wrapper for the native selected slot");
        Check(normal.receivedFlags == 0x12345678u && normal.receivedLabel == "CPedSA::GiveWeapon" && normal.requests == 1 && normal.customs == 1,
              "actual model request retains blocking flags and the original label before custom-model creation");
        Check(normal.events.front().value == 2 && normal.events[2].value == 124 && normal.events[5].value == 123,
              "skill reaches info lookup, model identifier reaches model lookup and raw ammo reaches the native callback");
        Scenario unarmed;
        Check(unarmed.Give(Type::Unarmed, 0xffffffffu, Skill::Raw) == &unarmed.weapons[0] && Steps(unarmed) == std::vector<Step>{Step::Native, Step::Wrapper} &&
                  unarmed.events[0].value == 0xffffffffu,
              "unarmed skips all model/info work but still grants raw ammo and wraps the returned slot");
        Scenario missingInfo;
        missingInfo.infoPresent = false;
        Check(missingInfo.Give(Type::Satchel, 13, Skill::Poor) == &missingInfo.weapons[8] &&
                  Steps(missingInfo) == std::vector<Step>{Step::Info, Step::Native, Step::Wrapper},
              "missing weapon info skips model preparation and detonator recursion without skipping the genuine outer grant");
        Scenario missingModel;
        missingModel.modelPresent = false;
        Check(missingModel.Give(Type::Ordinary, 55, Skill::Standard) == &missingModel.weapons[7] &&
                  Steps(missingModel) == std::vector<Step>{Step::Info, Step::GetModel, Step::LookupModel, Step::Native, Step::Wrapper},
              "missing model info skips only Request and MakeCustomModel");
        Scenario                satchel;
        const std::vector<Step> recursive{Step::Info,    Step::GetModel, Step::LookupModel, Step::Request,     Step::Custom,
                                          Step::Recurse, Step::Info,     Step::GetModel,    Step::LookupModel, Step::Request,
                                          Step::Custom,  Step::Native,   Step::Wrapper,     Step::Native,      Step::Wrapper};
        Check(satchel.Give(Type::Satchel, 73, Skill::Pro) == &satchel.weapons[8] && Steps(satchel) == recursive,
              "satchel completes recursive detonator preparation/grant/wrapper return before the outer native grant");
        Check(satchel.events[6] == Event{Step::Info, 40, 1} && satchel.events[11] == Event{Step::Native, 40, 1} &&
                  satchel.events[13] == Event{Step::Native, 39, 73} && satchel.grants == 2,
              "recursive detonator uses exactly one ammo and standard skill while the original request keeps its own ammo and type");
        Scenario satchelNoModel;
        satchelNoModel.modelPresent = false;
        Check(satchelNoModel.Give(Type::Satchel, 9, Skill::Raw) == &satchelNoModel.weapons[8] && satchelNoModel.grants == 2 && !satchelNoModel.requests &&
                  Count(satchelNoModel.events, Step::Recurse) == 1,
              "weapon-info-present satchel still gives its detonator when both model lookups are absent");
        for (const std::uint32_t ammo : {0u, 1u, 9999u, 99999u, 100000u, 0x80000000u, 0xffffffffu})
        {
            Scenario raw;
            raw.Give(Type::Ordinary, ammo, Skill::Raw);
            Check(raw.events[0].value == 255 && raw.events[5].value == ammo && raw.grants == 1,
                  "the policy adds no ammo clamp, signed normalization or skill narrowing beyond its typed caller argument");
        }
        for (const int slot : {-1, 0, 12, 13, 127})
        {
            Scenario returned;
            returned.overrideSlot = true;
            returned.slot = slot;
            Weapon* expected = slot >= 0 && slot < 13 ? &returned.weapons[slot] : nullptr;
            Check(returned.Give(Type::Ordinary, 3, Skill::Standard) == expected && returned.events.back().value == static_cast<std::uint32_t>(slot),
                  "the native returned slot is forwarded unchanged to the existing wrapper bank, including its real null result");
        }
        Scenario rawModel;
        rawModel.infos[0].model = -1;
        rawModel.Give(Type::Ordinary, 7, Skill::Standard);
        Check(rawModel.events[2].value == 0xffffffffu, "the model identifier keeps classic's explicit uint32 conversion without a new validity clamp");
        const auto baseline = satchel.events;
        for (unsigned fail = 0; fail != baseline.size(); ++fail)
        {
            Scenario interrupted;
            interrupted.failAt = static_cast<int>(fail);
            Weapon  sentinel{99};
            Weapon* output = &sentinel;
            bool    threw = false;
            try
            {
                output = interrupted.Give(Type::Satchel, 73, Skill::Pro);
            }
            catch (const std::runtime_error& error)
            {
                threw = std::string(error.what()) == "accepted-prefix-" + std::to_string(fail);
            }
            const std::vector<Event> prefix(baseline.begin(), baseline.begin() + fail + 1);
            Check(threw && output == &sentinel && interrupted.events == prefix,
                  "every callback exception propagates unchanged at its exact prefix without continuing, retrying or fabricating a wrapper");
            Check(interrupted.grants == Count(prefix, Step::Native) && interrupted.requests == Count(prefix, Step::Request) &&
                      interrupted.customs == Count(prefix, Step::Custom),
                  "already accepted native/model effects stay accounted for without policy rollback or replay");
        }
        std::puts("Production MTA ped give-weapon policy fixture completed");
        std::printf("Passed %u actual MTA ped give-weapon policy checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ped give-weapon policy failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
