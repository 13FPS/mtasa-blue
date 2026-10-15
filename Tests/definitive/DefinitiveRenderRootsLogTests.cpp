/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned checks for bounded scalar root diagnostics, without a renderer
 *****************************************************************************/
#include "../../Client/core/DefinitiveRenderRootsLog.h"
#include <cstdlib>
#include <cstdio>
#include <limits>
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
            std::fprintf(stderr, "Roots log fixture failed: %s\n", why);
            std::exit(1);
        }
    }
}

int main()
{
    using namespace mta::game_de;
    using namespace DefinitiveRenderRootsLog;
    static_assert(sizeof(RenderRootsDiagnosticV1) == 56);
    RenderRootsDiagnosticV1 state;
    state.version = 1;
    state.generationLow = 4;
    state.generationHigh = 1;
    state.acceptedSnapshotsLow = 4294967294u;
    state.acceptedSnapshotsHigh = 3;
    state.dispatchEntriesLow = 5;
    state.dispatchEntriesHigh = 4;
    state.queryPhase = 1;
    state.checkpoint = 26;
    state.refusalReason = 3;
    state.scalarValidMask = 3;
    state.contextNodeMask = 1;
    state.nativeBufferIndex = 2;
    const Fields expected{1, 4, 1, 4294967294u, 3, 5, 4, 1, 26, 3, 0, 3, 1, 2};
    Check(Copy(state) == expected, "all fourteen independently ordered SDK scalars survive the logger");
    Check(Format(expected, 17, 0x200000003ull, 1) ==
              "{\"event\":\"de_render_roots\",\"pid\":17,\"run_ms\":[3,2],\"phase\":1,\"fields\":[1,4,1,4294967294,3,5,4,1,26,3,0,3,1,2]}\n",
          "JSON preserves run split, unsigned values, order and separate outer phase");
    auto maximum = expected;
    maximum[10] = (std::numeric_limits<std::uint32_t>::max)();
    Check(Format(maximum, 1, 0, 2).find(",4294967295,3,1,2]") != std::string::npos, "the raw exception scalar remains unsigned rather than a signed HRESULT");

    std::vector<std::string> lines;
    const auto               sink = [&](const std::string& line)
    {
        lines.push_back(line);
        return true;
    };
    Recorder recorder;
    Check(recorder.Write(state, 17, 3, 0, sink), "available start record is optional and can be logged");
    ++state.acceptedSnapshotsLow;
    ++state.dispatchEntriesLow;
    Check(recorder.Write(state, 17, 3, 1, sink) && lines.size() == 2,
          "an available Begin report cannot suppress the first poll needed for a complete saved run");
    Check(!recorder.Write(state, 17, 3, 1, sink) && lines.size() == 2, "later counter-only churn does not consume records");
    state.acceptedSnapshotsLow = 0;
    ++state.acceptedSnapshotsHigh;
    state.dispatchEntriesLow = 0;
    ++state.dispatchEntriesHigh;
    Check(!recorder.Write(state, 17, 3, 1, sink), "split-counter carry alone is also deduplicated");
    state.checkpoint = 23;
    state.refusalReason = 4;
    Check(recorder.Write(state, 17, 3, 1, sink) && lines.size() == 3, "a different actual refusal checkpoint is preserved");
    Check(recorder.Write(state, 17, 3, 2, sink) && lines.size() == 4, "End writes its own record even when values are unchanged");
    Check(!recorder.Write(state, 17, 3, 3, sink) && lines.size() == 4, "invalid outer phase consumes no slot");
    state.version = 99;
    Check(!recorder.Write(state, 17, 3, 2, sink) && lines.size() == 4, "different diagnostic version is not relabeled as V1");
    state.version = 1;
    Recorder retry;
    Check(!retry.Write(state, 1, 0, 1, [](const std::string&) { return false; }), "failed file append does not commit a sample");
    Check(retry.Write(state, 1, 0, 1, sink), "identical sample remains retryable after sink failure");
    Recorder throwing;
    bool     raised = false;
    try
    {
        throwing.Write(state, 1, 0, 1, [](const std::string&) -> bool { throw std::runtime_error("owned sink failure"); });
    }
    catch (const std::runtime_error&)
    {
        raised = true;
    }
    Check(raised && throwing.Write(state, 1, 0, 1, sink), "optional I/O exception leaves recorder values and capacity unchanged");
    Recorder   bounded;
    unsigned   emitted = 0;
    const auto count = [&](const std::string&)
    {
        ++emitted;
        return true;
    };
    for (unsigned index = 0; index < Recorder::MaximumRecords - 1; ++index)
    {
        state.checkpoint = index % 2 + 1;
        Check(bounded.Write(state, 1, 0, 1, count), "bounded changing diagnostics retain the reserved terminal slot");
    }
    state.checkpoint = 28;
    Check(!bounded.Write(state, 1, 0, 1, count) && emitted == 31, "polling cannot consume End's final slot");
    Check(bounded.Write(state, 1, 0, 2, count) && emitted == 32, "End remains writable at the record limit");
    Check(!bounded.Write(state, 1, 0, 2, count) && emitted == 32, "no further record exceeds the fixed budget");
    Recorder noStart;
    Check(noStart.Write(state, 1, 0, 1, count) && noStart.Write(state, 1, 0, 2, count),
          "cache availability only after a callback does not require a fabricated Begin record");
    std::printf("{\"success\":true,\"checks\":%u}\n", checks);
}
