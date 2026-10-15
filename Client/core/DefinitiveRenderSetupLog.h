/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded value-only formatting of optional setup diagnostics
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderSetupDiagnostic.h"
#include <array>
#include <cstdint>
#include <string>

namespace DefinitiveRenderSetupLog
{
    using Fields = std::array<std::uint32_t, 18>;
    inline Fields Copy(const mta::game_de::RenderSetupDiagnosticV1& state) noexcept
    {
        return {state.version,
                state.generationLow,
                state.generationHigh,
                state.attempted,
                state.stage,
                state.failureStage,
                state.errorDomain,
                state.errorCode,
                state.initialThreads,
                state.heldThreads,
                state.stablePasses,
                state.transactionCommitted,
                state.hooksCommitted,
                state.cleanupAttempted,
                state.cleanupComplete,
                state.retainedSuspensions,
                state.safetyHeldSuspensions,
                state.installSucceeded};
    }

    // Setup refusal and cleanup are separate from the 136-field observation
    // protocol. Preserve only the SDK's unsigned scalars, never native pointers
    // or an inference that installation admitted a callback or GPU frame.
    inline std::string Format(const Fields& fields, std::uint32_t process, std::uint64_t runMs, std::uint32_t phase)
    {
        std::string line = "{\"event\":\"de_render_setup\",\"pid\":" + std::to_string(process) + ",\"run_ms\":[" +
                           std::to_string(static_cast<std::uint32_t>(runMs)) + "," + std::to_string(static_cast<std::uint32_t>(runMs >> 32)) +
                           "],\"phase\":" + std::to_string(phase) + ",\"fields\":[";
        for (std::size_t index = 0; index < fields.size(); ++index)
            line += (index ? "," : "") + std::to_string(fields[index]);
        return line + "]}\n";
    }

    class Recorder
    {
    public:
        static constexpr unsigned MaximumRecords = 32;
        template <typename Sink>
        bool Write(const mta::game_de::RenderSetupDiagnosticV1& state, std::uint32_t process, std::uint64_t runMs, std::uint32_t phase, Sink&& sink)
        {
            if (state.version != mta::game_de::RenderSetupDiagnosticVersionV1 || phase > 2 || written >= MaximumRecords ||
                (phase != 2 && written == MaximumRecords - 1))
                return false;
            const auto fields = Copy(state);
            // Every setup transition matters, including retained thread counts
            // and the first refusal surviving later cleanup. Suppress only an
            // identical sample; always leave one record available for End.
            if (phase == 1 && haveLast && fields == last)
                return false;
            if (!sink(Format(fields, process, runMs, phase)))
                return false;
            // An unavailable log must neither erase the retry nor consume the
            // bounded terminal slot. Logging cannot change cleanup ownership.
            last = fields;
            haveLast = true;
            ++written;
            return true;
        }

    private:
        Fields   last{};
        unsigned written = 0;
        bool     haveLast = false;
    };
}
