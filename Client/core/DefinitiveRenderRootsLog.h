/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded scalar logging of optional native root refusal checkpoints
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderRootsDiagnostic.h"
#include <array>
#include <cstdint>
#include <string>

namespace DefinitiveRenderRootsLog
{
    using Fields = std::array<std::uint32_t, 14>;
    inline Fields Copy(const mta::game_de::RenderRootsDiagnosticV1& state) noexcept
    {
        return {state.version,
                state.generationLow,
                state.generationHigh,
                state.acceptedSnapshotsLow,
                state.acceptedSnapshotsHigh,
                state.dispatchEntriesLow,
                state.dispatchEntriesHigh,
                state.queryPhase,
                state.checkpoint,
                state.refusalReason,
                state.exceptionCode,
                state.scalarValidMask,
                state.contextNodeMask,
                state.nativeBufferIndex};
    }

    // This separate record identifies an existing read/guard refusal. It carries
    // no native addresses and cannot alter the 136-field observation or admit GPU work.
    inline std::string Format(const Fields& fields, std::uint32_t process, std::uint64_t runMs, std::uint32_t phase)
    {
        std::string line = "{\"event\":\"de_render_roots\",\"pid\":" + std::to_string(process) + ",\"run_ms\":[" +
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
        bool Write(const mta::game_de::RenderRootsDiagnosticV1& state, std::uint32_t process, std::uint64_t runMs, std::uint32_t phase, Sink&& sink)
        {
            if (state.version != mta::game_de::RenderRootsDiagnosticVersionV1 || phase > 2 || written >= MaximumRecords ||
                (phase != 2 && written == MaximumRecords - 1))
                return false;
            const auto fields = Copy(state);
            auto       key = fields;
            // Keep correlation counters in emitted records, but counter progress
            // alone must not consume the bounded log before the final cleanup record.
            key[3] = key[4] = key[5] = key[6] = 0;
            // A callback can publish before Begin logging. Keep the first poll
            // even if it matches Begin, so the saved run proves worker polling.
            if (phase == 1 && havePoll && haveLast && key == last)
                return false;
            if (!sink(Format(fields, process, runMs, phase)))
                return false;
            last = key;
            haveLast = true;
            havePoll = havePoll || phase == 1;
            ++written;
            return true;
        }

    private:
        Fields   last{};
        unsigned written = 0;
        bool     haveLast = false;
        bool     havePoll = false;
    };
}
