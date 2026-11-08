/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual seven dummy SDK methods with real loading/ref effect boundaries
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoDummiesDE.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "ModelDummiesFixture.generated.h"
using namespace mta::game_de;
static_assert(sizeof(void*) == 8 && std::is_abstract_v<CModelInfoDummiesDE> && !std::is_abstract_v<ModelDummiesObserver>);
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
        Check(refused, "actual dummy SDK method preserves the exact native refusal");
    }
    struct Owner final : NativeModelClassificationAccess, NativeModelWheelAccess, NativeModelObjectGroupAccess, NativeModelDummiesAccess
    {
        NativeModelDummyPositions positions{};
        NativeModelDummyVector    cached{44, 55, 66}, set{};
        bool                      cacheFound = false, remove = false;
        ClockResult               status = ClockResult::Ok, cacheStatus = ClockResult::Ok;
        std::uint32_t             key = 0, index = 0;
        unsigned                  calls = 0;
        std::vector<std::string>* effects = nullptr;
        ClockResult               ReadModelVehicleDummyPositions(std::uint32_t model, NativeModelDummyPositions& output) noexcept override
        {
            ++calls;
            key = model;
            if (effects)
                effects->emplace_back("read");
            output = positions;
            return status;
        }
        ClockResult ReadCachedModelVehicleDummyDefault(std::uint32_t model, std::uint32_t dummy, NativeModelDummyVector& output, bool& found) noexcept override
        {
            ++calls;
            key = model;
            index = dummy;
            if (effects)
                effects->emplace_back("cache");
            found = cacheFound;
            if (found)
                output = cached;
            return cacheStatus;
        }
        ClockResult SetModelVehicleDummyPosition(std::uint32_t model, std::uint32_t dummy, const NativeModelDummyVector& value) noexcept override
        {
            ++calls;
            key = model;
            index = dummy;
            set = value;
            if (effects)
                effects->emplace_back("set");
            return status;
        }
        ClockResult ResetModelVehicleDummies(std::uint32_t model, bool selected) noexcept override
        {
            ++calls;
            key = model;
            remove = selected;
            if (effects)
                effects->emplace_back("reset");
            return status;
        }
        ClockResult ReadModelWheelSizes(std::uint32_t model, NativeModelWheelSizes& output) noexcept override
        {
            key = model;
            output = {2, 3};
            return ClockResult::Ok;
        }
        ClockResult ReadModelObjectPropertiesGroup(std::uint32_t model, std::uint16_t& output) noexcept override
        {
            key = model;
            output = 0xabcd;
            return ClockResult::Ok;
        }
    };
    struct OlderOwner final : NativeModelClassificationAccess, NativeModelWheelAccess, NativeModelObjectGroupAccess, NativeModelDummiesAccess
    {
    };
    void Sdk()
    {
        Owner owner, other;
        bool  rejected = false;
        try
        {
            ModelDummiesObserver bad(400, owner, owner, owner, other);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        Check(rejected && owner.calls == 0 && other.calls == 0, "unrelated dummy owner cannot join the linear wheel/group/model facade");
        constexpr std::uint32_t immutable = 0x12345678;
        ModelDummiesObserver    concrete(immutable, owner, owner, owner, owner);
        owner.effects = &concrete.effects;
        owner.positions[6] = {7, 8, 9};
        CModelInfo& model = concrete;
        Check(model.GetModel() == 0xfedcba98, "fixture deliberately poisons the public virtual model key");
        concrete.loaded = false;
        const auto current = model.GetVehicleDummyPosition(VehicleDummies::EXHAUST);
        Check(current.fX == 7 && owner.key == immutable && concrete.effects == std::vector<std::string>{"loaded", "request:GetVehicleDummyPosition", "read"},
              "current getter preserves real blocking Request before its immutable native vector key");
        concrete.effects.clear();
        concrete.loaded = false;
        model.SetVehicleDummyPosition(VehicleDummies::ENGINE, CVector(10, 11, 12));
        Check(owner.key == immutable && owner.index == 7 && owner.set.z == 12 &&
                  concrete.effects == std::vector<std::string>{"loaded", "request:SetVehicleDummyPosition", "set"},
              "setter keeps real loading policy before the one retained owner cache/store operation");
        concrete.effects.clear();
        concrete.loaded = false;
        const auto original = model.GetVehicleDummyDefaultPosition(VehicleDummies::EXHAUST);
        Check(original.fZ == 9 && owner.key == immutable && concrete.references == 0 &&
                  concrete.effects == std::vector<std::string>{"cache", "add:GetVehicleDummyDefaultPosition", "read", "remove"},
              "uncached default uses actual ModelAddRef/read/RemoveRef boundaries with no replacement load algorithm");
        concrete.effects.clear();
        owner.cacheFound = true;
        const auto cached = model.GetVehicleDummyDefaultPosition(VehicleDummies::EXHAUST);
        Check(cached.fX == 44 && cached.fY == 55 && concrete.effects == std::vector<std::string>{"cache"},
              "cached default returns the first original without another model reference or native vector read");
        concrete.effects.clear();
        concrete.loaded = false;
        std::array<CVector, static_cast<std::size_t>(VehicleDummies::VEHICLE_DUMMY_COUNT)> all{};
        Check(model.GetVehicleDummyPositions(all) && all[6].fY == 8 && concrete.effects == std::vector<std::string>{"read"},
              "const bulk getter copies fourteen vectors without inventing a load/ref step");
        concrete.effects.clear();
        concrete.loaded = true;
        Check(model.GetVehicleExhaustFumesPosition().fX == 7, "exhaust getter aliases the actual EXHAUST dummy");
        model.SetVehicleExhaustFumesPosition(CVector(13, 14, 15));
        Check(owner.index == 6 && owner.set.z == 15, "exhaust setter aliases the same actual dummy mutation owner");
        model.ResetVehicleDummies(false);
        Check(!owner.remove && owner.key == immutable, "Reset(false) preserves bulk cache-removal intent");
        model.ResetVehicleDummies(true);
        Check(owner.remove && owner.key == immutable, "Reset(true) preserves explicit single cache-removal intent");
        Check(model.GetVehicleWheelSize(ResizableVehicleWheelGroup::FRONT_AXLE) == 2 && owner.key == immutable && model.GetObjectPropertiesGroup() == 0xabcd &&
                  owner.key == immutable,
              "inherited production wheel/group methods keep one immutable model base beneath the dummy layer");
        for (std::uint32_t code = 1; code <= static_cast<std::uint32_t>(ClockResult::OriginalException); ++code)
        {
            owner.status = static_cast<ClockResult>(code);
            Refuses([&] { static_cast<void>(model.GetVehicleDummyPosition(VehicleDummies::EXHAUST)); }, "GetVehicleDummyPosition", owner.status);
            Refuses([&] { model.SetVehicleDummyPosition(VehicleDummies::EXHAUST, CVector()); }, "SetVehicleDummyPosition", owner.status);
            Refuses([&] { model.ResetVehicleDummies(true); }, "ResetVehicleDummies", owner.status);
            all[6] = CVector(101, 102, 103);
            Refuses([&] { static_cast<void>(model.GetVehicleDummyPositions(all)); }, "GetVehicleDummyPositions", owner.status);
            Check(all[6].fX == 101, "refused bulk read preserves the caller's entire output");
        }
        owner.status = ClockResult::ReadFailure;
        owner.cacheFound = false;
        concrete.effects.clear();
        Refuses([&] { static_cast<void>(model.GetVehicleDummyDefaultPosition(VehicleDummies::EXHAUST)); }, "GetVehicleDummyDefaultPosition", owner.status);
        Check(concrete.references == 1 && concrete.effects == std::vector<std::string>{"cache", "add:GetVehicleDummyDefaultPosition", "read"},
              "read refusal preserves the already admitted temporary reference instead of guessing exception-time cleanup");
        owner.status = ClockResult::Ok;
        concrete.effects.clear();
        const auto calls = owner.calls;
        Refuses([&] { model.SetVehicleDummyPosition(static_cast<VehicleDummies::Enum>(14), CVector()); }, "vehicle dummy index", ClockResult::InvalidArgument);
        Check(owner.calls == calls && concrete.effects.empty(), "native's extra slot14 is outside the real fourteen-ID SDK operation");
        concrete.vehicle = false;
        Check(model.GetVehicleDummyPosition(VehicleDummies::EXHAUST).fX == 0 && !model.GetVehicleDummyPositions(all),
              "nonvehicle predicates preserve the original zero/false SDK results");
        model.SetVehicleDummyPosition(VehicleDummies::EXHAUST, CVector());
        model.ResetVehicleDummies(true);
        Check(owner.calls == calls, "nonvehicle SDK operations enter no dummy owner effects");
        OlderOwner           older;
        ModelDummiesObserver missing(7, older, older, older, older);
        Refuses([&] { static_cast<void>(missing.GetVehicleDummyDefaultPosition(VehicleDummies::EXHAUST)); }, "GetVehicleDummyDefaultPosition",
                ClockResult::NotInitialized);
        Check(missing.references == 0, "missing cached-default capability cannot manufacture a miss and enter temporary reference work");
    }
}
int main()
{
    try
    {
        Sdk();
        std::puts("Actual model dummy and inherited wheel/group SDK fixture completed");
        std::printf("Passed %u actual MTA model dummy SDK checks (%u-bit; native access doubled)\n", checks, static_cast<unsigned>(sizeof(void*) * 8));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
