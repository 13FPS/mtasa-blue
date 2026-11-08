/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise six real SDK overrides through one retained model owner
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoObjectGroupDE.h"
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ModelObjectGroupFixture.generated.h"

using namespace mta::game_de;
static_assert(sizeof(void*) == 8, "DE SDK fixtures must use the actual x64 native graph");
static_assert(std::is_abstract_v<CModelInfoObjectGroupDE>);
static_assert(std::is_convertible_v<CModelInfoObjectGroupDE*, CModelInfo*>);
static_assert(!std::is_abstract_v<ModelObjectGroupObserver>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    template <class Action>
    void Refuses(Action&& action, const char* operation, ClockResult status)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = error.what() == std::string("DE ") + operation + ": " + std::to_string(static_cast<std::uint32_t>(status));
        }
        Check(refused, "real SDK override preserves the exact owner's explicit status");
    }
    struct Owner final : NativeModelClassificationAccess, NativeModelWheelAccess, NativeModelObjectGroupAccess
    {
        std::uint32_t key = 0;
        std::uint16_t group = 0xffff;
        ClockResult   status = ClockResult::Ok;
        unsigned      reads = 0, sets = 0, restores = 0, classifications = 0, wheelReads = 0, wheelSets = 0, wheelResets = 0;
        ClockResult   ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++classifications;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelObjectPropertiesGroup(std::uint32_t model, std::uint16_t& output) noexcept override
        {
            ++reads;
            key = model;
            output = group;
            return status;
        }
        ClockResult SetModelObjectPropertiesGroup(std::uint32_t model, std::uint16_t value) noexcept override
        {
            ++sets;
            key = model;
            group = value;
            return status;
        }
        ClockResult RestoreModelObjectPropertiesGroup(std::uint32_t model) noexcept override
        {
            ++restores;
            key = model;
            return status;
        }
        ClockResult ReadModelWheelSizes(std::uint32_t model, NativeModelWheelSizes& output) noexcept override
        {
            ++wheelReads;
            key = model;
            output = {1.5f, 0.75f};
            return ClockResult::Ok;
        }
        ClockResult SetModelWheelSize(std::uint32_t model, NativeModelWheelGroup selected, float value) noexcept override
        {
            ++wheelSets;
            key = model;
            return selected == NativeModelWheelGroup::All && value == 2.25f ? ClockResult::Ok : ClockResult::InvalidArgument;
        }
        ClockResult ResetModelWheelSizes(std::uint32_t model, const NativeModelWheelSizes* defaults) noexcept override
        {
            ++wheelResets;
            key = model;
            return defaults == nullptr ? ClockResult::Ok : ClockResult::InvalidArgument;
        }
    };
    struct OlderOwner final : NativeModelClassificationAccess, NativeModelWheelAccess, NativeModelObjectGroupAccess
    {
    };
    void Sdk()
    {
        Owner owner, other;
        bool  rejected = false;
        try
        {
            ModelObjectGroupObserver bad(400, owner, owner, other);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        Check(rejected && !owner.reads && !other.sets, "unrelated object-group writer cannot join a model facade");
        rejected = false;
        try
        {
            ModelObjectGroupObserver bad(400, owner, other, owner);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        Check(rejected && !owner.wheelReads && !other.wheelSets, "linear group composition preserves the wheel owner's identity check");
        constexpr std::uint32_t  immutable = 0x12345678;
        ModelObjectGroupObserver concrete(immutable, owner, owner, owner);
        CModelInfo&              model = concrete;
        Check(model.GetModel() == 0xfedcba98, "fixture deliberately poisons the public virtual GetModel");
        Check(model.GetObjectPropertiesGroup() == 0xffff && owner.key == immutable && owner.reads == 1,
              "actual group getter returns the full static sentinel using immutable model identity");
        owner.group = 0x1234;
        Check(model.GetObjectPropertiesGroup() == 0x1234 && owner.key == immutable && owner.reads == 2, "getter retains upper WORD bits");
        concrete.reportedModel = 7;
        for (const unsigned short raw :
             {static_cast<unsigned short>(0), static_cast<unsigned short>(256), static_cast<unsigned short>(0xfedc), static_cast<unsigned short>(0xffff)})
        {
            const auto reads = owner.reads;
            model.SetObjectPropertiesGroup(raw);
            Check(owner.group == raw && owner.key == immutable && owner.reads == reads,
                  "SDK Set forwards the raw WORD and leaves native same-value/cache policy to its one owner");
        }
        const auto reads = owner.reads;
        model.RestoreObjectPropertiesGroup();
        Check(owner.restores == 1 && owner.key == immutable && owner.reads == reads, "SDK restore delegates once without a duplicate cache or read");
        Check(!owner.classifications && !owner.wheelReads && !owner.wheelSets && !owner.wheelResets,
              "group SDK methods add no classification, load or wheel prerequisites");
        Check(model.GetVehicleWheelSize(ResizableVehicleWheelGroup::FRONT_AXLE) == 1.5f && owner.key == immutable,
              "inherited production wheel getter shares the same immutable model facade");
        model.SetVehicleWheelSize(ResizableVehicleWheelGroup::ALL_WHEELS, 2.25f);
        Check(owner.wheelSets == 1 && owner.key == immutable, "inherited production wheel setter reaches the same owner");
        model.ResetVehicleWheelSizes();
        Check(owner.wheelResets == 1 && owner.key == immutable, "inherited production wheel reset survives linear group composition");
        for (std::uint32_t code = 1; code <= static_cast<std::uint32_t>(ClockResult::OriginalException); ++code)
        {
            owner.status = static_cast<ClockResult>(code);
            Refuses([&] { static_cast<void>(model.GetObjectPropertiesGroup()); }, "GetObjectPropertiesGroup", owner.status);
            Refuses([&] { model.SetObjectPropertiesGroup(0xffff); }, "SetObjectPropertiesGroup", owner.status);
            Refuses([&] { model.RestoreObjectPropertiesGroup(); }, "RestoreObjectPropertiesGroup", owner.status);
        }
        OlderOwner               older;
        ModelObjectGroupObserver missing(7, older, older, older);
        Refuses([&] { static_cast<void>(missing.GetObjectPropertiesGroup()); }, "GetObjectPropertiesGroup", ClockResult::NotInitialized);
        Refuses([&] { missing.SetObjectPropertiesGroup(0); }, "SetObjectPropertiesGroup", ClockResult::NotInitialized);
        Refuses([&] { missing.RestoreObjectPropertiesGroup(); }, "RestoreObjectPropertiesGroup", ClockResult::NotInitialized);
        Check(!owner.classifications, "no SDK path manufactures classification to hide an owner refusal");
    }
}
int main()
{
    try
    {
        Sdk();
        std::puts("Actual model object-group and inherited wheel SDK fixture completed");
        std::printf("Passed %u actual MTA model object-group SDK checks (%u-bit; native access doubled)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
