/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Owned scalar setup log framing, refusal retention and bounds
 *****************************************************************************/
#include "../../Client/core/DefinitiveRenderSetupLog.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    unsigned checks = 0;
    void     Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(message);
    }

    void FieldAndFormatTests()
    {
        using namespace DefinitiveRenderSetupLog;
        mta::game_de::RenderSetupDiagnosticV1 state{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
        const Fields                          expected{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
        Check(Copy(state) == expected, "SDK field order is preserved independently of the observation protocol");
        Check(Format(expected, 0xffffffffu, 0xffffffffffffffffull, 2) ==
                  "{\"event\":\"de_render_setup\",\"pid\":4294967295,\"run_ms\":[4294967295,4294967295],\"phase\":2,\"fields\":[1,2,3,4,5,6,7,8,9,10,11,12,13,"
                  "14,15,16,17,18]}\n",
              "unsigned framing and split64 run identity remain exact at maximum width");
        state.errorCode = 0xffffffffu;
        Check(Copy(state)[7] == 0xffffffffu, "NTSTATUS and Win32 error bits are never sign-extended");
        Check(Format(Copy(state), 17, 0x100000002ull, 0).find("\"run_ms\":[2,1]") != std::string::npos, "run identity halves cannot be reversed or rounded");
    }

    void DedupAndFailureTests()
    {
        using namespace DefinitiveRenderSetupLog;
        using State = mta::game_de::RenderSetupDiagnosticV1;
        State state;
        state.version = 1;
        Recorder                 recorder;
        std::vector<std::string> lines;
        auto                     sink = [&](const std::string& line)
        {
            lines.push_back(line);
            return true;
        };
        Check(recorder.Write(state, 7, 8, 0, sink), "Begin is recorded even before a setup attempt");
        Check(!recorder.Write(state, 7, 8, 1, sink), "identical sample is suppressed");

        // Any changed SDK fact must survive deduplication. In particular, a
        // later cleanup facts must not hide the retained first failure stage.
        const std::array<std::uint32_t State::*, 17> members{
            &State::generationLow,         &State::generationHigh,  &State::attempted,        &State::stage,           &State::failureStage,
            &State::errorDomain,           &State::errorCode,       &State::initialThreads,   &State::heldThreads,     &State::stablePasses,
            &State::transactionCommitted,  &State::hooksCommitted,  &State::cleanupAttempted, &State::cleanupComplete, &State::retainedSuspensions,
            &State::safetyHeldSuspensions, &State::installSucceeded};
        for (auto member : members)
        {
            state.*member = 1;
            Check(recorder.Write(state, 7, 8, 1, sink), "changed setup field is not mistaken for duplicate evidence");
            Check(!recorder.Write(state, 7, 8, 1, sink), "new setup field becomes the next dedup key");
        }
        Check(recorder.Write(state, 7, 8, 2, sink), "End is forced for an unchanged final state");
        Check(lines.size() == 19, "only Begin, distinct samples and End were written");

        Recorder retry;
        unsigned calls = 0;
        auto     failed = [&](const std::string&)
        {
            ++calls;
            return false;
        };
        Check(!retry.Write(state, 7, 8, 1, failed), "failed sink is reported");
        Check(retry.Write(state, 7, 8, 1, sink), "failed sink cannot suppress a later identical retry");
        Check(calls == 1, "failed write calls the sink exactly once");
        state.version = 0;
        Check(!retry.Write(state, 7, 8, 0, failed), "unavailable diagnostic is rejected");
        state.version = 2;
        Check(!retry.Write(state, 7, 8, 0, failed), "unsupported diagnostic version is rejected");
        state.version = 1;
        Check(!retry.Write(state, 7, 8, 3, failed), "unsupported phase is rejected");
        Check(calls == 1, "invalid requests never reach the sink");
    }

    void TerminalReservationTests()
    {
        using namespace DefinitiveRenderSetupLog;
        mta::game_de::RenderSetupDiagnosticV1 state;
        state.version = 1;
        Recorder recorder;
        unsigned accepted = 0, failures = 0;
        auto     sink = [&](const std::string&)
        {
            ++accepted;
            return true;
        };
        auto failed = [&](const std::string&)
        {
            ++failures;
            return false;
        };
        for (unsigned index = 0; index < Recorder::MaximumRecords - 1; ++index)
        {
            state.initialThreads = index;
            Check(recorder.Write(state, 1, 2, index ? 1 : 0, sink), "bounded distinct records fit before the reserved terminal slot");
        }
        ++state.initialThreads;
        Check(!recorder.Write(state, 1, 2, 1, sink), "sample cannot consume the reserved terminal slot");
        Check(!recorder.Write(state, 1, 2, 0, sink), "repeated Begin cannot consume the reserved terminal slot");
        Check(!recorder.Write(state, 1, 2, 2, failed), "terminal sink failure remains retryable");
        Check(recorder.Write(state, 1, 2, 2, sink), "terminal retry uses its reserved slot");
        Check(!recorder.Write(state, 1, 2, 2, sink), "record cap also applies to repeated End");
        Check(accepted == Recorder::MaximumRecords && failures == 1, "bounded recorder never exceeds 32 successful writes");

        // A successful cleanup must retain the original setup refusal scalars;
        // the independent cache owns their precedence, and the logger copies it.
        Recorder cleanup;
        state = {};
        state.version = 1;
        state.attempted = 1;
        state.failureStage = static_cast<std::uint32_t>(mta::game_de::RenderSetupStage::TransactionPeers);
        state.errorDomain = static_cast<std::uint32_t>(mta::game_de::RenderSetupErrorDomain::Detours);
        state.errorCode = 0xffffffffu;
        state.stage = static_cast<std::uint32_t>(mta::game_de::RenderSetupStage::TransactionPeers);
        state.cleanupAttempted = state.cleanupComplete = 1;
        std::string emitted;
        Check(cleanup.Write(state, 11, 12, 2,
                            [&](const std::string& line)
                            {
                                emitted = line;
                                return true;
                            }),
              "clean End logs preserved refusal");
        Check(
            emitted == "{\"event\":\"de_render_setup\",\"pid\":11,\"run_ms\":[12,0],\"phase\":2,\"fields\":[1,0,0,1,24,24,3,4294967295,0,0,0,0,0,1,1,0,0,0]}\n",
            "successful cleanup cannot be formatted as successful setup or erase the first refusal");
    }
}

int main()
{
    try
    {
        FieldAndFormatTests();
        DedupAndFailureTests();
        TerminalReservationTests();
        std::cout << "{\"success\":true,\"checks\":" << checks << "}\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
