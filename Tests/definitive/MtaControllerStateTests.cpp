/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  PURPOSE:     Verify the shared MTA controller-state contract on x86 and x64
 *
 *****************************************************************************/

#include "../../Shared/sdk/CControllerState.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace
{
    int checks = 0;

    void Check(bool condition, const std::string& message)
    {
        ++checks;
        // The pad and keysync code consume this layout in optimized builds too,
        // so Release must retain the checks rather than compile out assertions.
        if (!condition)
            throw std::runtime_error(message);
    }

    static_assert(sizeof(signed short) == 2, "MTA controller fields use two-byte signed shorts");
    static_assert(sizeof(CControllerState) == 48, "MTA controller state retains its 24-field layout");
    static_assert(alignof(CControllerState) == alignof(signed short), "MTA controller alignment remains unchanged");
    static_assert(std::is_standard_layout_v<CControllerState>, "MTA pad fields retain a standard layout");
    static_assert(std::is_trivially_copyable_v<CControllerState>, "Existing keysync memcpy remains valid");

    // These offsets are the existing MTA contract, written independently of
    // sizeof/member iteration so reordered or widened fields cannot pass.
#define CHECK_FIELD(member, offset) \
    static_assert(std::is_same_v<decltype(CControllerState::member), signed short>, #member " remains signed short"); \
    static_assert(offsetof(CControllerState, member) == offset, #member " retains its MTA pad offset")

    CHECK_FIELD(LeftStickX, 0);
    CHECK_FIELD(LeftStickY, 2);
    CHECK_FIELD(RightStickX, 4);
    CHECK_FIELD(RightStickY, 6);
    CHECK_FIELD(LeftShoulder1, 8);
    CHECK_FIELD(LeftShoulder2, 10);
    CHECK_FIELD(RightShoulder1, 12);
    CHECK_FIELD(RightShoulder2, 14);
    CHECK_FIELD(DPadUp, 16);
    CHECK_FIELD(DPadDown, 18);
    CHECK_FIELD(DPadLeft, 20);
    CHECK_FIELD(DPadRight, 22);
    CHECK_FIELD(Start, 24);
    CHECK_FIELD(Select, 26);
    CHECK_FIELD(ButtonSquare, 28);
    CHECK_FIELD(ButtonTriangle, 30);
    CHECK_FIELD(ButtonCross, 32);
    CHECK_FIELD(ButtonCircle, 34);
    CHECK_FIELD(ShockButtonL, 36);
    CHECK_FIELD(ShockButtonR, 38);
    CHECK_FIELD(m_bChatIndicated, 40);
    CHECK_FIELD(m_bPedWalk, 42);
    CHECK_FIELD(m_bVehicleMouseLook, 44);
    CHECK_FIELD(m_bRadioTrackSkip, 46);

#undef CHECK_FIELD

    struct Field
    {
        const char*  name;
        signed short CControllerState::* member;
        signed short                     value;
    };

    // Distinct values expose omission and cross-field copying while retaining
    // negative analog input, the 128 boundary, and the full 255 button value.
    const std::array<Field, 24> fields = {{{"LeftStickX", &CControllerState::LeftStickX, -128},
                                           {"LeftStickY", &CControllerState::LeftStickY, 128},
                                           {"RightStickX", &CControllerState::RightStickX, -127},
                                           {"RightStickY", &CControllerState::RightStickY, 127},
                                           {"LeftShoulder1", &CControllerState::LeftShoulder1, 255},
                                           {"LeftShoulder2", &CControllerState::LeftShoulder2, 2},
                                           {"RightShoulder1", &CControllerState::RightShoulder1, 3},
                                           {"RightShoulder2", &CControllerState::RightShoulder2, 4},
                                           {"DPadUp", &CControllerState::DPadUp, 5},
                                           {"DPadDown", &CControllerState::DPadDown, 6},
                                           {"DPadLeft", &CControllerState::DPadLeft, 7},
                                           {"DPadRight", &CControllerState::DPadRight, 8},
                                           {"Start", &CControllerState::Start, 9},
                                           {"Select", &CControllerState::Select, 10},
                                           {"ButtonSquare", &CControllerState::ButtonSquare, 11},
                                           {"ButtonTriangle", &CControllerState::ButtonTriangle, 12},
                                           {"ButtonCross", &CControllerState::ButtonCross, 13},
                                           {"ButtonCircle", &CControllerState::ButtonCircle, 14},
                                           {"ShockButtonL", &CControllerState::ShockButtonL, 15},
                                           {"ShockButtonR", &CControllerState::ShockButtonR, 16},
                                           {"m_bChatIndicated", &CControllerState::m_bChatIndicated, 17},
                                           {"m_bPedWalk", &CControllerState::m_bPedWalk, 18},
                                           {"m_bVehicleMouseLook", &CControllerState::m_bVehicleMouseLook, 19},
                                           {"m_bRadioTrackSkip", &CControllerState::m_bRadioTrackSkip, 20}}};

    void TestZeroInitialization()
    {
        // A reused buffer exposes a constructor that leaves any prior input
        // behind, without inventing a game pad or requiring a Windows header.
        alignas(CControllerState) std::array<std::byte, sizeof(CControllerState)> storage;
        storage.fill(std::byte{0xa5});
        auto* state = new (storage.data()) CControllerState;
        for (const auto& field : fields)
            Check(state->*(field.member) == 0, std::string(field.name) + " initializes to zero");
        for (const auto byte : storage)
            Check(byte == std::byte{0}, "constructor clears the complete controller representation");
        state->~CControllerState();
    }

    void TestValuesAndCopy()
    {
        CControllerState source;
        for (const auto& field : fields)
            source.*(field.member) = field.value;
        const CControllerState copied(source);
        CControllerState       assigned;
        assigned = source;
        for (const auto& field : fields)
        {
            Check(source.*(field.member) == field.value, std::string(field.name) + " preserves its signed value");
            Check(copied.*(field.member) == field.value, std::string(field.name) + " survives copy construction");
            Check(assigned.*(field.member) == field.value, std::string(field.name) + " survives assignment");
        }

        source.LeftStickX = 0;
        source.ButtonCross = 0;
        source.m_bPedWalk = 0;
        Check(copied.LeftStickX == -128 && copied.ButtonCross == 13 && copied.m_bPedWalk == 18,
              "copied input is a value snapshot independent of later source changes");
        Check(assigned.LeftStickX == -128 && assigned.ButtonCross == 13 && assigned.m_bPedWalk == 18,
              "assigned input is a value snapshot independent of later source changes");
    }

    void TestCurrentAndPreviousSnapshots()
    {
        // These are ordinary value snapshots, not an imitation of engine input
        // binding. They lock the copy semantics used by existing MTA keysync.
        CControllerState current;
        for (const auto& field : fields)
            current.*(field.member) = field.value;
        const CControllerState previous = current;
        current = CControllerState();
        current.LeftStickY = -128;
        current.ButtonCross = 255;
        current.m_bPedWalk = 1;
        for (const auto& field : fields)
            Check(previous.*(field.member) == field.value, std::string(field.name) + " remains in the previous snapshot");
        for (const auto& field : fields)
        {
            signed short expected = 0;
            if (field.member == &CControllerState::LeftStickY)
                expected = -128;
            else if (field.member == &CControllerState::ButtonCross)
                expected = 255;
            else if (field.member == &CControllerState::m_bPedWalk)
                expected = 1;
            Check(current.*(field.member) == expected, std::string(field.name) + " belongs to the current snapshot");
        }
    }

    void TestRawCopyContract()
    {
        // Classic pad snapshots and packet recording copy the object bytes.
        // Verify that this existing contract still carries every control field.
        CControllerState source;
        for (const auto& field : fields)
            source.*(field.member) = field.value;
        CControllerState copied;
        std::memcpy(&copied, &source, sizeof(CControllerState));
        Check(std::memcmp(&copied, &source, sizeof(CControllerState)) == 0, "raw copy preserves the complete representation");
        for (const auto& field : fields)
            Check(copied.*(field.member) == field.value, std::string(field.name) + " survives existing raw-byte copying");
    }
}

int main()
{
    try
    {
        TestZeroInitialization();
        TestValuesAndCopy();
        TestCurrentAndPreviousSnapshots();
        TestRawCopyContract();
        std::cout << "Passed " << checks << " reused MTA controller-state checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "MTA controller-state checks failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
