/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Admit the actual network ABI before constructing its interface
 *****************************************************************************/
#pragma once

namespace NetworkModuleStartup
{
    using CheckCompatibility = unsigned long (*)(unsigned long, unsigned long*);
    enum class Failure
    {
        None,
        MissingCompatibility,
        VersionMismatch
    };
    struct Result
    {
        Failure       failure = Failure::None;
        unsigned long moduleVersion = 0;
    };

    template <class Initialize>
    Result Run(CheckCompatibility check, unsigned long expected, Initialize&& initialize)
    {
        // The initializer may construct a whole network owner. Reject an absent
        // or incompatible ABI before entering it, rather than dereferencing a
        // missing export or inspecting an already incompatible CNet vtable.
        if (!check)
            return {Failure::MissingCompatibility, 0};
        if (!check(expected, nullptr))
        {
            unsigned long actual = 0;
            check(1, &actual);
            return {Failure::VersionMismatch, actual};
        }
        initialize();
        return {};
    }
}
