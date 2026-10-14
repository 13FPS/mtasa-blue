/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Bounded value-only formatting of optional endpoint diagnostics
 *****************************************************************************/
#pragma once
#include "../sdk/game/CGameRenderEndpointDiagnostic.h"
#include <array>
#include <cstdint>
#include <string>

namespace DefinitiveRenderEndpointLog
{
    using Fields = std::array<std::uint32_t, 20>;
    inline Fields Copy(const mta::game_de::RenderEndpointDiagnosticV1& state) noexcept
    {
        return {state.version,
                state.generationLow,
                state.generationHigh,
                state.acceptedSnapshotsLow,
                state.acceptedSnapshotsHigh,
                state.preparationGate,
                state.preparationReason,
                state.errorDomain,
                state.errorCode,
                state.loadedEntryMatch,
                state.slotStatus,
                state.slotOwnerClass,
                state.slotRva,
                state.slotEqualsPinned,
                state.slotBranchKind,
                state.slotBranchOwnerClass,
                state.slotBranchRva,
                state.systemBranchKind,
                state.systemBranchOwnerClass,
                state.systemBranchRva};
    }

    // This is a separate diagnostic file, not another interpretation of the
    // 110-field observation wire. Numeric JSON cannot export native addresses.
    inline std::string Format(const Fields& fields, std::uint32_t process, std::uint64_t runMs, std::uint32_t phase)
    {
        std::string line = "{\"event\":\"de_render_endpoint\",\"pid\":" + std::to_string(process) + ",\"run_ms\":[" +
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
        bool Write(const mta::game_de::RenderEndpointDiagnosticV1& state, std::uint32_t process, std::uint64_t runMs, std::uint32_t phase, Sink&& sink)
        {
            if (state.version != mta::game_de::RenderEndpointDiagnosticVersionV1 || phase > 2 || written >= MaximumRecords ||
                (phase != 2 && written == MaximumRecords - 1))
                return false;
            const auto fields = Copy(state);
            auto       key = fields;
            // Counter progress alone is not new admission evidence. Keep the
            // actual counter in each emitted record, but bound callback churn.
            key[3] = key[4] = 0;
            if (phase == 1 && haveLast && key == last)
                return false;
            if (!sink(Format(fields, process, runMs, phase)))
                return false;
            last = key;
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
