/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Protect actual network initialization against incompatible modules
 *****************************************************************************/
#include "../../Client/core/NetworkModuleStartup.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
    unsigned      checks = 0, checksCalled = 0, versionQueries = 0;
    bool          accepted = false;
    unsigned long Compatibility(unsigned long version, unsigned long* actual)
    {
        ++checksCalled;
        if (actual)
        {
            ++versionQueries;
            if (version != 1)
                throw std::runtime_error("version query changed");
            *actual = 0x1234;
        }
        else if (version != 0x5678)
            throw std::runtime_error("requested ABI changed");
        return accepted ? 1 : 0;
    }
    void Check(bool value, const char* why)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(why);
    }
}
int main()
{
    using namespace NetworkModuleStartup;
    try
    {
        bool       initialized = false;
        const auto initialize = [&] { initialized = true; };
        auto       result = Run(nullptr, 0x5678, initialize);
        Check(result.failure == Failure::MissingCompatibility && !initialized && !checksCalled, "missing export never enters a factory or null function");
        result = Run(&Compatibility, 0x5678, initialize);
        Check(result.failure == Failure::VersionMismatch && result.moduleVersion == 0x1234 && !initialized && checksCalled == 2 && versionQueries == 1,
              "incompatible ABI reports its version before any network construction");
        accepted = true;
        result = Run(&Compatibility, 0x5678,
                     [&]
                     {
                         Check(checksCalled == 3 && versionQueries == 1, "compatibility precedes real initialization without a rejected-version query");
                         initialized = true;
                     });
        Check(result.failure == Failure::None && initialized, "compatible provider reaches network initialization");
        bool propagated = false;
        try
        {
            Run(&Compatibility, 0x5678, [] { throw std::runtime_error("initializer failure"); });
        }
        catch (const std::runtime_error& error)
        {
            propagated = std::strcmp(error.what(), "initializer failure") == 0;
        }
        Check(propagated && checksCalled == 4, "initialization failure preserves ownership failure handling");
        std::printf("Passed %u network startup checks\n", checks);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Network startup failed: %s\n", error.what());
        return 1;
    }
}
