/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Check movement routing and physical pad restoration independently
 *****************************************************************************/
#include "../../Client/game_de/NativeInputRouting.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{
    using namespace mta::game_de;
    using namespace mta::game_de::NativeInputRouting;
    int  checks = 0;
    void Check(bool value, const char* reason)
    {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    }
    using Field = signed short      CControllerState::*;
    constexpr std::array<Field, 20> unsupported = {&CControllerState::RightStickX,
                                                   &CControllerState::RightStickY,
                                                   &CControllerState::LeftShoulder1,
                                                   &CControllerState::LeftShoulder2,
                                                   &CControllerState::RightShoulder1,
                                                   &CControllerState::RightShoulder2,
                                                   &CControllerState::DPadUp,
                                                   &CControllerState::DPadDown,
                                                   &CControllerState::DPadLeft,
                                                   &CControllerState::DPadRight,
                                                   &CControllerState::Start,
                                                   &CControllerState::Select,
                                                   &CControllerState::ButtonSquare,
                                                   &CControllerState::ButtonTriangle,
                                                   &CControllerState::ButtonCircle,
                                                   &CControllerState::ShockButtonL,
                                                   &CControllerState::ShockButtonR,
                                                   &CControllerState::m_bChatIndicated,
                                                   &CControllerState::m_bVehicleMouseLook,
                                                   &CControllerState::m_bRadioTrackSkip};
    static_assert(sizeof(CControllerState) == 48 && sizeof(PadStateBytes) == 96);
    static_assert(std::is_standard_layout_v<CControllerState> && std::is_trivially_copyable_v<CControllerState>);
    static_assert(!std::is_copy_constructible_v<PadTransaction> && !std::is_copy_assignable_v<PadTransaction>);
    static_assert(QueryDomain(0) && QueryDomain(255) && !QueryDomain(256));
    static_assert(noexcept(QueryAction(std::declval<const MtaInputState&>(), 2, std::declval<ActionSample&>())));

    void Query(const MtaInputState& input, std::uint32_t action, float value, std::int32_t edge)
    {
        ActionSample sample{17.0f, -57};
        Check(QueryAction(input, action, sample), "valid scoped query must be authoritatively routed");
        Check(sample.value == value && sample.state == edge, "route must preserve independently specified magnitude and edge");
    }
    void Rejected(const MtaInputState& input)
    {
        Check(!ValidateMovementState(input), "unsupported canonical controller fields must reject both frames");
        for (const auto action : {std::uint32_t{2}, std::uint32_t{0x1e}, std::uint32_t{0x1f}, std::uint32_t{0x6d}, std::uint32_t{0xff}})
        {
            ActionSample sample{19.0f, -91};
            Check(!QueryAction(input, action, sample), "invalid frame cannot route a supported or unknown action");
            Check(sample.value == 19.0f && sample.state == -91, "failed query must preserve the complete output");
        }
        PadStateBytes physical{}, replacement{};
        physical.fill(std::byte{0x9d});
        replacement.fill(std::byte{0x37});
        const auto     before = replacement;
        PadTransaction transaction;
        Check(!transaction.Prepare(physical, input, replacement) && !transaction.Active(), "invalid frame cannot acquire transaction ownership");
        Check(replacement == before, "invalid prepare is atomic for replacement bytes");
    }
    void TestEdgesAndMagnitudes()
    {
        struct Edge
        {
            signed short current;
            signed short previous;
            std::int32_t state;
            float        value;
        };
        const std::array<Edge, 4> edges = {{{0, 255, 0, 0.0f}, {0, 0, 1, 0.0f}, {255, 0, 2, 1.0f}, {255, 255, 3, 1.0f}}};
        for (const auto& edge : edges)
        {
            MtaInputState input;
            input.current.ButtonCross = edge.current;
            input.previous.ButtonCross = edge.previous;
            Query(input, 2, edge.value, edge.state);
        }
        for (int value = -128; value <= 128; ++value)
        {
            for (const auto action : {std::uint32_t{0x1e}, std::uint32_t{0x1f}})
            {
                const auto    field = action == 0x1e ? &CControllerState::LeftStickX : &CControllerState::LeftStickY;
                MtaInputState input;
                input.current.*field = static_cast<signed short>(value);
                Query(input, action, static_cast<float>(value) / 128.0f, value == 0 ? 1 : 2);
                input.previous.*field = value == 0 ? 64 : static_cast<signed short>(-value);
                Query(input, action, static_cast<float>(value) / 128.0f, value == 0 ? 0 : 3);
            }
        }
        MtaInputState input;
        input.current.LeftStickX = -128;
        input.current.LeftStickY = 64;
        input.previous.LeftStickX = 0;
        input.previous.LeftStickY = -128;
        input.current.ButtonCross = 255;
        input.previous.ButtonCross = 0;
        for (const auto walk : {static_cast<signed short>(0), static_cast<signed short>(255)})
        {
            input.current.m_bPedWalk = walk;
            input.previous.m_bPedWalk = 255;
            for (std::uint32_t action = 0; action < 256; ++action)
            {
                Check(QueryDomain(action), "every native action in the verified query domain has authoritative membership");
                if (action != 2 && action != 0x1e && action != 0x1f)
                    Query(input, action, 0.0f, 1);
            }
            Query(input, 0x1e, -1.0f, 2);
            Query(input, 0x1f, 0.5f, 3);
            Query(input, 0x6d, 0.0f, 1);
        }
        for (const auto invalid : {std::uint32_t{256}, std::numeric_limits<std::uint32_t>::max()})
        {
            ActionSample out{3.0f, -3};
            Check(!QueryDomain(invalid) && !QueryAction(input, invalid, out), "out-of-domain and negative-as-unsigned IDs reject");
            Check(out.value == 3.0f && out.state == -3, "invalid action preserves output");
        }
    }
    void TestUnsupportedFrames()
    {
        for (const auto previous : {false, true})
        {
            for (const auto field : unsupported)
            {
                for (const auto value :
                     {static_cast<signed short>(-1), static_cast<signed short>(1), static_cast<signed short>(255), (std::numeric_limits<signed short>::min)()})
                {
                    MtaInputState input;
                    (previous ? input.previous : input.current).*field = value;
                    Rejected(input);
                }
            }
            for (const auto field : {&CControllerState::LeftStickX, &CControllerState::LeftStickY})
            {
                for (const auto value : {static_cast<signed short>(-129), static_cast<signed short>(129), (std::numeric_limits<signed short>::min)(),
                                         (std::numeric_limits<signed short>::max)()})
                {
                    MtaInputState input;
                    (previous ? input.previous : input.current).*field = value;
                    Rejected(input);
                }
            }
            for (const auto field : {&CControllerState::ButtonCross, &CControllerState::m_bPedWalk})
            {
                for (const auto value : {static_cast<signed short>(-1), static_cast<signed short>(1), static_cast<signed short>(128),
                                         static_cast<signed short>(254), static_cast<signed short>(256)})
                {
                    MtaInputState input;
                    (previous ? input.previous : input.current).*field = value;
                    Rejected(input);
                }
            }
        }
        // Consume the actual shared MTA producer, covering opposite directions,
        // walking/sprint combinations and its retained direction history.
        constexpr std::array<eControllerAction, 6> actions = {FORWARDS, BACKWARDS, LEFT, RIGHT, SPRINT, WALK};
        MtaInput::DirectionHistory                 history;
        MtaInputState                              input;
        for (unsigned mask = 0; mask < 64; ++mask)
        {
            std::uint64_t held = 0;
            for (unsigned bit = 0; bit < actions.size(); ++bit)
                if (mask & (1u << bit))
                    held |= MtaInput::ActionBit(actions[bit]);
            input.previous = input.current;
            input.current = MtaInput::BuildControllerState(held, {}, history);
            Check(ValidateMovementState(input), "all actual MTA movement-only producer frames must be accepted unchanged");
            Query(input, 2, mask & (1u << 4) ? 1.0f : 0.0f, mask & (1u << 4) ? (input.previous.ButtonCross ? 3 : 2) : (input.previous.ButtonCross ? 0 : 1));
        }
    }
    MtaInputState MovementFrame()
    {
        MtaInputState input;
        input.current.LeftStickX = -128;
        input.current.LeftStickY = 73;
        input.current.ButtonCross = 255;
        input.current.m_bPedWalk = 255;
        input.previous.LeftStickX = 36;
        input.previous.LeftStickY = -128;
        input.previous.ButtonCross = 0;
        input.previous.m_bPedWalk = 255;
        return input;
    }
    PadStateBytes ExpectedCanonicalBytes()
    {
        // Freeze byte positions independently of the helper's struct memcpy.
        // These are real arrays of int16 storage, never a reinterpreted struct.
        std::array<std::int16_t, 48> fields{};
        fields[0] = -128;
        fields[1] = 73;
        fields[16] = 255;
        fields[21] = 255;
        fields[24] = 36;
        fields[25] = -128;
        fields[40] = 0;
        fields[45] = 255;
        PadStateBytes bytes{};
        std::memcpy(bytes.data(), fields.data(), bytes.size());
        return bytes;
    }
    void TestTransactions()
    {
        const auto    input = MovementFrame();
        const auto    expected = ExpectedCanonicalBytes();
        PadStateBytes physical{};
        for (std::size_t at = 0; at < physical.size(); ++at)
            physical[at] = static_cast<std::byte>((at * 47 + 13) & 0xff);
        for (std::size_t appliedBytes = 0; appliedBytes <= 96; ++appliedBytes)
        {
            PadTransaction transaction;
            PadStateBytes  replacement{};
            Check(transaction.Prepare(physical, input, replacement) && transaction.Active(), "fresh transaction acquires exactly one restore snapshot");
            Check(replacement == expected, "current/old typed fields occupy independently specified canonical positions");
            std::array<std::byte, 0x188> pad{};
            pad.fill(std::byte{0xc7});
            std::memcpy(pad.data() + 0x38, physical.data(), physical.size());
            const auto before = pad;
            std::memcpy(pad.data() + 0x38, replacement.data(), appliedBytes);
            PadStateBytes nested{};
            nested.fill(std::byte{0x19});
            const auto nestedBefore = nested;
            Check(!transaction.Prepare(replacement, input, nested) && nested == nestedBefore,
                  "nested acquisition cannot replace the original snapshot or output");
            PadStateBytes restore{};
            Check(transaction.RestoreBytes(restore) && restore == physical && transaction.Active(),
                  "partial apply retains every original physical byte and ownership");
            PadStateBytes wrong = physical;
            wrong[appliedBytes % wrong.size()] ^= std::byte{1};
            Check(!transaction.ConfirmRestored(wrong) && transaction.Active(), "mismatched restoration readback cannot clear ownership");
            PadStateBytes retry{};
            Check(transaction.RestoreBytes(retry) && retry == physical, "failed readback retains the recovery snapshot");
            std::memcpy(pad.data() + 0x38, retry.data(), retry.size());
            Check(pad == before, "restoration preserves the complete physical pad including poisoned prefix/suffix");
            Check(transaction.ConfirmRestored(retry) && !transaction.Active(), "exact restore readback releases ownership");
            restore.fill(std::byte{0x82});
            const auto noOwner = restore;
            Check(!transaction.RestoreBytes(restore) && restore == noOwner && !transaction.ConfirmRestored(physical),
                  "inactive transaction cannot invent or confirm a restore");
            Check(transaction.Prepare(physical, input, replacement), "completed restoration permits the next context");
            Check(transaction.RestoreBytes(restore) && transaction.ConfirmRestored(restore), "next context retains independent restoration lifetime");
        }
        PadTransaction transaction;
        auto           alias = physical;
        Check(transaction.Prepare(alias, input, alias) && alias == expected, "aliased input/output staging still saves original bytes first");
        Check(transaction.RestoreBytes(alias) && alias == physical && transaction.ConfirmRestored(alias), "aliased staging restores original physical state");
    }
}
int main()
{
    try
    {
        TestEdgesAndMagnitudes();
        TestUnsupportedFrames();
        TestTransactions();
        std::cout << "Passed " << checks << " native input routing checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Native input routing failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
