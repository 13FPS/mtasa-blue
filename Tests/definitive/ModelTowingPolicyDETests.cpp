/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Test actual portable towing dispatch without inferred native subtypes
 *****************************************************************************/
#include "../../Client/game_de/CModelInfoDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include "ModelTowingFixture.generated.h"

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
    struct Refusal : std::runtime_error
    {
        Refusal() : std::runtime_error("required classifier refused") {}
    };
    struct Log
    {
        std::array<unsigned, 16> entries{};
        std::size_t              count = 0;
        void                     Push(unsigned value) noexcept
        {
            if (count >= entries.size())
                std::abort();
            entries[count++] = value;
        }
        void Expect(std::initializer_list<unsigned> expected)
        {
            Check(count == expected.size(), "portable policy performs exactly the required virtual calls");
            std::size_t index = 0;
            for (const auto value : expected)
                Check(entries[index++] == value, "portable policy preserves actual SA call order and short circuits");
        }
    };
    struct Owner final : NativeModelClassificationAccess
    {
        explicit Owner(Log& events) noexcept : log(events) {}
        ClockResult ReadModelVehiclePredicate(std::uint32_t key, NativeModelVehiclePredicate predicate, bool& output) noexcept override
        {
            CheckInput(key, predicate);
            log.Push(102);
            if (status != ClockResult::Ok)
                return status;
            output = boat;
            return ClockResult::Ok;
        }
        void CheckInput(std::uint32_t key, NativeModelVehiclePredicate predicate) noexcept
        {
            if (key != expectedModel || predicate != NativeModelVehiclePredicate::Boat)
                std::abort();
        }
        Log&          log;
        std::uint32_t expectedModel = 400;
        bool          boat = false;
        ClockResult   status = ClockResult::Ok;
    };
    class Model final : public ModelTowingFixtureBase
    {
    public:
        Model(std::uint32_t key, NativeModelClassificationAccess& owner, Log& events, unsigned label) noexcept
            : ModelTowingFixtureBase(key, owner), id(key), log(events), tag(label)
        {
        }
        DWORD GetModel() override
        {
            log.Push(tag);
            if (++modelReads == failModelRead)
                throw Refusal();
            return id;
        }
        bool IsTrain() override { return Predicate(1, train); }
        bool IsBike() override { return Predicate(3, bike); }
        bool IsBmx() override { return Predicate(4, bmx); }
        bool IsTrailer() override { return Predicate(5, trailer); }
        // Only the real portable method is under test. Required virtual
        // classifiers are controlled inputs; no native subtype mapping or
        // completed model factory is simulated by this strict stack receiver.
        std::uint32_t id;
        Log&          log;
        unsigned      tag, modelReads = 0, failModelRead = 0, failPredicate = 0;
        bool          train = false, bike = false, bmx = false, trailer = false;

    private:
        bool Predicate(unsigned operation, bool result)
        {
            log.Push(tag + operation);
            if (operation == failPredicate)
                throw Refusal();
            return result;
        }
    };
    static_assert(std::is_same_v<decltype(&Model::IsTowableBy), decltype(&CModelInfoDE::IsTowableBy)>);
    static_assert(std::is_same_v<decltype(&Model::IsBoat), decltype(&CModelInfoDE::IsBoat)>);
    static_assert(std::is_abstract_v<CModelInfoDE> && !std::is_abstract_v<Model>);
    struct Scenario
    {
        Log                             log;
        Owner                           targetOwner{log};
        NativeModelClassificationAccess towingOwner;
        Model                           target;
        Model                           towing;
        explicit Scenario(std::uint32_t targetKey = 400, std::uint32_t towingKey = 525)
            : target(targetKey, targetOwner, log, 100), towing(towingKey, towingOwner, log, 200)
        {
            targetOwner.expectedModel = targetKey;
        }
        bool Towable()
        {
            CModelInfo& sdk = target;
            return sdk.IsTowableBy(&towing);
        }
    };
    template <class Action>
    void Refuses(Action action)
    {
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            Check(true, "required classifier exception remains visible rather than a fabricated result");
            return;
        }
        Check(false, "required classifier refusal was lost");
    }
    void Decisions()
    {
        {
            Scenario s(400, 999);
            Check(s.Towable(), "ordinary nontrain towing is allowed without unrelated vehicle classifiers");
            s.log.Expect({200, 200, 101, 201});
        }
        {
            Scenario s;
            s.target.train = true;
            Check(!s.Towable(), "a towed train is excluded before querying towing train state");
            s.log.Expect({200, 200, 101});
        }
        {
            Scenario s;
            s.towing.train = true;
            Check(!s.Towable(), "a towing train is excluded before the special vehicle branch");
            s.log.Expect({200, 200, 101, 201});
        }
        for (const std::uint32_t towing : {525u, 531u})
        {
            for (const std::uint32_t target : {400u, 610u, 0xffffffffu})
            {
                Scenario ordinary(target, towing);
                Check(ordinary.Towable(), "towtruck/tractor can tow an ordinary nonexcluded target");
                ordinary.log.Expect({200, 200, 101, 201, 100, 102, 103, 104, 105});
                Scenario trailer(target, towing);
                trailer.target.trailer = true;
                Check(trailer.Towable() == (towing == 531 && target == 610), "only tractor+farm-trailer bypasses special trailer exclusion");
                trailer.log.Expect({200, 200, 101, 201, 100, 102, 103, 104, 105});
            }
            Scenario boat(610, towing);
            boat.targetOwner.boat = true;
            Check(!boat.Towable(), "boat excludes before bike/bmx/trailer even for farm trailer model ID");
            boat.log.Expect({200, 200, 101, 201, 100, 102});
            Scenario bike(610, towing);
            bike.target.bike = true;
            Check(!bike.Towable(), "bike excludes before bmx/trailer");
            bike.log.Expect({200, 200, 101, 201, 100, 102, 103});
            Scenario bmx(610, towing);
            bmx.target.bmx = true;
            Check(!bmx.Towable(), "bmx excludes before trailer");
            bmx.log.Expect({200, 200, 101, 201, 100, 102, 103, 104});
        }
    }
    void FailurePrefixes()
    {
        for (unsigned call : {1u, 2u})
        {
            Scenario s;
            s.towing.failModelRead = call;
            Refuses([&] { s.Towable(); });
            if (call == 1)
                s.log.Expect({200});
            else
                s.log.Expect({200, 200});
        }
        {
            Scenario s;
            s.target.failPredicate = 1;
            Refuses([&] { s.Towable(); });
            s.log.Expect({200, 200, 101});
        }
        {
            Scenario s;
            s.towing.failPredicate = 1;
            Refuses([&] { s.Towable(); });
            s.log.Expect({200, 200, 101, 201});
        }
        {
            Scenario s;
            s.target.failModelRead = 1;
            Refuses([&] { s.Towable(); });
            s.log.Expect({200, 200, 101, 201, 100});
        }
        {
            Scenario s;
            s.targetOwner.status = ClockResult::BindingChanged;
            Refuses([&] { s.Towable(); });
            s.log.Expect({200, 200, 101, 201, 100, 102});
        }
        for (unsigned operation : {3u, 4u, 5u})
        {
            Scenario s;
            s.target.failPredicate = operation;
            Refuses([&] { s.Towable(); });
            if (operation == 3)
                s.log.Expect({200, 200, 101, 201, 100, 102, 103});
            else if (operation == 4)
                s.log.Expect({200, 200, 101, 201, 100, 102, 103, 104});
            else
                s.log.Expect({200, 200, 101, 201, 100, 102, 103, 104, 105});
        }
    }
}
int main()
{
    try
    {
        Decisions();
        FailurePrefixes();
        std::puts("Actual MTA model towing policy SDK fixture completed");
        std::printf("Passed %u actual MTA model towing SDK checks (%zu-bit; virtual policy dispatch)\n", checks, sizeof(void*) * 8);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
