/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Preserve classic logical-control synthesis independently of a backend
 *****************************************************************************/

#include "../../Shared/sdk/CControlStateSynthesis.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    std::uint64_t checks = 0;
    std::uint64_t frames = 0;

    void Check(bool condition, const char* description)
    {
        ++checks;
        if (!condition)
            throw std::runtime_error(description);
    }

    // Frozen BEFORE extraction from git HEAD, independent of the shared helper:
    // CKeyBinds.cpp blob 3a66d5f3e51e28a91a91fc721a82c0f4cc99304f,
    // CKeyBinds.h blob ecde1221b791237fe06f539a1ce46e33d6223938,
    // CControllerConfigManager.h blob 91b280f3b23fb473475eda79b0a6959bb46cb052.
    // Original g_bcControls order is sparse in eControllerAction. Numeric values
    // here prevent the production enum/mask helpers from redefining the oracle.
    constexpr std::array<unsigned int, 44> legacyActionNumbers = {0,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                                                  23, 24, 25, 26, 27, 28, 29, 30, 31, 34, 35, 36, 37, 38, 39, 40, 41, 45, 47, 48, 49, 50};

    constexpr std::array<eControllerAction, 44> namedActions = {FIRE,
                                                                NEXT_WEAPON,
                                                                PREVIOUS_WEAPON,
                                                                FORWARDS,
                                                                BACKWARDS,
                                                                LEFT,
                                                                RIGHT,
                                                                ZOOM_IN,
                                                                ZOOM_OUT,
                                                                ENTER_EXIT,
                                                                CHANGE_CAMERA,
                                                                JUMP,
                                                                SPRINT,
                                                                LOOK_BEHIND,
                                                                CROUCH,
                                                                ACTION,
                                                                WALK,
                                                                VEHICLE_FIRE,
                                                                VEHICLE_SECONDARY_FIRE,
                                                                VEHICLE_LEFT,
                                                                VEHICLE_RIGHT,
                                                                STEER_FORWARDS_DOWN,
                                                                STEER_BACK_UP,
                                                                ACCELERATE,
                                                                BRAKE_REVERSE,
                                                                RADIO_NEXT,
                                                                RADIO_PREVIOUS,
                                                                RADIO_USER_TRACK_SKIP,
                                                                HORN,
                                                                SUB_MISSION,
                                                                HANDBRAKE,
                                                                VEHICLE_LOOK_LEFT,
                                                                VEHICLE_LOOK_RIGHT,
                                                                VEHICLE_LOOK_BEHIND,
                                                                VEHICLE_MOUSE_LOOK,
                                                                SPECIAL_CONTROL_LEFT,
                                                                SPECIAL_CONTROL_RIGHT,
                                                                SPECIAL_CONTROL_DOWN,
                                                                SPECIAL_CONTROL_UP,
                                                                AIM_WEAPON,
                                                                CONVERSATION_YES,
                                                                CONVERSATION_NO,
                                                                GROUP_CONTROL_FORWARDS,
                                                                GROUP_CONTROL_BACK};

    constexpr std::uint64_t LegacyBit(std::size_t index)
    {
        return std::uint64_t{1} << legacyActionNumbers[index];
    }

    constexpr std::uint64_t LegacyMask()
    {
        std::uint64_t mask = 0;
        for (auto number : legacyActionNumbers)
            mask |= std::uint64_t{1} << number;
        return mask;
    }

    static_assert(noexcept(MtaInput::BuildControllerState(std::uint64_t{}, std::declval<const MtaInput::Context&>(),
                                                          std::declval<MtaInput::DirectionHistory&>())),
                  "Logical control synthesis must not throw across a backend boundary");

    struct LegacyHistory
    {
        bool lastForwards = false;
        bool lastBackwards = false;
        bool moveForwards = false;
        bool lastLeft = false;
        bool lastRight = false;
        bool moveLeft = false;
    };

    // These branches preserve the ORIGINAL methods and indexed g_bcControls
    // formulas rather than calling, adapting or generating expectations from
    // BuildControllerState. Context acquisition, GUI/joystick effects, engine
    // pad writes and bind dispatch intentionally stay outside this pure oracle.
    bool LegacyForwardsBackwards(CControllerState& cs, const std::array<bool, 44>& controls, LegacyHistory& history)
    {
        bool currentForwards = controls[3];
        bool currentBackwards = controls[4];
        if (currentForwards && !currentBackwards)
        {
            history.lastForwards = true;
            history.lastBackwards = false;
        }
        else if (!currentForwards && currentBackwards)
        {
            history.lastForwards = false;
            history.lastBackwards = true;
        }
        bool both = false;
        if (currentForwards && currentBackwards)
        {
            both = true;
            if (!history.lastForwards && history.lastBackwards)
                history.moveForwards = true;
            else if (history.lastForwards && !history.lastBackwards)
                history.moveForwards = false;
            bool forwards = history.moveForwards;
            bool backwards = !history.moveForwards;
            cs.LeftStickY = static_cast<short>(forwards ? forwards * -128 : backwards * 128);
        }
        return both;
    }

    bool LegacyLeftRight(CControllerState& cs, const std::array<bool, 44>& controls, LegacyHistory& history)
    {
        bool currentLeft = controls[5];
        bool currentRight = controls[6];
        if (currentLeft && !currentRight)
        {
            history.lastLeft = true;
            history.lastRight = false;
        }
        else if (!currentLeft && currentRight)
        {
            history.lastLeft = false;
            history.lastRight = true;
        }
        bool both = false;
        if (currentLeft && currentRight)
        {
            both = true;
            if (!history.lastLeft && history.lastRight)
                history.moveLeft = true;
            else if (history.lastLeft && !history.lastRight)
                history.moveLeft = false;
            bool left = history.moveLeft;
            bool right = !history.moveLeft;
            cs.LeftStickX = static_cast<short>(left ? left * -128 : right * 128);
        }
        return both;
    }

    CControllerState LegacySynthesis(std::uint64_t held, const MtaInput::Context& context, LegacyHistory& history)
    {
        std::array<bool, 44> controls{};
        for (std::size_t index = 0; index < controls.size(); ++index)
            controls[index] = (held & LegacyBit(index)) != 0;
        CControllerState cs;
        if (!context.isDead)
        {
            if (!context.inVehicle)
            {
                cs.ButtonCircle = controls[0] && !context.hasDetonator ? 255 : 0;
                if (context.aimingWeapon)
                {
                    cs.RightShoulder2 = controls[8] ? 255 : 0;
                    cs.LeftShoulder2 = controls[7] ? 255 : 0;
                }
                else
                {
                    bool nextWeapon = controls[1];
                    bool previousWeapon = controls[2];
                    if (nextWeapon && previousWeapon)
                        nextWeapon = previousWeapon = false;
                    cs.RightShoulder2 = nextWeapon ? 255 : 0;
                    cs.LeftShoulder2 = previousWeapon ? 255 : 0;
                }
                if (!LegacyForwardsBackwards(cs, controls, history))
                    cs.LeftStickY = (!controls[3] && !controls[4]) ? 0 : controls[3] ? -128 : 128;
                if (!LegacyLeftRight(cs, controls, history))
                    cs.LeftStickX = (!controls[5] && !controls[6]) ? 0 : controls[5] ? -128 : 128;
                cs.ButtonSquare = !context.enteringVehicle && !context.aimingWeapon && controls[11] ? 255 : 0;
                cs.ButtonCross = controls[12] ? 255 : 0;
                cs.ShockButtonR = controls[13] ? 255 : 0;
                cs.ShockButtonL = controls[14] ? 255 : 0;
                cs.LeftShoulder1 = controls[15] ? 255 : 0;
                cs.m_bPedWalk = controls[16] ? 255 : 0;
                cs.RightShoulder1 = controls[39] ? 255 : 0;
                cs.DPadRight = controls[40] ? 255 : 0;
                cs.DPadLeft = controls[41] ? 255 : 0;
                cs.DPadUp = controls[42] ? 255 : 0;
                cs.DPadDown = controls[43] ? 255 : 0;
            }
            else
            {
                cs.ButtonCircle = controls[17] ? 255 : 0;
                cs.LeftShoulder1 = controls[18] ? 255 : 0;
                cs.LeftStickX = ((controls[19] && controls[20]) || (!controls[19] && !controls[20])) ? 0 : controls[19] ? -128 : 128;
                cs.LeftStickY = ((controls[21] && controls[22]) || (!controls[21] && !controls[22])) ? 0 : controls[21] ? -128 : 128;
                cs.ButtonCross = controls[23] ? 255 : 0;
                cs.ButtonSquare = controls[24] ? 255 : 0;
                cs.DPadUp = controls[25] ? 255 : 0;
                cs.DPadDown = controls[26] ? 255 : 0;
                cs.m_bRadioTrackSkip = controls[27] ? 255 : 0;
                cs.ShockButtonL = controls[28] ? 255 : 0;
                cs.ShockButtonR = controls[29] ? 255 : 0;
                cs.RightShoulder1 = controls[30] ? 255 : 0;
                cs.LeftShoulder2 = controls[31] || controls[33] ? 255 : 0;
                cs.RightShoulder2 = controls[32] || controls[33] ? 255 : 0;
                cs.RightStickX = ((controls[35] && controls[36]) || (!controls[35] && !controls[36])) ? 0 : controls[35] ? 128 : -128;
                cs.RightStickY = ((controls[37] && controls[38]) || (!controls[37] && !controls[38])) ? 0 : controls[37] ? 128 : -128;
            }
            cs.ButtonTriangle = controls[9] ? 255 : 0;
            cs.Select = controls[10] ? 255 : 0;
        }
        return cs;
    }

    struct Field
    {
        const char*  name;
        signed short CControllerState::* member;
    };

    constexpr std::array<Field, 24> fields = {{{"LeftStickX", &CControllerState::LeftStickX},
                                               {"LeftStickY", &CControllerState::LeftStickY},
                                               {"RightStickX", &CControllerState::RightStickX},
                                               {"RightStickY", &CControllerState::RightStickY},
                                               {"LeftShoulder1", &CControllerState::LeftShoulder1},
                                               {"LeftShoulder2", &CControllerState::LeftShoulder2},
                                               {"RightShoulder1", &CControllerState::RightShoulder1},
                                               {"RightShoulder2", &CControllerState::RightShoulder2},
                                               {"DPadUp", &CControllerState::DPadUp},
                                               {"DPadDown", &CControllerState::DPadDown},
                                               {"DPadLeft", &CControllerState::DPadLeft},
                                               {"DPadRight", &CControllerState::DPadRight},
                                               {"Start", &CControllerState::Start},
                                               {"Select", &CControllerState::Select},
                                               {"ButtonSquare", &CControllerState::ButtonSquare},
                                               {"ButtonTriangle", &CControllerState::ButtonTriangle},
                                               {"ButtonCross", &CControllerState::ButtonCross},
                                               {"ButtonCircle", &CControllerState::ButtonCircle},
                                               {"ShockButtonL", &CControllerState::ShockButtonL},
                                               {"ShockButtonR", &CControllerState::ShockButtonR},
                                               {"m_bChatIndicated", &CControllerState::m_bChatIndicated},
                                               {"m_bPedWalk", &CControllerState::m_bPedWalk},
                                               {"m_bVehicleMouseLook", &CControllerState::m_bVehicleMouseLook},
                                               {"m_bRadioTrackSkip", &CControllerState::m_bRadioTrackSkip}}};

    MtaInput::Context ContextFromBits(unsigned int bits)
    {
        MtaInput::Context context;
        context.inVehicle = (bits & 1u) != 0;
        context.hasDetonator = (bits & 2u) != 0;
        context.isDead = (bits & 4u) != 0;
        context.aimingWeapon = (bits & 8u) != 0;
        context.enteringVehicle = (bits & 16u) != 0;
        return context;
    }

    void HistoryFromBits(unsigned int bits, MtaInput::DirectionHistory& actual, LegacyHistory& expected)
    {
        actual.lastForwards = expected.lastForwards = (bits & 1u) != 0;
        actual.lastBackwards = expected.lastBackwards = (bits & 2u) != 0;
        actual.moveForwards = expected.moveForwards = (bits & 4u) != 0;
        actual.lastLeft = expected.lastLeft = (bits & 8u) != 0;
        actual.lastRight = expected.lastRight = (bits & 16u) != 0;
        actual.moveLeft = expected.moveLeft = (bits & 32u) != 0;
    }

    CControllerState Compare(std::uint64_t held, const MtaInput::Context& context, MtaInput::DirectionHistory& actualHistory, LegacyHistory& expectedHistory)
    {
        ++frames;
        auto expected = LegacySynthesis(held, context, expectedHistory);
        auto actual = MtaInput::BuildControllerState(held, context, actualHistory);
        for (const auto& field : fields)
        {
            ++checks;
            if (actual.*field.member != expected.*field.member)
                throw std::runtime_error("Differential frame " + std::to_string(frames) + " field " + field.name + " held mask " + std::to_string(held) +
                                         ": expected " + std::to_string(expected.*field.member) + ", actual " + std::to_string(actual.*field.member));
        }
        Check(actualHistory.lastForwards == expectedHistory.lastForwards, "Forward history differs from original");
        Check(actualHistory.lastBackwards == expectedHistory.lastBackwards, "Backward history differs from original");
        Check(actualHistory.moveForwards == expectedHistory.moveForwards, "Vertical winner differs from original");
        Check(actualHistory.lastLeft == expectedHistory.lastLeft, "Left history differs from original");
        Check(actualHistory.lastRight == expectedHistory.lastRight, "Right history differs from original");
        Check(actualHistory.moveLeft == expectedHistory.moveLeft, "Horizontal winner differs from original");
        return actual;
    }

    void ExhaustiveContextsAndActions()
    {
        for (unsigned int contextBits = 0; contextBits < 32; ++contextBits)
        {
            const auto context = ContextFromBits(contextBits);
            for (std::size_t first = 0; first < legacyActionNumbers.size(); ++first)
            {
                MtaInput::DirectionHistory actual;
                LegacyHistory              expected;
                Compare(LegacyBit(first), context, actual, expected);
                for (std::size_t second = 0; second < legacyActionNumbers.size(); ++second)
                {
                    // Both press orders matter for direction history even when
                    // a two-action held mask is identical on the second frame.
                    actual = MtaInput::DirectionHistory();
                    expected = LegacyHistory();
                    Compare(LegacyBit(first), context, actual, expected);
                    Compare(LegacyBit(first) | LegacyBit(second), context, actual, expected);
                    Compare(LegacyBit(second), context, actual, expected);
                    Compare(0, context, actual, expected);
                }
            }
            MtaInput::DirectionHistory actual;
            LegacyHistory              expected;
            Compare(0, context, actual, expected);
            Compare(LegacyMask(), context, actual, expected);
        }
    }

    void ExhaustiveInitialHistory()
    {
        for (unsigned int historyBits = 0; historyBits < 64; ++historyBits)
        {
            for (unsigned int contextBits = 0; contextBits < 32; ++contextBits)
            {
                for (unsigned int directions = 0; directions < 16; ++directions)
                {
                    MtaInput::DirectionHistory actual;
                    LegacyHistory              expected;
                    HistoryFromBits(historyBits, actual, expected);
                    std::uint64_t held = 0;
                    for (std::size_t index = 0; index < 4; ++index)
                        if (directions & (1u << index))
                            held |= LegacyBit(index + 3);
                    Compare(held, ContextFromBits(contextBits), actual, expected);
                }
            }
        }
    }

    void ExplicitPolicySequences()
    {
        MtaInput::DirectionHistory actual;
        LegacyHistory              expected;
        const auto                 foot = ContextFromBits(0);
        const auto                 bothAxes = LegacyBit(3) | LegacyBit(4) | LegacyBit(5) | LegacyBit(6);
        auto                       state = Compare(bothAxes, foot, actual, expected);
        Check(state.LeftStickX == 128 && state.LeftStickY == 128, "Fresh simultaneous opposites retain original positive fallback");
        Compare(LegacyBit(3) | LegacyBit(5), foot, actual, expected);
        Compare(0, foot, actual, expected);
        state = Compare(bothAxes, foot, actual, expected);
        Check(state.LeftStickX == 128 && state.LeftStickY == 128, "Neutral retains last solitary direction; newly added opposite wins");
        Compare(LegacyBit(4) | LegacyBit(6), foot, actual, expected);
        Compare(0, foot, actual, expected);
        state = Compare(bothAxes, foot, actual, expected);
        Check(state.LeftStickX == -128 && state.LeftStickY == -128, "Reversed press order preserves original negative winners");
        Compare(LegacyBit(3) | LegacyBit(5), ContextFromBits(4), actual, expected);
        Compare(LegacyBit(3) | LegacyBit(5), ContextFromBits(1), actual, expected);
        state = Compare(bothAxes, foot, actual, expected);
        Check(state.LeftStickX == -128 && state.LeftStickY == -128, "Dead and vehicle frames must not rewrite foot history");
        state = Compare(LegacyBit(0) | LegacyBit(11), ContextFromBits(2), actual, expected);
        Check(state.ButtonCircle == 0 && state.ButtonSquare == 255, "Detonator suppresses fire only");
        state = Compare(LegacyBit(11) | LegacyBit(12) | LegacyBit(16), ContextFromBits(8), actual, expected);
        Check(state.ButtonSquare == 0 && state.ButtonCross == 255 && state.m_bPedWalk == 255, "Aim suppresses jump, retaining sprint/walk policy");
        state = Compare(LegacyBit(1) | LegacyBit(2), foot, actual, expected);
        Check(state.RightShoulder2 == 0 && state.LeftShoulder2 == 0, "Opposed weapon cycling cancels");
        state = Compare(LegacyBit(31) | LegacyBit(32) | LegacyBit(33), ContextFromBits(1), actual, expected);
        Check(state.LeftShoulder2 == 255 && state.RightShoulder2 == 255, "Vehicle look-behind uses both shoulders");
        state = Compare(LegacyBit(34), ContextFromBits(1), actual, expected);
        Check(state.m_bVehicleMouseLook == 0, "Original unassigned mouse-look field must not become invented coverage");
    }

    void DeterministicMixedFrames()
    {
        std::uint64_t              seed = 0x6f726967696e616cULL;
        MtaInput::DirectionHistory actual;
        LegacyHistory              expected;
        for (unsigned int frame = 0; frame < 16384; ++frame)
        {
            // Fixed xorshift inputs give reproducible changes, unknown bits,
            // contradictory directions, and context transitions across frames.
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            Compare(seed, ContextFromBits(frame % 32), actual, expected);
        }
    }
}

int main()
{
    try
    {
        Check(MtaInput::KnownActionMask == LegacyMask(), "KnownActionMask retains exactly the original forty-four sparse actions");
        for (std::size_t index = 0; index < namedActions.size(); ++index)
        {
            Check(static_cast<unsigned int>(namedActions[index]) == legacyActionNumbers[index], "Original controller action value changed");
            Check(MtaInput::ActionBit(namedActions[index]) == LegacyBit(index), "Named action does not produce its original sparse bit");
        }
        Check(MtaInput::ActionBit(static_cast<eControllerAction>(-1)) == 0, "Negative action must not cause an invalid shift");
        Check(MtaInput::ActionBit(static_cast<eControllerAction>(64)) == 0, "Action beyond mask width must not cause an invalid shift");
        ExhaustiveContextsAndActions();
        ExhaustiveInitialHistory();
        ExplicitPolicySequences();
        DeterministicMixedFrames();
        std::cout << "Passed " << checks << " original MTA control-synthesis checks across " << frames << " differential frames\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Control synthesis preservation failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
