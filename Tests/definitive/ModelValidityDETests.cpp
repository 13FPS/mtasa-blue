/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual IsValid policy through existing mandatory SDK observations
 ****************************************************************************/
#include "../../Client/game_de/CModelInfoRequestDE.h"
#include <vector>
#include <game/CGame.h>
#include <game/CPools.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fixture
{
    [[noreturn]] inline void Unqualified() noexcept
    {
        std::fputs("FAIL: unrelated SDK operation reached model validity fixture\n", stderr);
        std::abort();
    }
}
#include "ModelRequestFixture.generated.h"

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
    struct Trace
    {
        std::array<unsigned, 16> calls{};
        std::size_t              count = 0;
        void                     Add(unsigned call) noexcept
        {
            if (count == calls.size())
                fixture::Unqualified();
            calls[count++] = call;
        }
        void Expect(std::initializer_list<unsigned> expected)
        {
            Check(count == expected.size() && std::equal(expected.begin(), expected.end(), calls.begin()),
                  "actual IsValid retains the source's exact branch order and no fallback/retry calls");
            count = 0;
        }
    };
    struct Failure : std::runtime_error
    {
        explicit Failure(unsigned selected) : std::runtime_error("existing SDK owner refused"), phase(selected) {}
        unsigned phase;
    };
    template <class Action>
    void Fails(Action action, unsigned expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const Failure& error)
        {
            refused = error.phase == expected;
        }
        Check(refused, "original required SDK failure propagates from its exact phase without fallback");
    }
    template <class Action>
    void Refuses(Action action)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error&)
        {
            refused = true;
        }
        Check(refused, "missing/refused current model or pool authority never becomes a successful validity answer");
    }
    struct Txd final : fixture::TxdFixtureBase
    {
        explicit Txd(Trace& trace) : trace(trace) {}
        bool IsFreeTextureDictonarySlot(std::uint32_t key) override
        {
            trace.Add(3);
            lastKey = key;
            if (failure)
                throw Failure(3);
            return free;
        }
        Trace&        trace;
        std::uint32_t lastKey = 0xffffffffu;
        bool          free = false, failure = false;
    };
    struct Pools final : fixture::PoolsFixtureBase
    {
        Pools(Trace& trace, Txd& txd) : trace(trace), txd(txd) {}
        CTxdPool& GetTxdPool() noexcept override
        {
            trace.Add(2);
            return txd;
        }
        Trace& trace;
        Txd&   txd;
    };
    struct Game final : fixture::GameFixtureBase
    {
        explicit Game(Trace& trace) : trace(trace) {}
        CPools* GetPools() const noexcept override
        {
            trace.Add(1);
            return pools;
        }
        std::uint32_t GetBaseIDforTXD() override
        {
            trace.Add(4);
            if (failure == 4)
                throw Failure(4);
            return txdBase;
        }
        std::uint32_t GetCountOfAllFileIDs() override
        {
            trace.Add(5);
            if (failure == 5)
                throw Failure(5);
            return fileCount;
        }
        Trace&        trace;
        CPools*       pools = nullptr;
        std::uint32_t txdBase = 20000, fileCount = 30004;
        unsigned      failure = 0;
    };
    struct Access final : NativeModelClassificationAccess
    {
        explicit Access(Trace& trace) : trace(trace) {}
        ClockResult ReadModelInterface(std::uint32_t key, std::uintptr_t& output) noexcept override
        {
            trace.Add(6);
            lastKey = key;
            ++reads;
            if (status == ClockResult::Ok)
                output = tag;
            return status;
        }
        ClockResult ReadModelRender(std::uint32_t, std::uintptr_t&) noexcept override
        {
            ++unexpected;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelType(std::uint32_t, NativeModelTypeObservation&) noexcept override
        {
            ++unexpected;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelVehiclePredicate(std::uint32_t, NativeModelVehiclePredicate, bool&) noexcept override
        {
            ++unexpected;
            return ClockResult::NotInitialized;
        }
        ClockResult ReadModelArchiveAllocated(std::uint32_t, bool&) noexcept override
        {
            ++unexpected;
            return ClockResult::NotInitialized;
        }
        Trace&         trace;
        std::uint32_t  lastKey = 0;
        std::uintptr_t tag = 0;
        unsigned       reads = 0, unexpected = 0;
        ClockResult    status = ClockResult::Ok;
    };
    struct Model final : fixture::ModelFixtureBase
    {
        Model(std::uint32_t key, NativeModelClassificationAccess& owner, CGame& game) : ModelFixtureBase(key, owner, game) {}

    protected:
        void RequestModelDependencies(EModelRequestType, const char*) override { fixture::Unqualified(); }
        void ReportBlockingModelLoad(bool, int, const char*) override { fixture::Unqualified(); }
        void IncrementNativeModelInterfaceReference() override { fixture::Unqualified(); }
        void RemoveNativeModelInterfaceReference() override { fixture::Unqualified(); }
    };
    static_assert(std::is_same_v<decltype(&Model::IsValid), decltype(&CModelInfoRequestDE::IsValid)>);
    static_assert(std::is_same_v<decltype(&Model::GetInterface), decltype(&CModelInfoDE::GetInterface)>);
    static_assert(std::is_abstract_v<CModelInfoRequestDE> && !std::is_abstract_v<Model>);
}
int main()
{
    try
    {
        Trace trace;
        Txd   txd(trace);
        Pools pools(trace, txd);
        Game  game(trace);
        game.pools = &pools;
        Access access(trace);
        for (const auto key : {20000u, 20001u, 24999u})
        {
            Model       model(key, access, game);
            CModelInfo& sdk = model;
            // Fixed TXD occupancy precedes even deliberately incompatible
            // dynamic file bounds, with no interface/native data substitute.
            game.txdBase = 0;
            game.fileCount = 0;
            for (const bool free : {false, true})
            {
                txd.free = free;
                Check(sdk.IsValid() == !free && txd.lastKey == key - 20000,
                      "actual IsValid uses exact fixed TXD slot offset and occupancy inversion at both boundaries");
                trace.Expect({1, 2, 3});
            }
        }
        game.txdBase = 20000;
        game.fileCount = 30004;
        for (const auto key : {0u, 19999u})
        {
            Model       model(key, access, game);
            CModelInfo& sdk = model;
            for (const auto tag : {std::uintptr_t{0}, std::uintptr_t{1}, std::uintptr_t{0xfffffffffffffff0ull}})
            {
                access.tag = tag;
                Check(sdk.IsValid() == (tag != 0) && access.lastKey == key,
                      "actual DFF validity observes current opaque null/poison interface without dereference or stale cache");
                trace.Expect({4, 6});
            }
        }
        for (const auto key : {25000u, 30003u})
        {
            Model model(key, access, game);
            Check(model.IsValid(), "dynamic non-DFF file range is valid without model interface/type/archive/loaded queries");
            trace.Expect({4, 5});
        }
        Model countBoundary(30004, access, game);
        access.tag = 0;
        Check(!countBoundary.IsValid() && access.lastKey == 30004, "exact file-count boundary falls through to current interface observation");
        trace.Expect({4, 5, 6});
        game.txdBase = 60000;
        game.fileCount = 70000;
        Model fullWidth(65536, access, game);
        Check(fullWidth.IsValid(), "dynamic game bounds preserve the full DWORD key rather than narrowing it to a model WORD");
        trace.Expect({4, 5});
        game.txdBase = 20000;
        game.fileCount = 30004;
        access.status = ClockResult::InvalidArgument;
        Refuses([&] { (void)fullWidth.IsValid(); });
        Check(access.lastKey == 65536, "out-of-domain current interface refusal receives the original full key");
        trace.Expect({4, 5, 6});
        Model maximum(0xffffffffu, access, game);
        Refuses([&] { (void)maximum.IsValid(); });
        Check(access.lastKey == 0xffffffffu, "maximum DWORD is not clamped into a valid bank key");
        trace.Expect({4, 5, 6});
        Model dff(17, access, game);
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            access.status = static_cast<ClockResult>(status);
            Refuses([&] { (void)dff.IsValid(); });
            trace.Expect({4, 6});
        }
        access.status = ClockResult::Ok;
        Model selectedTxd(20000, access, game);
        txd.failure = true;
        Fails([&] { (void)selectedTxd.IsValid(); }, 3);
        trace.Expect({1, 2, 3});
        txd.failure = false;
        game.pools = nullptr;
        Refuses([&] { (void)selectedTxd.IsValid(); });
        trace.Expect({1});
        game.pools = &pools;
        game.failure = 4;
        Fails([&] { (void)dff.IsValid(); }, 4);
        trace.Expect({4});
        game.failure = 5;
        Model nonDff(25000, access, game);
        Fails([&] { (void)nonDff.IsValid(); }, 5);
        trace.Expect({4, 5});
        game.failure = 0;
        NativeModelClassificationAccess absent;
        Model                           missing(17, absent, game);
        Refuses([&] { (void)missing.IsValid(); });
        trace.Expect({4});
        Check(!access.unexpected, "validity never asks render, model type, archive or native classification as an invented readiness policy");
        std::puts("Production actual model validity SDK fixture completed");
        std::printf("Passed %u actual model validity SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Model validity SDK fixture failed: %s\n", error.what());
        return 1;
    }
}
