/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Null-preserving classic flags and actual DE raw-flags SDK dispatch
 ****************************************************************************/
#include "../../Client/game_de/CStreamingDE.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

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

    // Only GetStreamingInfo is supplied for this legacy transport fixture.
    // Calls through the new inherited default execute the actual Shared body,
    // not a test copy of its null/flags policy or a DE20-byte row substitute.
    struct LegacyStreaming final : CStreaming
    {
        CStreamingInfo           row{};
        bool                     present = true;
        unsigned                 queries = 0;
        std::uint32_t            received = 0;
        [[noreturn]] static void Unavailable() { throw std::logic_error("unrelated legacy streaming operation"); }
        void                     RequestModel(DWORD, DWORD) override { Unavailable(); }
        void                     RemoveModel(std::uint32_t) override { Unavailable(); }
        void                     LoadAllRequestedModels(bool, const char*) override { Unavailable(); }
        bool                     HasModelLoaded(DWORD) override { Unavailable(); }
        void                     RequestSpecialModel(DWORD, const char*, DWORD) override { Unavailable(); }
        CStreamingInfo*          GetStreamingInfo(uint32 model) override
        {
            ++queries;
            received = model;
            return present ? &row : nullptr;
        }
        void          ReinitStreaming() override { Unavailable(); }
        unsigned char AddArchive(const wchar_t*) override { Unavailable(); }
        void          RemoveArchive(unsigned char) override { Unavailable(); }
        void          SetStreamingInfo(unsigned int, unsigned char, unsigned int, unsigned short, unsigned int) override { Unavailable(); }
        bool          SetStreamingBufferSize(uint32) override { Unavailable(); }
        uint32        GetStreamingBufferSize() override { Unavailable(); }
        void          MakeSpaceFor(std::uint32_t) override { Unavailable(); }
        std::uint32_t GetMemoryUsed() const override { Unavailable(); }
        void          RemoveBigBuildings() override { Unavailable(); }
        void          LoadScene(const CVector*) override { Unavailable(); }
        void          LoadSceneCollision(const CVector*) override { Unavailable(); }
        std::uint32_t GetNumPedsLoaded() const noexcept override { std::abort(); }
        bool          IsModelInLoadedPedGroup(std::uint16_t) const noexcept override { std::abort(); }
        std::uint32_t GetNumLoadedVehicles() const noexcept override { std::abort(); }
        bool          IsModelInLoadedVehicleGroup(std::uint16_t) const noexcept override { std::abort(); }
    };

    struct FlagsAccess final : NativeStreamingModelAccess
    {
        std::uint32_t received = 0, *mutateInput = nullptr;
        unsigned      reads = 0, upgrades = 0, loads = 0, requests = 0;
        unsigned char flags = 0, state = 0;
        ClockResult   result = ClockResult::Ok;
        ClockResult   ReadStreamingModelLoaded(std::uint32_t model, NativeModelLoadedReader::Value& output) noexcept override
        {
            ++reads;
            if (mutateInput)
            {
                *mutateInput = 0;
                mutateInput = nullptr;
            }
            received = model;
            output.modelIndex = model;
            output.rawFlags = flags;
            output.rawLoadState = state;
            output.streamedIn = state == 1;
            return result;
        }
        ClockResult ReadStreamingUpgradeLoaded(std::uint32_t, bool&) noexcept override
        {
            ++upgrades;
            return ClockResult::NotInitialized;
        }
        ClockResult LoadStreamingModels(bool) noexcept override
        {
            ++loads;
            return ClockResult::NotInitialized;
        }
        ClockResult RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept override
        {
            ++requests;
            return ClockResult::NotInitialized;
        }
    };

    struct FlagsObserver final : CStreamingDE
    {
        explicit FlagsObserver(NativeStreamingModelAccess& owner) : CStreamingDE(owner) {}
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
        // All12 real DE overrides remain inherited. These ten raw operations
        // stay genuinely unsupported; the new getter must never call them.
    };
    static_assert(std::is_same_v<decltype(&LegacyStreaming::GetModelStreamingFlags), decltype(&CStreaming::GetModelStreamingFlags)>);
    static_assert(std::is_same_v<decltype(&FlagsObserver::GetModelStreamingFlags), decltype(&CStreamingDE::GetModelStreamingFlags)>);
    static_assert(std::is_same_v<decltype(std::declval<CStreaming&>().GetModelStreamingFlags(0)), std::optional<std::uint8_t>>);
    static_assert(std::is_abstract_v<CStreamingDE> && !std::is_abstract_v<FlagsObserver>);

    template <class Action>
    void Refuses(Action action, ClockResult expected)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            const std::string text = error.what();
            const auto        suffix = ": " + std::to_string(static_cast<unsigned>(expected));
            refused = text.find("GetModelStreamingFlags") != std::string::npos && text.size() >= suffix.size() &&
                      text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        Check(refused, "owner refusal was not preserved by Require");
    }
}

