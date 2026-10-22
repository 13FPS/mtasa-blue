/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Verify actual streaming SDK dispatch without executing game code
 ****************************************************************************/
#include "../../Client/game_de/CStreamingDE.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }
    struct Access final : NativeStreamingModelAccess
    {
        unsigned      requests = 0, queries = 0, upgradeRequests = 0, upgradeQueries = 0, loads = 0, removes = 0;
        std::uint32_t model = 0, flags = 0;
        ClockResult   requestResult = ClockResult::Ok, queryResult = ClockResult::Ok;
        bool          loaded = false, lastOnlyPriority = true;
        ClockResult   RemoveStreamingModel(std::uint32_t index) noexcept override
        {
            ++removes;
            model = index;
            return requestResult;
        }
        ClockResult LoadStreamingModels(bool onlyPriority) noexcept override
        {
            ++loads;
            lastOnlyPriority = onlyPriority;
            return requestResult;
        }
        ClockResult RequestStreamingModel(std::uint32_t index, std::uint32_t requestFlags) noexcept override
        {
            ++requests;
            model = index;
            flags = requestFlags;
            return requestResult;
        }
        ClockResult ReadStreamingModelLoaded(std::uint32_t index, NativeModelLoadedReader::Value& output) noexcept override
        {
            ++queries;
            model = index;
            if (queryResult == ClockResult::Ok)
            {
                output.modelIndex = index;
                output.streamedIn = loaded;
            }
            return queryResult;
        }
        ClockResult RequestStreamingUpgrade(std::uint32_t index, std::uint32_t requestFlags) noexcept override
        {
            ++upgradeRequests;
            model = index;
            flags = requestFlags;
            return requestResult;
        }
        ClockResult ReadStreamingUpgradeLoaded(std::uint32_t index, bool& output) noexcept override
        {
            ++upgradeQueries;
            model = index;
            if (queryResult == ClockResult::Ok)
                output = loaded;
            return queryResult;
        }
    };
    // Only the three shipping overrides are exercised. Other SDK operations are
    // deliberately unavailable in this test-only concrete adapter.
    struct Streaming final : CStreamingDE
    {
        explicit Streaming(NativeStreamingModelAccess& owner) : CStreamingDE(owner) {}
        [[noreturn]] static void Unavailable() { throw std::logic_error("unrelated SDK operation in streaming fixture"); }
        void                     RequestSpecialModel(DWORD, const char*, DWORD) override { Unavailable(); }
        CStreamingInfo*          GetStreamingInfo(uint32) override { Unavailable(); }
        void                     ReinitStreaming() override { Unavailable(); }
        unsigned char            AddArchive(const wchar_t*) override { Unavailable(); }
        void                     RemoveArchive(unsigned char) override { Unavailable(); }
        void                     SetStreamingInfo(unsigned int, unsigned char, unsigned int, unsigned short, unsigned int) override { Unavailable(); }
        bool                     SetStreamingBufferSize(uint32) override { Unavailable(); }
        uint32                   GetStreamingBufferSize() override { Unavailable(); }
        void                     MakeSpaceFor(std::uint32_t) override { Unavailable(); }
        std::uint32_t            GetMemoryUsed() const override { Unavailable(); }
        void                     RemoveBigBuildings() override { Unavailable(); }
        void                     LoadScene(const CVector*) override { Unavailable(); }
        void                     LoadSceneCollision(const CVector*) override { Unavailable(); }
        std::uint32_t            GetNumPedsLoaded() const noexcept override { std::abort(); }
        bool                     IsModelInLoadedPedGroup(std::uint16_t) const noexcept override { std::abort(); }
        std::uint32_t            GetNumLoadedVehicles() const noexcept override { std::abort(); }
        bool                     IsModelInLoadedVehicleGroup(std::uint16_t) const noexcept override { std::abort(); }
    };
    template <class Action>
    void Refuses(Action action, const char* operation)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find(operation) != std::string::npos;
        }
        Check(refused, "owner refusal must remain an explicit SDK failure with operation context");
    }
}
int main()
{
    try
    {
        Access      owner;
        Streaming   streaming(owner);
        CStreaming* sdk = &streaming;
        for (const auto model : std::array<DWORD, 7>{0, 999, 1194, 19999, 20000, 65536, (std::numeric_limits<DWORD>::max)()})
        {
            for (const auto flags : std::array<DWORD, 4>{0, 6, 0x16, (std::numeric_limits<DWORD>::max)()})
            {
                const auto before = owner.requests;
                sdk->RequestModel(model, flags);
                Check(owner.requests == before + 1 && owner.model == model && owner.flags == flags,
                      "actual SDK request forwards the complete DWORD model and flags once, without narrowing or hidden retries");
            }
            for (const bool loaded : {false, true})
            {
                owner.loaded = loaded;
                const auto before = owner.queries;
                Check(sdk->HasModelLoaded(model) == loaded && owner.queries == before + 1 && owner.model == model,
                      "actual SDK query preserves true and false loaded results and full model key");
            }
        }
        for (DWORD upgrade = 1000; upgrade <= 1193; ++upgrade)
        {
            const auto requests = owner.requests, queries = owner.queries;
            for (const auto flags : std::array<DWORD, 4>{0, 6, 0x16, (std::numeric_limits<DWORD>::max)()})
            {
                const auto before = owner.upgradeRequests;
                sdk->RequestModel(upgrade, flags);
                Check(owner.upgradeRequests == before + 1 && owner.model == upgrade && owner.flags == flags,
                      "every upgrade and complete DWORD flag set must reach the paired native operation exactly once");
            }
            for (const bool loaded : {false, true})
            {
                owner.loaded = loaded;
                const auto before = owner.upgradeQueries;
                Check(sdk->HasModelLoaded(upgrade) == loaded && owner.upgradeQueries == before + 1 && owner.model == upgrade,
                      "actual SDK returns the paired upgrade query's true or false result");
            }
            Check(owner.requests == requests && owner.queries == queries,
                  "MTA's entire upgrade interval cannot fall through to an ordinary native request or raw row query");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.requestResult = owner.queryResult = static_cast<ClockResult>(status);
            const auto requests = owner.requests, queries = owner.queries, upgradeRequests = owner.upgradeRequests, upgradeQueries = owner.upgradeQueries;
            Refuses([&] { sdk->RequestModel(7, 0x16); }, "RequestModel");
            Refuses([&] { (void)sdk->HasModelLoaded(7); }, "HasModelLoaded");
            Refuses([&] { sdk->RequestModel(1000, 0x16); }, "RequestModel vehicle upgrade");
            Refuses([&] { (void)sdk->HasModelLoaded(1193); }, "HasModelLoaded vehicle upgrade");
            Check(owner.requests == requests + 1 && owner.queries == queries + 1, "a refused or entered-uncertain request is never retried by the SDK wrapper");
            Check(owner.upgradeRequests == upgradeRequests + 1 && owner.upgradeQueries == upgradeQueries + 1,
                  "a refused or entered-uncertain upgrade never retries or falls through to ordinary streaming");
        }
        owner.requestResult = ClockResult::Ok;
        const auto priorLoads = owner.loads;
        sdk->LoadAllRequestedModels(false, "actual SDK diagnostic tag");
        Check(owner.loads == priorLoads + 1 && !owner.lastOnlyPriority,
              "blocking-all SDK intent reaches the owner once without inventing native tag arguments");
        sdk->LoadAllRequestedModels(true, "priority intent");
        Check(owner.loads == priorLoads + 2 && owner.lastOnlyPriority,
              "actual SDK true intent reaches the distinct qualified owner route once without false fallback");
        for (const auto key : std::array<std::uint32_t, 5>{0, 19999, 20000, 65536, 0xffffffff})
        {
            const auto before = owner.removes;
            sdk->RemoveModel(key);
            Check(owner.removes == before + 1 && owner.model == key, "actual SDK RemoveModel forwards complete DWORD once without hidden reference decrement");
        }
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            owner.requestResult = static_cast<ClockResult>(status);
            const auto calls = owner.loads;
            Refuses([&] { sdk->LoadAllRequestedModels(false, nullptr); }, "LoadAllRequestedModels");
            Check(owner.loads == calls + 1 && !owner.lastOnlyPriority, "blocking-all entered failure remains explicit without SDK retry");
            Refuses([&] { sdk->LoadAllRequestedModels(true, nullptr); }, "LoadAllRequestedModels priority-only");
            Check(owner.loads == calls + 2 && owner.lastOnlyPriority, "priority-only owner refusal never retries through blocking-all");
            const auto before = owner.removes;
            Refuses([&] { sdk->RemoveModel(17); }, "RemoveModel");
            Check(owner.removes == before + 1, "entered-uncertain direct native removal never retries through the SDK wrapper");
        }
        std::puts("Production MTA streaming load-all SDK fixture completed");
        NativeStreamingModelAccess absent;
        Streaming                  unbound(absent);
        Refuses([&] { unbound.RequestModel(0, 6); }, "RequestModel");
        Refuses([&] { (void)unbound.HasModelLoaded(0); }, "HasModelLoaded");
        Refuses([&] { unbound.RequestModel(1000, 6); }, "RequestModel vehicle upgrade");
        Refuses([&] { (void)unbound.HasModelLoaded(1193); }, "HasModelLoaded vehicle upgrade");
        Refuses([&] { unbound.RemoveModel(17); }, "RemoveModel");
        Refuses([&] { unbound.LoadAllRequestedModels(false, nullptr); }, "LoadAllRequestedModels");
        Refuses([&] { unbound.LoadAllRequestedModels(true, nullptr); }, "LoadAllRequestedModels priority-only");
        std::puts("Production MTA streaming priority SDK fixture completed");
        std::puts("Production MTA streaming remove SDK fixture completed");
        std::puts("Production MTA streaming SDK slice fixture completed");
        std::printf("Passed %u actual MTA streaming SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Streaming SDK failed after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
