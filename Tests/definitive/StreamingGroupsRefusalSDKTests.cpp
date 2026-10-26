/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve current-owner refusal through the four real streaming getters
 ****************************************************************************/
#include "../../Client/game_de/CStreamingDE.h"
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    using namespace mta::game_de;
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }

    struct GroupsAccess final : NativeStreamingModelAccess
    {
        unsigned                                reads = 0;
        ClockResult                             result = ClockResult::Ok;
        NativeStreamingModelGroupsReader::Value groups;
        std::uint16_t*                          mutateCaller = nullptr;
        ClockResult                             ReadStreamingModelGroups(NativeStreamingModelGroupsReader::Value& output) noexcept override
        {
            ++reads;
            if (mutateCaller)
            {
                *mutateCaller = 0;
                mutateCaller = nullptr;
            }
            // Deliberately publish even on refusal. The SDK must check status
            // before using an apparent empty or populated tracker snapshot.
            output = groups;
            return result;
        }
    };

    struct GroupsObserver final : CStreamingDE
    {
        explicit GroupsObserver(NativeStreamingModelAccess& owner) : CStreamingDE(owner) {}
        [[noreturn]] static void Unavailable() { throw std::logic_error("unrelated raw streaming operation"); }
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
        // The four tested getters and all other production overrides are
        // inherited. These ten rejectors do not claim a complete native service.
    };
    static_assert(std::is_abstract_v<CStreamingDE> && !std::is_abstract_v<GroupsObserver>);
    static_assert(std::is_same_v<decltype(&GroupsObserver::GetNumPedsLoaded), decltype(&CStreamingDE::GetNumPedsLoaded)>);
    static_assert(std::is_same_v<decltype(&GroupsObserver::IsModelInLoadedPedGroup), decltype(&CStreamingDE::IsModelInLoadedPedGroup)>);
    static_assert(std::is_same_v<decltype(&GroupsObserver::GetNumLoadedVehicles), decltype(&CStreamingDE::GetNumLoadedVehicles)>);
    static_assert(std::is_same_v<decltype(&GroupsObserver::IsModelInLoadedVehicleGroup), decltype(&CStreamingDE::IsModelInLoadedVehicleGroup)>);
    static_assert(!noexcept(std::declval<const CStreaming&>().GetNumPedsLoaded()));
    static_assert(!noexcept(std::declval<const CStreaming&>().IsModelInLoadedPedGroup(0)));
    static_assert(!noexcept(std::declval<const CStreaming&>().GetNumLoadedVehicles()));
    static_assert(!noexcept(std::declval<const CStreaming&>().IsModelInLoadedVehicleGroup(0)));
    static_assert(!noexcept(std::declval<const CStreamingDE&>().GetNumPedsLoaded()));
    static_assert(!noexcept(std::declval<const CStreamingDE&>().IsModelInLoadedPedGroup(0)));
    static_assert(!noexcept(std::declval<const CStreamingDE&>().GetNumLoadedVehicles()));
    static_assert(!noexcept(std::declval<const CStreamingDE&>().IsModelInLoadedVehicleGroup(0)));

    template <class Action>
    void Refuses(Action action, ClockResult expected)
    {
        bool caught = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string message(error.what());
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            caught = message.find("ReadStreamingModelGroups") != std::string::npos && message.size() >= suffix.size() &&
                     message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(caught, "group refusal must reach the SDK caller with its actual owner status");
    }
}

int main()
{
    try
    {
        GroupsAccess      owner;
        GroupsObserver    receiver(owner);
        const CStreaming& sdk = receiver;
        owner.groups.peds.fill(-1);
        owner.groups.vehicles.fill(-1);
        Check(sdk.GetNumPedsLoaded() == 0 && sdk.GetNumLoadedVehicles() == 0 && !sdk.IsModelInLoadedPedGroup(17) && !sdk.IsModelInLoadedVehicleGroup(17),
              "genuine successful empty observations remain zero/false");

        owner.groups.pedCount = (std::numeric_limits<std::uint32_t>::max)();
        owner.groups.peds.back() = 0xffff;
        auto model = std::uint16_t{0xffff};
        owner.mutateCaller = &model;
        auto before = owner.reads;
        Check(sdk.IsModelInLoadedPedGroup(model) && model == 0 && owner.reads == before + 1,
              "membership preserves its copied full16 input through one actual owner observation");
        Check(sdk.GetNumPedsLoaded() == 0xffffffffu, "successful raw ped count must not acquire a new clamp");
        owner.groups.vehicles.fill(17);
        owner.groups.vehicles.back() = 123;
        Check(sdk.GetNumLoadedVehicles() == 50 && sdk.IsModelInLoadedVehicleGroup(123),
              "qualified DE vehicle capacity and last-slot membership stay unchanged");
        owner.groups.vehicles[1] = -1;
        Check(sdk.GetNumLoadedVehicles() == 1 && !sdk.IsModelInLoadedVehicleGroup(123),
              "successful native unused sentinel still stops the vehicle tracker scan");

        for (const auto failure : std::array<ClockResult, 14>{
                 ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
                 ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
                 ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.result = failure;
            owner.groups.pedCount = 0;
            owner.groups.peds.fill(-1);
            owner.groups.vehicles.fill(-1);
            std::uint32_t count = 0xfeed1234u;
            bool          membership = true;
            before = owner.reads;
            Refuses([&] { count = sdk.GetNumPedsLoaded(); }, failure);
            Check(count == 0xfeed1234u && owner.reads == before + 1, "failed ped count neither publishes zero nor retries");
            before = owner.reads;
            Refuses([&] { membership = sdk.IsModelInLoadedPedGroup(17); }, failure);
            Check(membership && owner.reads == before + 1, "failed ped membership neither publishes false nor retries");
            before = owner.reads;
            Refuses([&] { count = sdk.GetNumLoadedVehicles(); }, failure);
            Check(count == 0xfeed1234u && owner.reads == before + 1, "failed vehicle count preserves the caller output and makes one request");
            before = owner.reads;
            Refuses([&] { membership = sdk.IsModelInLoadedVehicleGroup(17); }, failure);
            Check(membership && owner.reads == before + 1, "failed vehicle membership preserves the caller output and makes one request");
        }
        NativeStreamingModelAccess absent;
        GroupsObserver             missing(absent);
        Refuses([&] { (void)missing.GetNumPedsLoaded(); }, ClockResult::NotInitialized);
        Refuses([&] { (void)missing.IsModelInLoadedPedGroup(17); }, ClockResult::NotInitialized);
        Refuses([&] { (void)missing.GetNumLoadedVehicles(); }, ClockResult::NotInitialized);
        Refuses([&] { (void)missing.IsModelInLoadedVehicleGroup(17); }, ClockResult::NotInitialized);
        owner.result = ClockResult::Ok;
        Check(sdk.GetNumPedsLoaded() == 0 && !sdk.IsModelInLoadedVehicleGroup(17), "a later successful current observation is independent of earlier refusal");
        std::puts("Production MTA streaming groups explicit-refusal SDK fixture completed");
        std::printf("Passed %u actual MTA streaming groups-refusal SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Streaming groups-refusal SDK contract failed: %s\n", error.what());
        return 1;
    }
}
