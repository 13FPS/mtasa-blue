/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual seven named vehicle SDK predicates over the existing owner
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include "../../Client/game_de/NativeModelVehicleNamedProfile.h"
#include <array>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ModelNamedPredicateFixture.generated.h"
using namespace mta::game_de;
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsTrain), decltype(&CModelInfoDE::IsTrain)>);
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsBike), decltype(&CModelInfoDE::IsBike)>);
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsBmx), decltype(&CModelInfoDE::IsBmx)>);
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsTrailer), decltype(&CModelInfoDE::IsTrailer)>);
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsMonsterTruck), decltype(&CModelInfoDE::IsMonsterTruck)>);
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsQuadBike), decltype(&CModelInfoDE::IsQuadBike)>);
static_assert(std::is_same_v<decltype(&NamedVehiclePredicateObserver::IsFakePlane), decltype(&CModelInfoDE::IsFakePlane)>);
namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
    bool Query(CModelInfo& model, NativeModelVehiclePredicate predicate)
    {
        switch (predicate)
        {
            case NativeModelVehiclePredicate::Boat:
                return model.IsBoat();
            case NativeModelVehiclePredicate::Car:
                return model.IsCar();
            case NativeModelVehiclePredicate::Plane:
                return model.IsPlane();
            case NativeModelVehiclePredicate::Heli:
                return model.IsHeli();
            case NativeModelVehiclePredicate::Train:
                return model.IsTrain();
            case NativeModelVehiclePredicate::Bike:
                return model.IsBike();
            case NativeModelVehiclePredicate::Bmx:
                return model.IsBmx();
            case NativeModelVehiclePredicate::Trailer:
                return model.IsTrailer();
            case NativeModelVehiclePredicate::MonsterTruck:
                return model.IsMonsterTruck();
            case NativeModelVehiclePredicate::QuadBike:
                return model.IsQuadBike();
            case NativeModelVehiclePredicate::FakePlane:
                return model.IsFakePlane();
        }
        throw std::runtime_error("unknown SDK fixture predicate");
    }
    struct Access final : NativeModelClassificationAccess
    {
        std::uint32_t               key = 0;
        NativeModelVehiclePredicate received = NativeModelVehiclePredicate::Boat;
        ClockResult                 status = ClockResult::Ok;
        unsigned                    calls = 0, unrelated = 0;
        bool                        value = false;
        ClockResult                 ReadModelVehiclePredicate(std::uint32_t index, NativeModelVehiclePredicate predicate, bool& output) noexcept override
        {
            ++calls;
            key = index;
            received = predicate;
            output = status == ClockResult::Ok ? value : !value;
            return status;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelVehicleSubtype(std::uint32_t, std::uint32_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelInterface(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
        ClockResult ReadModelRender(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unrelated;
            return ClockResult::ReadFailure;
        }
    };
    template <class F>
    void Refuses(F&& f, ClockResult status)
    {
        bool refused = false;
        try
        {
            f();
        }
        catch (const std::runtime_error& e)
        {
            const std::string text = e.what(), tail = ": " + std::to_string(static_cast<unsigned>(status));
            refused = text.find("DE model owner refused vehicle predicate") != std::string::npos && text.size() >= tail.size() &&
                      text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
        }
        Check(refused, "SDK preserves the exact predicate-owner refusal without interpreting a poisoned result");
    }
    void Cases()
    {
        constexpr std::array<NativeModelVehiclePredicate, 11> predicates{
            NativeModelVehiclePredicate::Boat,     NativeModelVehiclePredicate::Car,      NativeModelVehiclePredicate::Plane,
            NativeModelVehiclePredicate::Heli,     NativeModelVehiclePredicate::Train,    NativeModelVehiclePredicate::Bike,
            NativeModelVehiclePredicate::Bmx,      NativeModelVehiclePredicate::Trailer,  NativeModelVehiclePredicate::MonsterTruck,
            NativeModelVehiclePredicate::QuadBike, NativeModelVehiclePredicate::FakePlane};
        Access access;
        for (const auto key : std::array<std::uint32_t, 4>{0, 19999, 20000, 0xffffffffu})
        {
            NamedVehiclePredicateObserver observer(key, access);
            CModelInfo&                   sdk = observer;
            for (const auto predicate : predicates)
                for (const bool value : {false, true})
                {
                    access.value = value;
                    const auto before = access.calls;
                    Check(Query(sdk, predicate) == value && access.calls == before + 1 && access.key == key && access.received == predicate,
                          "all seven named and four existing real SDK predicates forward fullDWORD and exact intent once");
                }
        }
        NamedVehiclePredicateObserver observer(17, access);
        for (const auto predicate : predicates)
            for (const auto status :
                 {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                  ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                  ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
            {
                access.status = status;
                const auto before = access.calls;
                Refuses([&] { Query(observer, predicate); }, status);
                Check(access.calls == before + 1 && access.key == 17 && access.received == predicate,
                      "refusals neither retry nor substitute a different owner predicate");
            }
        NativeModelClassificationAccess absent;
        NamedVehiclePredicateObserver   missing(17, absent);
        for (const auto predicate : predicates)
            Refuses([&] { Query(missing, predicate); }, ClockResult::NotInitialized);
        Check(!access.unrelated, "classification never substitutes archive/type/lowBYTE/raw interface/render or factory channels");
        Access other;
        other.value = true;
        NamedVehiclePredicateObserver independent(0x80000000u, other);
        const auto                    before = access.calls;
        Check(independent.IsTrain() && other.calls == 1 && other.key == 0x80000000u && other.received == NativeModelVehiclePredicate::Train &&
                  access.calls == before,
              "an independent actual SDK receiver retains its exact supplied classification owner");
        std::puts("Actual model info named predicates SDK fixture completed");
        std::puts("Actual same-owner named predicate dispatch SDK fixture completed");
        std::puts("Actual named predicate refusal transport SDK fixture completed");
    }
}
int main()
{
    try
    {
        Cases();
        std::printf("Passed %u named vehicle predicate SDK checks (%zu-bit; transport only)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL after %u named vehicle predicate SDK checks: %s\n", checks, e.what());
        return 1;
    }
}
