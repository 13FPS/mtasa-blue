/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise the actual classic launcher without ambient diagnostics
 *****************************************************************************/

#include "../../Client/launch/StdInc.h"

namespace ClassicLauncherFixture
{
    // The fixture must exercise the real startup dispatch without appending to
    // the user's report.log or opening a support page if a case regresses.
    inline void AddReportLog(uint, const SString&, uint = 0)
    {
    }

    inline void BrowseToSolution(const SString&, int = 0, const SString& = "", const SString& = "")
    {
        ExitProcess(76);
    }
}

#define AddReportLog     ClassicLauncherFixture::AddReportLog
#define BrowseToSolution ClassicLauncherFixture::BrowseToSolution
#include "../../Client/launch/Main.cpp"
#undef BrowseToSolution
#undef AddReportLog