int main()
{
    try
    {
        LegacyStreaming legacy;
        CStreaming&     classic = legacy;
        legacy.present = false;
        auto before = legacy.queries;
        Check(!classic.GetModelStreamingFlags(0xfedcba98u).has_value(), "classic null row must remain absent");
        Check(legacy.queries == before + 1 && legacy.received == 0xfedcba98u, "classic default did not query once with full key");
        legacy.present = true;
        for (const auto flags : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{6}, std::uint8_t{0x80}, std::uint8_t{0xff}})
        {
            legacy.row.flg = flags;
            before = legacy.queries;
            const auto value = classic.GetModelStreamingFlags(26315);
            Check(value.has_value() && *value == flags, "classic present flags changed or became absent");
            Check(legacy.queries == before + 1, "classic flag read repeated GetStreamingInfo");
        }

        FlagsAccess   owner;
        FlagsObserver receiver(owner);
        CStreaming&   streaming = receiver;
        for (const auto model : {std::uint32_t{0}, std::uint32_t{999}, std::uint32_t{1000}, std::uint32_t{1193}, std::uint32_t{20000}, std::uint32_t{26315},
                                 std::uint32_t{0xffff}, std::uint32_t{0x10000}, std::uint32_t{0x80000001}, std::uint32_t{0xffffffff}})
        {
            owner.flags = 0xa6;
            owner.state = 0;
            auto input = model;
            owner.mutateInput = &input;
            const auto readsBefore = owner.reads;
            const auto value = streaming.GetModelStreamingFlags(input);
            Check(value.has_value() && *value == 0xa6, "DE unloaded row flags were discarded");
            Check(owner.reads == readsBefore + 1 && owner.received == model && input == 0, "DE key was narrowed/repeated/torn");
        }
        // Loading state must not suppress required flags. This includes the
        // native reader's other raw byte values, not just loaded-state1.
        for (const auto state : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}, std::uint8_t{4}, std::uint8_t{0xff}})
        {
            owner.state = state;
            owner.flags = STREAMING_FLAG_GAME_REQUIRED | STREAMING_FLAG_MISSION_REQUIRED;
            const auto value = streaming.GetModelStreamingFlags(1000);
            Check(value.has_value() && *value == 6, "DE flags incorrectly depended on loaded/upgrade state");
        }
        owner.flags = 0;
        Check(streaming.GetModelStreamingFlags(7) == std::optional<std::uint8_t>{std::uint8_t{0}}, "present DE zero must not mean absent");
        for (const auto failure :
             {ClockResult::InvalidArgument, ClockResult::NotInitialized, ClockResult::WrongThread, ClockResult::BindingChanged, ClockResult::ReadFailure,
              ClockResult::WriteFailure, ClockResult::RestoreFailure, ClockResult::PatchFailure, ClockResult::Busy, ClockResult::Expired,
              ClockResult::UnsupportedBuild, ClockResult::OwnerConflict, ClockResult::Stopped, ClockResult::OriginalException})
        {
            owner.result = failure;
            owner.flags = 0;
            const auto readsBefore = owner.reads;
            Refuses([&] { streaming.GetModelStreamingFlags(26315); }, failure);
            Check(owner.reads == readsBefore + 1, "DE refusal retried or substituted another observation");
        }
        Check(owner.upgrades == 0 && owner.loads == 0 && owner.requests == 0, "flags query invoked paired/loading effects");
        NativeStreamingModelAccess defaults;
        FlagsObserver              missing(defaults);
        Refuses([&] { missing.GetModelStreamingFlags(7); }, ClockResult::NotInitialized);
        std::puts("Production MTA streaming model-flags SDK contract fixture completed");
        std::printf("Passed %u actual MTA streaming model-flags SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Streaming model-flags SDK failure after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
