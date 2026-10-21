/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise borrowed native startup-pump observations over owned bytes
 *****************************************************************************/
#include "../../Client/game_de/NativeStartupCompletionAccess.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using Access = mta::game_de::NativeStartupCompletionAccess;
    using Result = mta::game_de::ClockResult;
    namespace P = mta::game_de::NativeStartupCompletionProfile;
    constexpr std::uintptr_t image = 0x10000000, receiver = 0x20000000;
    unsigned                 checks = 0;
    void                     Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL after %u startup-completion checks: %s\n", checks, reason);
            std::exit(EXIT_FAILURE);
        }
    }
    struct Region
    {
        std::uintptr_t             address;
        std::vector<unsigned char> bytes;
    };
    struct Fixture;
    Fixture* current = nullptr;
    struct Fixture
    {
        std::vector<Region> regions;
        unsigned            reads = 0, failAt = 0, ownerCalls = 0, revokeAt = 0, watched = 0, changeRead = 2;
        std::uintptr_t      changeAt = 0;
        std::uint64_t       changeValue = 0;
        std::size_t         changeBytes = 0;
        Result              ownerResult = Result::Ok;
        void                Add(std::uintptr_t at, std::size_t bytes) { regions.push_back({at, std::vector<unsigned char>(bytes)}); }
        void                PutBytes(std::uintptr_t at, const void* value, std::size_t bytes)
        {
            for (auto& region : regions)
                if (at >= region.address && at - region.address <= region.bytes.size() && bytes <= region.bytes.size() - (at - region.address))
                {
                    std::memcpy(region.bytes.data() + at - region.address, value, bytes);
                    return;
                }
            std::abort();
        }
        template <typename T>
        void Put(std::uintptr_t at, T value)
        {
            PutBytes(at, &value, sizeof(value));
        }
        Fixture()
        {
            current = this;
            Add(image + P::RootRva, 8);
            Add(image + P::StateRva, 4);
            Add(image + P::RequestRva, 1);
            Add(receiver, 8);
            Add(receiver + P::PumpOffset, 1);
            Put(image + P::RootRva, receiver);
            Put(receiver, image + P::VtableRva);
            Put(image + P::StateRva, std::uint32_t{9});
            Put(image + P::RequestRva, static_cast<unsigned char>(0));
            Put(receiver + P::PumpOffset, static_cast<unsigned char>(0));
        }
        static bool Read(std::uintptr_t at, void* output, std::size_t bytes) noexcept
        {
            auto& fixture = *current;
            if (++fixture.reads == fixture.failAt)
                return false;
            if (at == fixture.changeAt && ++fixture.watched == fixture.changeRead)
                fixture.PutBytes(at, &fixture.changeValue, fixture.changeBytes);
            for (const auto& region : fixture.regions)
                if (at >= region.address && at - region.address <= region.bytes.size() && bytes <= region.bytes.size() - (at - region.address))
                {
                    std::memcpy(output, region.bytes.data() + at - region.address, bytes);
                    return true;
                }
            return false;
        }
        static Result Owner(void* actual) noexcept
        {
            auto& fixture = *current;
            ++fixture.ownerCalls;
            if (actual != reinterpret_cast<void*>(receiver))
                return Result::OwnerConflict;
            return fixture.revokeAt == fixture.ownerCalls ? Result::WrongThread : fixture.ownerResult;
        }
    };
    Access::Value Sentinel()
    {
        return {123, 45, 67};
    }
    bool Untouched(const Access::Value& value)
    {
        return value.nativeState == 123 && value.request == 45 && value.pumpActive == 67;
    }
    Result Read(Access::Value& value)
    {
        return Access(image, Fixture::Read, Fixture::Owner).Read(reinterpret_cast<void*>(receiver), value);
    }
    void Refuses(const char* reason)
    {
        auto value = Sentinel();
        Check(Read(value) != Result::Ok, reason);
        Check(Untouched(value), "refusal preserves prior caller value");
    }
}

