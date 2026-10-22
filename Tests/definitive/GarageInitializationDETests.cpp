/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Actual SDK garage schedule, mandatory receivers and accepted prefix
 *****************************************************************************/
#include "../../Client/game_de/CGaragesDE.h"
#include <game/CGarage.h>
#include <game/CGarageInitializationPolicy.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    unsigned checks = 0;
    void     Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
        {
            std::fprintf(stderr, "FAIL: %s\n", why);
            std::exit(EXIT_FAILURE);
        }
    }
    [[noreturn]] void Unqualified()
    {
        std::fputs("FAIL: unrelated native garage effect reached SDK receiver\n", stderr);
        std::abort();
    }
    constexpr std::array<DWORD, 16> Expected = {7, 8, 10, 11, 12, 15, 18, 19, 24, 27, 32, 33, 36, 40, 41, 47};
    struct Event
    {
        DWORD id;
        BYTE  type;
    };
    struct Host;
    struct Garage final : CGarage
    {
        Host* host = nullptr;
        DWORD id = 0;
        BYTE  type = 0;
        void  SetType(BYTE value) override;
        BYTE  GetType() const override { return type; }
        void  GetPosition(CVector&) const override { Unqualified(); }
        void  GetDirection(float&, float&, float&, float&) const override { Unqualified(); }
        void  GetSize(float&, float&, float&) const override { Unqualified(); }
        void  GetBoundingBox(float&, float&, float&, float&) const override { Unqualified(); }
        void  SetOpen(bool) override { Unqualified(); }
        bool  IsOpen() const override { Unqualified(); }
    };
    struct Host final : mta::game_de::CGaragesDE
    {
        std::array<Garage, MAX_GARAGES> garages;
        std::vector<DWORD>              lookups;
        std::vector<Event>              attempts;
        DWORD                           missing = MAX_GARAGES, refuseLookup = MAX_GARAGES, refuseType = MAX_GARAGES;
        bool                            acceptBeforeFailure = false;
        Host()
        {
            for (DWORD index = 0; index < MAX_GARAGES; ++index)
            {
                garages[index].host = this;
                garages[index].id = index;
                garages[index].type = static_cast<BYTE>(100 + index);
            }
        }
        CGarage* GetGarage(DWORD index) override
        {
            lookups.push_back(index);
            if (index == refuseLookup)
                throw std::runtime_error("fixture garage lookup refused");
            return index < MAX_GARAGES && index != missing ? &garages[index] : nullptr;
        }
    };
    void Garage::SetType(BYTE value)
    {
        host->attempts.push_back({id, value});
        if (id == host->refuseType && !host->acceptBeforeFailure)
            throw std::runtime_error("fixture garage type refused");
        type = value;
        if (id == host->refuseType)
            throw std::runtime_error("fixture garage type refused");
    }
    void CheckPrefix(const Host& host, unsigned accepted)
    {
        for (DWORD index = 0; index < MAX_GARAGES; ++index)
        {
            bool initialized = false;
            for (unsigned entry = 0; entry < accepted; ++entry)
                initialized |= Expected[entry] == index;
            Check(host.garages[index].type == (initialized ? 1 : static_cast<BYTE>(100 + index)),
                  "only the accepted MTA prefix changes; unselected and later garage types remain untouched");
        }
        for (unsigned index = 0; index < host.attempts.size(); ++index)
            Check(host.attempts[index].id == Expected[index % Expected.size()] && host.attempts[index].type == 1,
                  "every attempted write preserves exact existing SDK identity, order and BYTE1 intent");
    }
}

int main()
{
    static_assert(std::is_abstract_v<mta::game_de::CGaragesDE> && !std::is_abstract_v<Host>);
    static_assert(MtaGame::CGarageInitializationPolicy::Indices.size() == Expected.size());
    Host      success;
    CGarages& sdk = success;
    sdk.Initialize();
    Check(success.lookups.size() == 16 && success.attempts.size() == 16, "actual SDK initialization resolves and sets all16 required garages once");
    for (unsigned index = 0; index < Expected.size(); ++index)
        Check(success.lookups[index] == Expected[index], "actual SDK GetGarage order matches classic initialization identities");
    CheckPrefix(success, 16);
    sdk.Initialize();
    Check(success.attempts.size() == 32, "an explicit second Initialize preserves classic call behavior without a hidden success latch");
    CheckPrefix(success, 16);

    for (unsigned failure = 0; failure < Expected.size(); ++failure)
    {
        for (const bool accepted : {false, true})
        {
            Host host;
            host.refuseType = Expected[failure];
            host.acceptBeforeFailure = accepted;
            bool refused = false;
            try
            {
                host.Initialize();
            }
            catch (const std::runtime_error& error)
            {
                refused = std::string(error.what()) == "fixture garage type refused";
            }
            Check(refused && host.lookups.size() == failure + 1 && host.attempts.size() == failure + 1,
                  "receiver failure propagates unchanged at its exact position without a retry, rollback or later write");
            CheckPrefix(host, failure + (accepted ? 1 : 0));
        }
        Host missing;
        missing.missing = Expected[failure];
        bool unavailable = false;
        try
        {
            missing.Initialize();
        }
        catch (const std::runtime_error& error)
        {
            unavailable = std::string(error.what()).find("DE CGarages Initialize requires garage ") == 0;
        }
        Check(unavailable && missing.lookups.size() == failure + 1 && missing.attempts.size() == failure,
              "a mandatory absent garage refuses instead of silently skipping or acknowledging complete initialization");
        CheckPrefix(missing, failure);

        Host lookup;
        lookup.refuseLookup = Expected[failure];
        bool lookupRefused = false;
        try
        {
            lookup.Initialize();
        }
        catch (const std::runtime_error& error)
        {
            lookupRefused = std::string(error.what()) == "fixture garage lookup refused";
        }
        Check(lookupRefused && lookup.lookups.size() == failure + 1 && lookup.attempts.size() == failure,
              "lookup admission failure preserves its original exception and earlier accepted prefix");
        CheckPrefix(lookup, failure);
    }
    std::puts("Production actual shared garage initialization SDK fixture completed");
    std::printf("Passed %u actual garage initialization SDK checks (%zu-bit)\n", checks, sizeof(void*) * 8);
    return 0;
}
