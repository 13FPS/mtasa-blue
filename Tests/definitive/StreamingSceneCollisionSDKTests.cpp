/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Focused actual SDK transport for collision-only streaming intent
 ****************************************************************************/
// A successful Access return here proves CPU dispatch only, never game work.
#include "../../Client/game_de/CStreamingDE.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

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
    struct CollisionAccess final : NativeStreamingModelAccess
    {
        unsigned       collisions = 0, sceneFallbacks = 0, fullFallbacks = 0, modelFallbacks = 0;
        const CVector* received = nullptr;
        ClockResult    result = ClockResult::Ok;
        // This distinct optional access channel leaves the actual owner to
        // qualify native effects; the spy deliberately never dereferences XYZ.
        ClockResult LoadStreamingSceneCollision(const CVector* position) noexcept override
        {
            ++collisions;
            received = position;
            return result;
        }
        ClockResult LoadStreamingScene(const CVector*) noexcept override
        {
            ++sceneFallbacks;
            return ClockResult::NotInitialized;
        }
        ClockResult LoadStreamingModels(bool) noexcept override
        {
            ++fullFallbacks;
            return ClockResult::NotInitialized;
        }
        ClockResult RequestStreamingModel(std::uint32_t, std::uint32_t) noexcept override
        {
            ++modelFallbacks;
            return ClockResult::NotInitialized;
        }
    };
    struct CollisionObserver final : CStreamingDE
    {
        explicit CollisionObserver(NativeStreamingModelAccess& access) : CStreamingDE(access) {}
        [[noreturn]] static void Unavailable() { throw std::logic_error("unrelated operation in collision-only SDK contract"); }
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
        // Inherit all ten existing production overrides and the real
        // collision override. A test-owned collision override would hide the
        // production dispatch this contract needs to exercise.
    };
    static_assert(std::is_abstract_v<CStreamingDE> && !std::is_abstract_v<CollisionObserver>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::LoadSceneCollision), decltype(&CStreamingDE::LoadSceneCollision)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::RequestModel), decltype(&CStreamingDE::RequestModel)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::RequestSpecialModel), decltype(&CStreamingDE::RequestSpecialModel)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::RemoveModel), decltype(&CStreamingDE::RemoveModel)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::HasModelLoaded), decltype(&CStreamingDE::HasModelLoaded)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::LoadAllRequestedModels), decltype(&CStreamingDE::LoadAllRequestedModels)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::LoadScene), decltype(&CStreamingDE::LoadScene)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::GetNumPedsLoaded), decltype(&CStreamingDE::GetNumPedsLoaded)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::IsModelInLoadedPedGroup), decltype(&CStreamingDE::IsModelInLoadedPedGroup)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::GetNumLoadedVehicles), decltype(&CStreamingDE::GetNumLoadedVehicles)>);
    static_assert(std::is_same_v<decltype(&CollisionObserver::IsModelInLoadedVehicleGroup), decltype(&CStreamingDE::IsModelInLoadedVehicleGroup)>);

    template <class Action>
    void Refuses(Action action)
    {
        bool refused = false;
        try
        {
            action();
        }
        catch (const std::runtime_error& error)
        {
            refused = std::string(error.what()).find("LoadSceneCollision") != std::string::npos;
        }
        Check(refused, "collision owner refusal stays visible through the actual SDK operation context");
    }
}
int main()
{
    try
    {
        CollisionAccess   access;
        CollisionObserver observer(access);
        CStreaming&       sdk = observer;
        // Include exact negative-zero, finite and nonfinite bits. The SDK
        // transports one pointer unchanged; native admission owns copying,
        // finite checks and the genuine operation's coordinate-domain rules.
        for (const auto bits : std::array<std::array<std::uint32_t, 3>, 3>{
                 {{{0x80000000u, 0x3f800001u, 0xc3964000u}}, {{0u, 0x7f7fffffu, 0xff7fffffu}}, {{0x7fc12345u, 0x7f800000u, 0xff800000u}}}})
        {
            CVector position;
            static_assert(sizeof(position) == sizeof(bits));
            std::memcpy(&position, bits.data(), sizeof(position));
            const auto before = access.collisions;
            sdk.LoadSceneCollision(&position);
            Check(access.collisions == before + 1 && access.received == &position && !std::memcmp(&position, bits.data(), sizeof(position)),
                  "actual collision SDK forwards one original pointer and preserves all caller XYZ bits");
        }
        const auto nullCalls = access.collisions;
        sdk.LoadSceneCollision(nullptr);
        Check(access.collisions == nullCalls + 1 && access.received == nullptr, "null SDK collision intent still reaches the separately qualified owner once");
        CVector       position(1.0f, 2.0f, 3.0f);
        const CVector originalPosition = position;
        for (unsigned status = 1; status <= static_cast<unsigned>(ClockResult::OriginalException); ++status)
        {
            access.result = static_cast<ClockResult>(status);
            const auto before = access.collisions;
            Refuses([&] { sdk.LoadSceneCollision(&position); });
            Check(access.collisions == before + 1 && access.received == &position,
                  "refused or entered-unknown collision loading never retries through the SDK");
            Check(!std::memcmp(&position, &originalPosition, sizeof(position)), "refused SDK collision loading never edits or normalizes caller XYZ bits");
        }
        NativeStreamingModelAccess legacy;
        Check(legacy.LoadStreamingSceneCollision(&position) == ClockResult::NotInitialized,
              "an older owner defaults to explicit refusal for the new optional collision channel");
        CollisionObserver missing(legacy);
        Refuses([&] { missing.LoadSceneCollision(&position); });
        Check(access.sceneFallbacks == 0 && access.fullFallbacks == 0 && access.modelFallbacks == 0,
              "collision-only intent never falls back to LoadScene blocking-all or ordinary model request");
        std::puts("Production MTA streaming scene-collision SDK contract fixture completed");
        std::printf("Passed %u actual MTA streaming collision SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Scene-collision SDK contract failed: %s\n", error.what());
        return 1;
    }
}