int main()
{
    static_assert(P::pins.size() == 9 && P::PumpOffset == 0x890);
    unsigned successfulReads = 0;
    for (std::uint32_t state = 6; state <= 9; ++state)
        for (unsigned char request = 0; request <= 1; ++request)
            for (unsigned char active = 0; active <= 1; ++active)
            {
                Fixture fixture;
                fixture.Put(image + P::StateRva, state);
                fixture.Put(image + P::RequestRva, request);
                fixture.Put(receiver + P::PumpOffset, active);
                auto value = Sentinel();
                Check(Read(value) == Result::Ok, "qualified native state/request/pump tuple observed");
                Check(value.nativeState == state && value.request == request && value.pumpActive == active, "native facts preserved exactly");
                Check(value.PumpRetired() == (state == 9 && request == 0 && active == 0), "only exact native retirement condition exposed");
                Check(fixture.ownerCalls == 3, "owner scope checked before each sample and final publication");
                successfulReads = fixture.reads;
            }
    {
        Fixture       fixture;
        Access::Value value{};
        Check(Read(value) == Result::Ok && value.PumpRetired(), "existing completed session can already expose zero before any request entry");
        Check(fixture.ownerCalls == 3, "reader observes facts without claiming or executing a startup request");
    }
    for (unsigned fail = 1; fail <= successfulReads; ++fail)
    {
        Fixture fixture;
        fixture.failAt = fail;
        Refuses("every actual guarded-read failure refuses");
    }
    for (unsigned revoke = 1; revoke <= 3; ++revoke)
    {
        Fixture fixture;
        fixture.revokeAt = revoke;
        auto value = Sentinel();
        Check(Read(value) == Result::WrongThread && Untouched(value), "scope revocation preserves exact refusal and prior output");
    }
    for (const auto result : {Result::Busy, Result::BindingChanged, Result::OwnerConflict, Result::NotInitialized})
    {
        Fixture fixture;
        fixture.ownerResult = result;
        auto value = Sentinel();
        Check(Read(value) == result && Untouched(value) && fixture.reads == 0, "refused owner prevents all raw object reads");
    }
    const std::uintptr_t watched[] = {image + P::RootRva, receiver, image + P::StateRva, image + P::RequestRva, receiver + P::PumpOffset};
    const std::size_t    widths[] = {8, 8, 4, 1, 1};
    const std::uint64_t  changed[] = {receiver + 8, image + P::VtableRva + 8, 8, 1, 1};
    for (std::size_t i = 0; i < sizeof(watched) / sizeof(watched[0]); ++i)
        for (unsigned read : {2u, 3u, 4u})
        {
            Fixture fixture;
            fixture.changeAt = watched[i];
            fixture.changeRead = read;
            fixture.changeBytes = widths[i];
            fixture.changeValue = changed[i];
            Refuses("changed current root/vtable/state/request/pump rejects complete sample");
        }
    for (std::uint32_t state = 0; state <= 11; ++state)
        if (state < 6 || state > 9)
        {
            Fixture fixture;
            fixture.Put(image + P::StateRva, state);
            Refuses("unqualified native state is not invented frontend or success");
        }
    for (unsigned char malformed : {static_cast<unsigned char>(2), static_cast<unsigned char>(255)})
    {
        Fixture fixture;
        fixture.Put(image + P::RequestRva, malformed);
        Refuses("malformed native request byte refuses");
    }
    for (unsigned char malformed : {static_cast<unsigned char>(2), static_cast<unsigned char>(255)})
    {
        Fixture fixture;
        fixture.Put(receiver + P::PumpOffset, malformed);
        Refuses("malformed native pump byte refuses");
    }
    {
        Fixture fixture;
        fixture.Put(receiver, image + P::VtableRva + 8);
        Refuses("foreign receiver vtable refuses");
    }
    {
        Fixture fixture;
        fixture.Put(image + P::RootRva, receiver + 8);
        Refuses("stale receiver is not the current published root");
    }
    {
        Fixture fixture;
        auto    value = Sentinel();
        Check(Access(image, nullptr, Fixture::Owner).Read(reinterpret_cast<void*>(receiver), value) == Result::InvalidArgument && Untouched(value),
              "missing guarded reader preserves output");
        Check(Access(image, Fixture::Read, nullptr).Read(reinterpret_cast<void*>(receiver), value) == Result::InvalidArgument && Untouched(value),
              "missing actual owner preserves output");
        Check(Access(image, Fixture::Read, Fixture::Owner).Read(nullptr, value) == Result::InvalidArgument && Untouched(value), "null receiver refuses");
        Check(Access((std::numeric_limits<std::uintptr_t>::max)() - 1, Fixture::Read, Fixture::Owner).Read(reinterpret_cast<void*>(receiver), value) !=
                      Result::Ok &&
                  Untouched(value),
              "corrupt image arithmetic refuses");
    }
    std::printf("Passed %u native startup-completion checks\n", checks);
    std::puts("Actual pump facts, exact current receiver, stable tuple and borrowed scope revocation checked");
    return EXIT_SUCCESS;
}
